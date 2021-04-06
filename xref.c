// cc -o xref xref.c -Wall -O3
#include <errno.h>
#include <fcntl.h>              // open
#include <stdbool.h>
#include <stdlib.h>             // strtoull
#include <stdint.h>
#include <stdio.h>              // fprintf, stderr
#include <string.h>             // strerror
#include <unistd.h>             // close
#include <sys/mman.h>           // mmap, munmap, MAP_FAILED, PROT_READ
#include <sys/stat.h>           // fstat

#define SWAP32(x) (((x & 0xff000000) >> 24) | ((x & 0xff0000) >> 8) | ((x & 0xff00) << 8) | ((x & 0xff) << 24))

#define FAT_CIGAM       0xbebafeca
#define MH_MAGIC_64     0xfeedfacf
#define LC_SEGMENT_64   0x19
#define CPU_TYPE_ARM64  0x0100000c

typedef uint32_t vm_prot_t;

typedef struct
{
    uint32_t magic;
    uint32_t nfat_arch;
} fat_hdr_t;

typedef struct
{
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t offset;
    uint32_t size;
    uint32_t align;
} fat_arch_t;

typedef struct
{
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} mach_hdr_t;

typedef struct
{
    uint32_t cmd;
    uint32_t cmdsize;
} mach_lc_t;

typedef struct
{
    uint32_t  cmd;
    uint32_t  cmdsize;
    char      segname[16];
    uint64_t  vmaddr;
    uint64_t  vmsize;
    uint64_t  fileoff;
    uint64_t  filesize;
    vm_prot_t maxprot;
    vm_prot_t initprot;
    uint32_t  nsects;
    uint32_t  flags;
} mach_seg_t;

int main(int argc, const char **argv)
{
    int retval = -1;
    int fd = -1;
    void *mem = MAP_FAILED;
    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s file addr\n", argv[0]);
        goto out;
    }

    char *str = NULL;
    const unsigned long long search = strtoull(argv[2], &str, 16);
    if(argv[2][0] == '\0' || str[0] != '\0')
    {
        fprintf(stderr, "Bad address.\n");
        goto out;
    }

    fd = open(argv[1], O_RDONLY);
    if(fd == -1)
    {
        fprintf(stderr, "open: %s\n", strerror(errno));
        goto out;
    }

    struct stat s;
    if(fstat(fd, &s) != 0)
    {
        fprintf(stderr, "fstat: %s\n", strerror(errno));
        goto out;
    }

    if(sizeof(mach_hdr_t) > s.st_size)
    {
        fprintf(stderr, "File too short to contain Mach-O header.\n");
        goto out;
    }

    mem = mmap(NULL, s.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    if(mem == MAP_FAILED)
    {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
        goto out;
    }
    uint64_t filesize = s.st_size;

    mach_hdr_t *hdr = mem;
    fat_hdr_t *fat = mem;
    if(fat->magic == FAT_CIGAM)
    {
        if(sizeof(fat_arch_t) * SWAP32(fat->nfat_arch) > filesize - sizeof(mach_hdr_t))
        {
            fprintf(stderr, "File too short to contain fat header.\n");
            goto out;
        }
        bool found = false;
        fat_arch_t *arch = (fat_arch_t*)(fat + 1);
        for(size_t i = 0; i < SWAP32(fat->nfat_arch); ++i)
        {
            if(SWAP32(arch[i].cputype) == CPU_TYPE_ARM64)
            {
                uint32_t offset = SWAP32(arch[i].offset);
                uint32_t newsize = SWAP32(arch[i].size);
                if(offset > filesize || newsize > filesize - offset)
                {
                    fprintf(stderr, "Fat arch out of bounds.\n");
                    goto out;
                }
                if(newsize < sizeof(mach_hdr_t))
                {
                    fprintf(stderr, "Fat arch is too short to contain a Mach-O.\n");
                    goto out;
                }
                hdr = (mach_hdr_t*)((uintptr_t)hdr + offset);
                filesize = newsize;
                found = true;
                break;
            }
        }
        if(!found)
        {
            fprintf(stderr, "No arm64 slice in fat binary.\n");
            goto out;
        }
    }

    if(hdr->magic != MH_MAGIC_64)
    {
        fprintf(stderr, "Not a 64-bit Mach-O.\n");
        goto out;
    }
    if(hdr->cputype != CPU_TYPE_ARM64)
    {
        fprintf(stderr, "Not an arm64 binary.\n");
        goto out;
    }
    if(hdr->sizeofcmds > filesize - sizeof(*hdr))
    {
        fprintf(stderr, "File too small for Mach-O load commands.\n");
        goto out;
    }

    for(mach_lc_t *lc = (mach_lc_t*)(hdr + 1), *end = (mach_lc_t*)((uintptr_t)lc + hdr->sizeofcmds); lc < end; lc = (mach_lc_t*)((uintptr_t)lc + lc->cmdsize))
    {
        size_t space = (uintptr_t)end - (uintptr_t)lc;
        if(sizeof(*lc) > space || lc->cmdsize > space)
        {
            fprintf(stderr, "File too small for Mach-O load command.\n");
            goto out;
        }
        if(lc->cmd == LC_SEGMENT_64)
        {
            mach_seg_t *seg = (mach_seg_t*)lc;
            if(seg->fileoff > filesize || seg->filesize > filesize - seg->fileoff)
            {
                fprintf(stderr, "Mach-O segment out of bounds.\n");
                goto out;
            }
            uint64_t addr = seg->vmaddr;
            for(uint32_t *p = (uint32_t*)((uint8_t*)mem + seg->fileoff), *e = p + (seg->filesize / 4); p < e; ++p, addr += 4)
            {
                uint32_t v = *p;
                if((v & 0x1f000000) == 0x10000000) // adr and adrp
                {
                    uint32_t reg = v & 0x1f;
                    bool is_adrp = (v & 0x80000000) != 0;
                    int64_t base = is_adrp ? (addr & 0xfffffffffffff000) : addr;
                    int64_t off  = ((int64_t)((((v >> 5) & 0x7ffff) << 2) | ((v >> 29) & 0x3)) << 43) >> (is_adrp ? 31 : 43);
                    uint64_t target = base + off;
                    if(target == search)
                    {
                        printf("%#llx: %s x%u, %#llx\n", addr, is_adrp ? "adrp" : "adr", reg, target);
                    }
                    // More complicated cases - up to 3 instr
                    else
                    {
                        uint32_t *q = p + 1;
                        while(q < e && *q == 0xd503201f) // nop
                        {
                            ++q;
                        }
                        if(q < e)
                        {
                            v = *q;
                            uint32_t reg2 = reg;
                            uint32_t aoff = 0;
                            bool found = false;
                            if((v & 0xff8003e0) == (0x91000000 | (reg << 5))) // 64bit add, match reg
                            {
                                reg2 = v & 0x1f;
                                aoff = (v >> 10) & 0xfff;
                                if(v & 0x400000) aoff <<= 12;
                                if(target + aoff == search)
                                {
                                    printf("%#llx: %s x%u, %#llx; add x%u, x%u, %#x\n", addr, is_adrp ? "adrp" : "adr", reg, target, reg2, reg, aoff);
                                    found = true;
                                }
                                else
                                {
                                    do
                                    {
                                        ++q;
                                    } while(q < e && *q == 0xd503201f); // nop
                                }
                            }
                            if(!found && q < e)
                            {
                                v = *q;
                                if((v & 0xff8003e0) == (0x91000000 | (reg2 << 5))) // 64bit add, match reg
                                {
                                    uint32_t xoff = (v >> 10) & 0xfff;
                                    if(v & 0x400000) xoff <<= 12;
                                    if(target + aoff + xoff == search)
                                    {
                                        // If we get here, we know the previous add matched
                                        printf("%#llx: %s x%u, %#llx; add x%u, x%u, %#x; add x%u, x%u, %#x\n", addr, is_adrp ? "adrp" : "adr", reg, target, reg2, reg, aoff, v & 0x1f, reg2, xoff);
                                    }
                                }
                                else if((v & 0x3e0003e0) == (0x38000000 | (reg2 << 5))) // all of str[hb]/ldr[hb], match reg
                                {
                                    const char *inst = NULL;
                                    uint8_t size;
                                    size = (v >> 30) & 0x3;
                                    uint8_t opc = (v >> 22) & 0x3;
                                    switch((opc << 4) | size)
                                    {
                                        case 0x00:            inst = "strb";  break;
                                        case 0x01:            inst = "strh";  break;
                                        case 0x02: case 0x03: inst = "str";   break;
                                        case 0x10:            inst = "ldrb";  break;
                                        case 0x11:            inst = "ldrh";  break;
                                        case 0x12: case 0x13: inst = "ldr";   break;
                                        case 0x20: case 0x30: inst = "ldrsb"; break;
                                        case 0x21: case 0x31: inst = "ldrsh"; break;
                                        case 0x22:            inst = "ldrsw"; break;
                                    }
                                    if(inst)
                                    {
                                        uint8_t regsize = opc == 2 && size < 2 ? 3 : size;
                                        const char *rs = regsize == 3 ? "x" : "w";
                                        if((v & 0x1000000) != 0) // unsigned offset
                                        {
                                            uint64_t uoff = ((v >> 10) & 0xfff) << size;
                                            if(target + aoff + uoff == search)
                                            {
                                                if(aoff) // Have add
                                                {
                                                    printf("%#llx: %s x%u, %#llx; add x%u, x%u, %#x; %s %s%u, [x%u, %#llx]\n", addr, is_adrp ? "adrp" : "adr", reg, target, reg2, reg, aoff, inst, rs, v & 0x1f, reg2, uoff);
                                                }
                                                else // Have no add
                                                {
                                                    printf("%#llx: %s x%u, %#llx; %s %s%u, [x%u, %#llx]\n", addr, is_adrp ? "adrp" : "adr", reg, target, inst, rs, v & 0x1f, reg2, uoff);
                                                }
                                            }
                                        }
                                        else if((v & 0x00200000) == 0)
                                        {
                                            int64_t soff = ((int64_t)((v >> 12) & 0x1ff) << 55) >> 55;
                                            const char *sign = soff < 0 ? "-" : "";
                                            if(target + aoff + soff == search)
                                            {
                                                if((v & 0x400) == 0)
                                                {
                                                    if((v & 0x800) == 0) // unscaled
                                                    {
                                                        switch((opc << 4) | size)
                                                        {
                                                            case 0x00:            inst = "sturb";  break;
                                                            case 0x01:            inst = "sturh";  break;
                                                            case 0x02: case 0x03: inst = "stur";   break;
                                                            case 0x10:            inst = "ldurb";  break;
                                                            case 0x11:            inst = "ldurh";  break;
                                                            case 0x12: case 0x13: inst = "ldur";   break;
                                                            case 0x20: case 0x30: inst = "ldursb"; break;
                                                            case 0x21: case 0x31: inst = "ldursh"; break;
                                                            case 0x22:            inst = "ldursw"; break;
                                                        }
                                                    }
                                                    else // unprivileged
                                                    {
                                                        switch((opc << 4) | size)
                                                        {
                                                            case 0x00:            inst = "sttrb";  break;
                                                            case 0x01:            inst = "sttrh";  break;
                                                            case 0x02: case 0x03: inst = "sttr";   break;
                                                            case 0x10:            inst = "ldtrb";  break;
                                                            case 0x11:            inst = "ldtrh";  break;
                                                            case 0x12: case 0x13: inst = "ldtr";   break;
                                                            case 0x20: case 0x30: inst = "ldtrsb"; break;
                                                            case 0x21: case 0x31: inst = "ldtrsh"; break;
                                                            case 0x22:            inst = "ldtrsw"; break;
                                                        }
                                                    }
                                                    if(aoff) // Have add
                                                    {
                                                        printf("%#llx: %s x%u, %#llx; add x%u, x%u, %#x; %s %s%u, [x%u, %s%#llx]\n", addr, is_adrp ? "adrp" : "adr", reg, target, reg2, reg, aoff, inst, rs, v & 0x1f, reg2, sign, soff);
                                                    }
                                                    else // Have no add
                                                    {
                                                        printf("%#llx: %s x%u, %#llx; %s %s%u, [x%u, %s%#llx]\n", addr, is_adrp ? "adrp" : "adr", reg, target, inst, rs, v & 0x1f, reg2, sign, soff);
                                                    }
                                                }
                                                else // pre/post-index
                                                {
                                                    if((v & 0x800) != 0) // pre
                                                    {
                                                        if(aoff) // Have add
                                                        {
                                                            printf("%#llx: %s x%u, %#llx; add x%u, x%u, %#x; %s %s%u, [x%u, %s%#llx]!\n", addr, is_adrp ? "adrp" : "adr", reg, target, reg2, reg, aoff, inst, rs, v & 0x1f, reg2, sign, soff);
                                                        }
                                                        else // Have no add
                                                        {
                                                            printf("%#llx: %s x%u, %#llx; %s %s%u, [x%u, %s%#llx]!\n", addr, is_adrp ? "adrp" : "adr", reg, target, inst, rs, v & 0x1f, reg2, sign, soff);
                                                        }
                                                    }
                                                    else // post
                                                    {
                                                        if(aoff) // Have add
                                                        {
                                                            printf("%#llx: %s x%u, %#llx; add x%u, x%u, %#x; %s %s%u, [x%u], %s%#llx\n", addr, is_adrp ? "adrp" : "adr", reg, target, reg2, reg, aoff, inst, rs, v & 0x1f, reg2, sign, soff);
                                                        }
                                                        else // Have no add
                                                        {
                                                            printf("%#llx: %s x%u, %#llx; %s %s%u, [x%u], %s%#llx\n", addr, is_adrp ? "adrp" : "adr", reg, target, inst, rs, v & 0x1f, reg2, sign, soff);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                // TODO: pairs, SIMD ldr/str, atomics
                            }
                        }
                    }
                }
                else if((v & 0xbf000000) == 0x18000000 || (v & 0xff000000) == 0x98000000) // ldr and ldrsw literal
                {
                    int64_t off = ((int64_t)((v >> 5) & 0x7ffff) << 45) >> 43;
                    if(addr + off == search)
                    {
                        uint32_t reg  = v & 0x1f;
                        bool is_ldrsw = (v & 0xff000000) == 0x98000000;
                        bool is_64bit = (v & 0x40000000) != 0 && is_ldrsw;
                        printf("%#llx: %s %s%u, %#llx\n", addr, is_ldrsw ? "ldrsw" : "ldr", is_64bit ? "x" : "w", reg, search);
                    }
                }
                else if((v & 0x7c000000) == 0x14000000) // b and bl
                {
                    int64_t off = ((int64_t)(v & 0x3ffffff) << 38) >> 36;
                    if(addr + off == search)
                    {
                        bool is_bl = (v & 0x80000000) != 0;
                        printf("%#llx: %s %#llx\n", addr, is_bl ? "bl" : "b", search);
                    }
                }
                else if((v & 0xff000010) == 0x54000000) // b.cond
                {
                    int64_t off = ((int64_t)((v >> 5) & 0x7ffff) << 45) >> 43;
                    if(addr + off == search)
                    {
                        const char *cond;
                        switch(v & 0xf)
                        {
                            case 0x0: cond = "eq"; break;
                            case 0x1: cond = "ne"; break;
                            case 0x2: cond = "hs"; break;
                            case 0x3: cond = "lo"; break;
                            case 0x4: cond = "mi"; break;
                            case 0x5: cond = "pl"; break;
                            case 0x6: cond = "vs"; break;
                            case 0x7: cond = "vc"; break;
                            case 0x8: cond = "hi"; break;
                            case 0x9: cond = "ls"; break;
                            case 0xa: cond = "ge"; break;
                            case 0xb: cond = "lt"; break;
                            case 0xc: cond = "gt"; break;
                            case 0xd: cond = "le"; break;
                            case 0xe: cond = "al"; break;
                            case 0xf: cond = "nv"; break;
                        }
                        printf("%#llx: b.%s %#llx\n", addr, cond, search);
                    }
                }
                else if((v & 0x7e000000) == 0x34000000) // cbz and cbnz
                {
                    int64_t off = ((int64_t)((v >> 5) & 0x7ffff) << 45) >> 43;
                    if(addr + off == search)
                    {
                        uint32_t reg  = v & 0x1f;
                        bool is_64bit = (v & 0x80000000) != 0;
                        bool is_nz    = (v & 0x01000000) != 0;
                        printf("%#llx: %s %s%u, %#llx\n", addr, is_nz ? "cbnz" : "cbz", is_64bit ? "x" : "w", reg, search);
                    }
                }
                else if((v & 0x7e000000) == 0x36000000) // tbz and tbnz
                {
                    int64_t off = ((int64_t)((v >> 5) & 0x3fff) << 50) >> 48;
                    if(addr + off == search)
                    {
                        uint32_t reg  = v & 0x1f;
                        uint32_t bit  = ((v >> 19) & 0x1f) | ((v >> 26) & 0x20);
                        bool is_64bit = bit > 31;
                        bool is_nz    = (v & 0x01000000) != 0;
                        printf("%#llx: %s %s%u, %u, %#llx\n", addr, is_nz ? "tbnz" : "tbz", is_64bit ? "x" : "w", reg, bit, search);
                    }
                }
            }
        }
    }

    retval = 0;
out:;
    if(mem != MAP_FAILED) munmap(mem, s.st_size);
    if(fd != -1) close(fd);
    return retval;
}

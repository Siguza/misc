// gcc -o vmacho vmacho.c -Wall -O3
#include <errno.h>
#include <fcntl.h>              // open
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>              // fprintf, stderr
#include <stdlib.h>             // malloc, free
#include <string.h>             // strerror
#include <strings.h>            // bzero
#include <unistd.h>             // write, close
#include <sys/stat.h>           // fstat
#include <sys/mman.h>           // mmap, munmap
#include <mach/mach.h>
#include <mach-o/loader.h>

#define LOG(str, args...) do { fprintf(stderr, str "\n", ##args); } while(0)

typedef struct mach_header        mach_hdr32_t;
typedef struct mach_header_64     mach_hdr64_t;
typedef struct load_command       mach_lc_t;
typedef struct segment_command    mach_seg32_t;
typedef struct segment_command_64 mach_seg64_t;

int main(int argc, const char **argv)
{
    int retval = -1,
        infd   = -1,
        outfd  = -1,
        outc   = -1,
        oflags = O_WRONLY | O_TRUNC | O_CREAT | O_EXCL;
    void *file = MAP_FAILED,
         *mem  = NULL;
    size_t flen = 0,
           mlen = 0;

    int aoff = 1;
    for(int i=aoff; i < argc; i++)
    {
        if(strcmp(argv[aoff], "-f") == 0)
        {
            oflags &= ~O_EXCL;
            ++aoff;
        }
        else if(strcmp(argv[aoff], "-c") == 0)
        {
            outc = 1;
            ++aoff;
        }
    }
    if(argc - aoff != 2)
    {
        fprintf(stderr, "Usage: %s [-f] in out\n"
                        "    -f  Force (overwrite existing files)\n"
                        "    -c  Output as c array\n"
                        , argv[0]);
        goto out;
    }
    infd = open(argv[aoff], O_RDONLY);
    if(infd == -1)
    {
        LOG("open(%s): %s", argv[aoff], strerror(errno));
        goto out;
    }
    struct stat s;
    if(fstat(infd, &s) != 0)
    {
        LOG("fstat: %s", strerror(errno));
        goto out;
    }
    flen = s.st_size;
    if(flen < sizeof(uint32_t))
    {
        LOG("File too short for magic.");
        goto out;
    }
    file = mmap(NULL, flen, PROT_READ, MAP_FILE | MAP_PRIVATE, infd, 0);
    if(file == MAP_FAILED)
    {
        LOG("mmap: %s", strerror(errno));
        goto out;
    }
    uintptr_t ufile = (uintptr_t)file;
    uint32_t magic = *(uint32_t*)file;
    mach_lc_t *lcs = NULL;
    uint32_t sizeofcmds = 0;
    if(magic == MH_MAGIC)
    {
        mach_hdr32_t *hdr = file;
        if(flen < sizeof(*hdr) + hdr->sizeofcmds)
        {
            LOG("File too short for load commands.");
            goto out;
        }
        lcs = (mach_lc_t*)(hdr + 1);
        sizeofcmds = hdr->sizeofcmds;
    }
    else if(magic == MH_MAGIC_64)
    {
        mach_hdr64_t *hdr = file;
        if(flen < sizeof(*hdr) + hdr->sizeofcmds)
        {
            LOG("File too short for load commands.");
            goto out;
        }
        lcs = (mach_lc_t*)(hdr + 1);
        sizeofcmds = hdr->sizeofcmds;
    }
    else
    {
        LOG("Bad magic: %08x", magic);
        goto out;
    }

    uint64_t lowest  = ~0,
             highest =  0;
    for(mach_lc_t *cmd = lcs, *end = (mach_lc_t*)((uintptr_t)cmd + sizeofcmds); cmd < end; cmd = (mach_lc_t*)((uintptr_t)cmd + cmd->cmdsize))
    {
        if((uintptr_t)cmd + sizeof(*cmd) > (uintptr_t)end || (uintptr_t)cmd + cmd->cmdsize > (uintptr_t)end || (uintptr_t)cmd + cmd->cmdsize < (uintptr_t)cmd)
        {
            LOG("Bad LC: 0x%lx", (uintptr_t)cmd - ufile);
            goto out;
        }
        uint64_t vmaddr   = 0,
                 vmsize   = 0,
                 fileoff  = 0,
                 filesize = 0;
        vm_prot_t prot = 0;
        if(cmd->cmd == LC_SEGMENT)
        {
            mach_seg32_t *seg = (mach_seg32_t*)cmd;
            vmaddr   = seg->vmaddr;
            vmsize   = seg->vmsize;
            fileoff  = seg->fileoff;
            filesize = seg->filesize;
            prot = seg->maxprot;
        }
        else if(cmd->cmd == LC_SEGMENT_64)
        {
            mach_seg64_t *seg = (mach_seg64_t*)cmd;
            vmaddr   = seg->vmaddr;
            vmsize   = seg->vmsize;
            fileoff  = seg->fileoff;
            filesize = seg->filesize;
            prot = seg->maxprot;
        }
        else
        {
            continue;
        }
        uint64_t off = fileoff + filesize;
        if(off > flen || off < fileoff)
        {
            LOG("Bad segment: 0x%lx", (uintptr_t)cmd - ufile);
            goto out;
        }
        if((prot & VM_PROT_ALL) != 0)
        {
            uintptr_t start = vmaddr;
            if(start < lowest)
            {
                lowest = start;
            }
            uintptr_t end = start + vmsize;
            if(end > highest)
            {
                highest = end;
            }
        }
    }
    if(highest < lowest)
    {
        LOG("Bad memory layout, lowest: 0x%llx, highest: 0x%llx", lowest, highest);
        goto out;
    }
    mlen = highest - lowest;
    mem = malloc(mlen);
    if(!mem)
    {
        LOG("malloc: %s", strerror(errno));
        goto out;
    }
    bzero(mem, mlen);
    for(mach_lc_t *cmd = lcs, *end = (mach_lc_t*)((uintptr_t)cmd + sizeofcmds); cmd < end; cmd = (mach_lc_t*)((uintptr_t)cmd + cmd->cmdsize))
    {
        uint64_t vmaddr   = 0,
                 vmsize   = 0,
                 fileoff  = 0,
                 filesize = 0;
        if(cmd->cmd == LC_SEGMENT)
        {
            mach_seg32_t *seg = (mach_seg32_t*)cmd;
            vmaddr   = seg->vmaddr;
            vmsize   = seg->vmsize;
            fileoff  = seg->fileoff;
            filesize = seg->filesize;
        }
        else if(cmd->cmd == LC_SEGMENT_64)
        {
            mach_seg64_t *seg = (mach_seg64_t*)cmd;
            vmaddr   = seg->vmaddr;
            vmsize   = seg->vmsize;
            fileoff  = seg->fileoff;
            filesize = seg->filesize;
        }
        else
        {
            continue;
        }
        size_t size = filesize < vmsize ? filesize : vmsize;
        memcpy((void*)((uintptr_t)mem + (vmaddr - lowest)), (void*)(ufile + fileoff), size);
    }

    if(strcmp(argv[aoff + 1], "-") == 0)
    {
        outfd = STDOUT_FILENO;
    }
    else
    {
        outfd = open(argv[aoff + 1], oflags, 0666);
        if(outfd == -1)
        {
            LOG("open(%s): %s", argv[aoff + 1], strerror(errno));
            goto out;
        }
    }

    if(outc == -1)
    {
        for(size_t off = 0; off < mlen; )
        {
            ssize_t w = write(outfd, (void*)((uintptr_t)mem + off), mlen - off);
            if(w == -1)
            {
                LOG("write: %s", strerror(errno));
                goto out;
            }
            off += w;
        }
    }
    else
    {
        for(uint8_t *ptr=(uint8_t *)mem; ptr != mem + mlen; ptr++)
        {
            int r = -1,
                w = ptr - (uint8_t *)mem;

            if (w % 12 == 0)
            {
                if (w != 0)
                {
                    r = dprintf(outfd, ",\n");
                    if(r < 0) goto dprintf_error;
                }

                r = dprintf(outfd, "  ");
                if(r < 0) goto dprintf_error;
            }
            else
            {
                r = dprintf(outfd, ", ");
                if(r < 0) goto dprintf_error;
            }

            r = dprintf(outfd, "0x%02x", *ptr);
            if(r < 0) goto dprintf_error;
        }
    }

    LOG("Done, base address: 0x%llx", lowest);
    retval = 0;

out:;
    if(outfd != -1) close(outfd);
    if(mem) free(mem);
    if(file != MAP_FAILED) munmap(file, flen);
    if(infd != -1) close(infd);
    return retval;

dprintf_error:;
    LOG("dprintf: %s", strerror(errno));
    goto out;
}

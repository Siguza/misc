#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, const char **argv)
{
    int aoff = 1;
    const char *odir = ".";
    bool list = false;
    for(; aoff < argc; ++aoff)
    {
        if(argv[aoff][0] != '-')
        {
            break;
        }
        if(argv[aoff][1] == '-' && argv[aoff][2] == '\0')
        {
            ++aoff;
            break;
        }

        if(strcmp(argv[aoff], "-l") == 0)
        {
            list = true;
        }
        else if(strcmp(argv[aoff], "-o") == 0)
        {
            if(++aoff >= argc)
            {
                fprintf(stderr, "-o needs an argument\n");
                return -1;
            }
            odir = argv[aoff];
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n", argv[aoff]);
            return -1;
        }
    }
    if(aoff + 1 != argc)
    {
        fprintf(stderr, "Usage: %s [-l|-o dir] [file]\n", argv[0]);
        return -1;
    }

    int retval = -1;
    int fd = -1;
    int od = -1;
    struct stat s;
    void *mem = MAP_FAILED;

    fd = open(argv[aoff], O_RDONLY);
    if(fd == -1)
    {
        fprintf(stderr, "open(%s): %s\n", argv[aoff], strerror(errno));
        goto out;
    }
    if(fstat(fd, &s) != 0)
    {
        fprintf(stderr, "fstat: %s\n", strerror(errno));
        goto out;
    }
    mem = mmap(NULL, s.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    if(mem == MAP_FAILED)
    {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
        goto out;
    }

    struct
    {
        uint32_t idk[8];
        char magic[8];
        uint32_t num;
        uint32_t zero;
        struct
        {
            char name[4];
            uint32_t off;
            uint32_t len;
            uint32_t zero;
        } ftab[];
    } *hdr = mem;
    if(s.st_size < 0x20)
    {
        fprintf(stderr, "File too small for header\n");
        goto out;
    }
    if(memcmp(hdr->magic, "rkosftab", 8) != 0)
    {
        fprintf(stderr, "Bad magic\n");
        goto out;
    }
    if(hdr->zero != 0)
    {
        fprintf(stderr, "hdr->zero != 0\n");
        goto out;
    }
    uint32_t num = hdr->num;
    if((uintptr_t)(&hdr->ftab[num]) - (uintptr_t)hdr > s.st_size)
    {
        fprintf(stderr, "File too small for ftab\n");
        goto out;
    }

    if(!list)
    {
        od = open(odir, O_RDONLY | O_DIRECTORY);
        if(od == -1)
        {
            fprintf(stderr, "open(%s): %s\n", odir, strerror(errno));
            goto out;
        }
    }

    for(uint32_t i = 0; i < num; ++i)
    {
        if(hdr->ftab[i].zero != 0)
        {
            fprintf(stderr, "ftab[%u].zero != 0\n", i);
            goto out;
        }
        uint32_t off = hdr->ftab[i].off;
        uint32_t len = hdr->ftab[i].len;
        uint32_t end;
        if(__builtin_add_overflow(off, len, &end))
        {
            fprintf(stderr, "ftab[%u] off+len overflows\n", i);
            goto out;
        }
        if((uintptr_t)hdr + off < (uintptr_t)&hdr->ftab[num])
        {
            fprintf(stderr, "ftab[%u] overlaps header\n", i);
            if(!list)
            {
                goto out;
            }
        }
        if(end > s.st_size)
        {
            fprintf(stderr, "ftab[%u] exceeds length of file\n", i);
            if(!list)
            {
                goto out;
            }
        }

        if(list)
        {
            printf("0x%08x-0x%08x %.4s\n", off, end, hdr->ftab[i].name);
        }
        else
        {
            char name[5];
            memcpy(name, hdr->ftab[i].name, 4);
            name[4] = '\0';
            int ofd = openat(od, name, O_WRONLY | O_CREAT | O_EXCL, 0644);
            if(ofd == -1)
            {
                fprintf(stderr, "open(%s/%s): %s\n", odir, name, strerror(errno));
                goto out;
            }
            ssize_t s = write(ofd, (const void*)((uintptr_t)hdr + off), len);
            close(ofd);
            if(s != len)
            {
                fprintf(stderr, "write(%s/%s): %s\n", odir, name, strerror(errno));
                goto out;
            }
        }
    }

    retval = 0;
out:;
    if(od != -1) close(fd);
    if(mem != MAP_FAILED) munmap(mem, s.st_size);
    if(fd != -1) close(fd);

    return retval;
}

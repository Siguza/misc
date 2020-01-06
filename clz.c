// cc -o clz clz.c -Wall -O3
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char **argv)
{
    int off = 1;
    bool big = false;
    for(; off < argc; ++off)
    {
        if(argv[off][0] != '-' || (argv[off][1] >= '0' && argv[off][1] <= '9')) break;
        if(strcmp(argv[off], "-l") == 0) big = true;
        else
        {
            fprintf(stderr, "[!] Invalid argument: %s\n", argv[off]);
            return 1;
        }
    }
    if(argc - off < 1)
    {
        fprintf(stderr, "Usage:\n"
                        "    %s [-l] number\n"
                        , argv[0]);
        return 1;
    }
    unsigned long long l = strtoull(argv[off], NULL, 0);
    int result;
    if(big)
    {
        result = __builtin_clzll(l);
    }
    else
    {
        if(l > UINT_MAX)
        {
            fprintf(stderr, "[!] Number too big, use -l.\n");
            return 1;
        }
        result = __builtin_clz((unsigned int)l);
    }
    printf("%u\n", result);
    return 0;
}

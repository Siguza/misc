// gcc -o rand rand.c -Wall -O3
#include <stdbool.h>            // bool, true, false
#include <stdint.h>             // uint32_t, UINT32_MAX
#include <stdio.h>              // stderr, fprintf, printf, putchar
#include <stdlib.h>             // arc4random_uniform
#include <string.h>             // strcmp

int main(int argc, const char **argv)
{
    uint32_t max = UINT32_MAX;
    bool string = false;
    int off = 1;

    if(argc > off && strcmp(argv[off], "-s") == 0)
    {
        string = true;
        max = 32;
        ++off;
    }
    if(argc > off)
    {
        char *end;
        long long l = strtoll(argv[off], &end, 0);
        if(argv[off][0] == '\0' || end[0] != '\0')
        {
            fprintf(stderr, "Invalid input: %s\n", argv[off]);
            return 1;
        }
        else if(l > UINT32_MAX)
        {
            fprintf(stderr, "Too large: %s\n", argv[off]);
            return 1;
        }
        max = l;
        ++off;
    }

    if(string)
    {
        for(uint32_t i = 0; i < max; ++i)
        {
            uint32_t c = arc4random_uniform(62);
            putchar(c + (c < 10 ? '0' : c < 36 ? ('A' - 10) : ('a' - 36)));
        }
        putchar('\n');
    }
    else
    {
        uint32_t min = 0;
        if(argc > off)
        {
            char *end;
            long long l = strtoll(argv[off], &end, 0);
            if(argv[off][0] == '\0' || end[0] != '\0')
            {
                fprintf(stderr, "Invalid input: %s\n", argv[off]);
                return 1;
            }
            else if(l > UINT32_MAX)
            {
                fprintf(stderr, "Too large: %s\n", argv[off]);
                return 1;
            }
            min = l;
        }
        printf("%u\n", arc4random_uniform(max - min) + min);
    }

    return 0;
}

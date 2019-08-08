// gcc -o bindump bindump.c -Wall -O3
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char **argv)
{
    if(argc < 2)
    {
        fputs("[!] Too few arguments.\n", stderr);
        return 1;
    }
    char *end = NULL;
    uint64_t num = strtoull(argv[1], &end, 0);
    if(*end != '\0')
    {
        fprintf(stderr, "[!] Error at %s\n", end);
        return 1;
    }
    for(size_t i = 1; i <= 64; ++i)
    {
        putchar('0' + ((num >> (64 - i)) & 1));
    }
    putchar('\n');
    return 0;
}

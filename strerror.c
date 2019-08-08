// gcc -o strerror strerror.c -Wall -O3
// gcc -o strerror strerror.c -Wall -O3 -framework CoreFoundation -framework Security
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#   include <mach/mach.h>
#   include <CoreFoundation/CoreFoundation.h>
#   include <Security/Security.h>
#endif

#ifdef __APPLE__
typedef enum
{
    kUnix,
    kMach,
    kSec,
} what_t;
#endif

int main(int argc, const char **argv)
{
    int off = 1;
#ifdef __APPLE__
    CFStringRef str = NULL;
    char buf[512];
    buf[0] = '\0';
    what_t mode = kUnix;
#endif
    for(; off < argc; ++off)
    {
        if(argv[off][0] != '-' || (argv[off][0] >= '0' && argv[off][0] <= '9')) break;
#ifdef __APPLE__
        if(strcmp(argv[off], "-m") == 0) mode = kMach;
        else if(strcmp(argv[off], "-s") == 0) mode = kSec;
#endif
        else
        {
            fprintf(stderr, "[!] Invalid argument: %s\n", argv[off]);
            return 1;
        }
    }
    if(argc - off < 1)
    {
        fprintf(stderr, "Usage:\n"
                        "    %s [-m|-s] number\n"
                        , argv[0]);
        return 1;
    }
    int i = (int)strtoul(argv[off], NULL, 0);
    const char *s =
#ifdef __APPLE__
    mode == kSec ? ((str = SecCopyErrorMessageString(i, NULL)), !str ? "(null)" : (CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8), buf)) :
    mode == kMach ? mach_error_string(i) :
#endif
    strerror(i);
    printf("%s\n", s);
    return 0;
}

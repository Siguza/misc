// cc -o strerror strerror.c -Wall -O3
// cc -o strerror strerror.c -Wall -O3 -framework CoreFoundation -framework Security
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#ifdef __APPLE__
#   include <mach/mach.h>
#   include <CoreFoundation/CoreFoundation.h>
#   include <Security/Security.h>
char *xpc_strerror(int);
#endif

#ifdef __APPLE__
typedef enum
{
    kUnix,
    kMach,
    kSec,
    kXpc,
} what_t;
#endif

int main(int argc, char **argv)
{
    int off = 1;
#ifdef __APPLE__
    CFStringRef str = NULL;
    char buf[512];
    buf[0] = '\0';
    what_t mode = kUnix;
#endif

    int ch;
#ifdef __APPLE__
    while ((ch = getopt(argc, argv, "msux")) != -1)
#else
    while ((ch = getopt(argc, argv, "u")) != -1)
#endif
    {
        switch (ch)
        {
            case 'm':
                mode = kMach;
                break;
            case 's':
                mode = kSec;
                break;
            case 'x':
                mode = kXpc;
                break;
            case 'u':
                mode = kUnix;
                break;
            case '?':
            default:
                fprintf(stderr, "[!] Invalid argument: %s\n", argv[off]);
                return EXIT_FAILURE;
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0) {
        fprintf(stderr, "usage: strerror [-msux] [errno]\n");
        return EXIT_FAILURE;
    }

    int i = (int)strtoul(argv[0], NULL, 0);

    char *s = NULL;
    switch (mode)
    {
#ifdef __APPLE__
        case kSec:
            str = SecCopyErrorMessageString(i, NULL);
            if (str) {
                CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8);
                s = buf;
            } else
                s = "(null)";
            break;
        case kXpc:
            s = xpc_strerror(i);
            break;
        case kMach:
            s = mach_error_string(i);
            break;
#endif
        case kUnix:
        default:
            s = strerror(i);
            break;
    }
    printf("%s\n", s);
    return 0;
}

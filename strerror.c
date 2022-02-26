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

typedef enum
{
    kUnix,
#ifdef __APPLE__
    kMach,
    kSec,
    kXpc,
#endif
} what_t;

int main(int argc, char **argv)
{
#ifdef __APPLE__
    CFStringRef str = NULL;
    char buf[512];
    buf[0] = '\0';
#endif

    what_t mode = kUnix;

#ifdef __APPLE__
    int ch;
    while((ch = getopt(argc, argv, "msux")) != -1)
    {
        switch(ch)
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
                fprintf(stderr, "[!] Invalid argument: -%c\n", ch);
                return EXIT_FAILURE;
        }
    }
#endif
    argc -= optind;
    argv += optind;

    if(argc == 0)
    {
#ifdef __APPLE__
        fprintf(stderr, "Usage: strerror [-msux] <errno>\n");
#else
        fprintf(stderr, "Usage: strerror <errno>\n");
#endif
        return EXIT_FAILURE;
    }

    int i = (int)strtoul(argv[0], NULL, 0);

    char *s = NULL;
    switch(mode)
    {
#ifdef __APPLE__
        case kSec:
            str = SecCopyErrorMessageString(i, NULL);
            if(str)
            {
                CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8);
                s = buf;
            }
            else
            {
                s = "(null)";
            }
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

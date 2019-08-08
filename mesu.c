// gcc -o mesu mesu.c -Wall -O3 -framework CoreFoundation
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#define ASSERT(obj, badop, str, args...) \
if(!(obj)) \
{ \
    fprintf(stderr, "\x1b[1;91mError: " str #obj "\x1b[0m\n", ##args); \
    badop; \
} do {} while(0)

int main(int argc, const char **argv)
{
    bool complete = false;
    int aoff;
    for(aoff = 1; aoff < argc; ++aoff)
    {
        if(argv[aoff][0] != '-') break;
        if(strcmp(argv[aoff], "--") == 0)
        {
            ++aoff;
            break;
        }
        if(strcmp(argv[aoff], "-c") == 0) complete = true;
        else
        {
            fprintf(stderr, "Bad arg.\n");
            return -1;
        }
    }
    FILE *f = aoff >= argc ? stdin : fopen(argv[aoff], "r");
    if(!f) perror("Error: fopen");
    fseeko(f, 0, SEEK_END);
    off_t len = ftello(f);
    fseeko(f, 0, SEEK_SET);
    char *buf = malloc(len);
    if(!buf) perror("Error: malloc");
    size_t off = 0,
           remain = len;
    while(remain > 0)
    {
        size_t n = fread(buf + off, 1, remain, f);
        off += n;
        remain -= n;
    }
    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8*)buf, len, kCFAllocatorNull);
    ASSERT(data, return -1, "");
    CFDictionaryRef plist = CFPropertyListCreateWithData(NULL, data, 0, NULL, NULL);
    ASSERT(plist, return -1, "");
    CFArrayRef assets = CFDictionaryGetValue(plist, CFSTR("Assets"));
    ASSERT(assets, return -1, "");
    CFIndex num = CFArrayGetCount(assets);
    printf("%11s %11s %11s %11s %11s %s\n", "Version", "Build", "Preq", "Device", "Model", "URL");
    for(CFIndex i = 0; i < num; ++i)
    {
        CFDictionaryRef fw = CFArrayGetValueAtIndex(assets, i);
        ASSERT(fw, continue, "idx %lu ", i);
        CFStringRef prestr = CFDictionaryGetValue(fw, CFSTR("PrerequisiteBuild"));
        if(complete && prestr) continue;
        CFStringRef cfvers  = CFDictionaryGetValue(fw, CFSTR("OSVersion"));
        CFStringRef cfbuild = CFDictionaryGetValue(fw, CFSTR("Build"));
        CFStringRef cfbase  = CFDictionaryGetValue(fw, CFSTR("__BaseURL"));
        CFStringRef cfpath  = CFDictionaryGetValue(fw, CFSTR("__RelativePath"));
        CFArrayRef  devices = CFDictionaryGetValue(fw, CFSTR("SupportedDevices"));
        CFArrayRef  models  = CFDictionaryGetValue(fw, CFSTR("SupportedDeviceModels"));
        ASSERT(cfvers,  continue, "idx %lu ", i);
        ASSERT(cfbuild, continue, "idx %lu ", i);
        ASSERT(cfbase,  continue, "idx %lu ", i);
        ASSERT(cfpath,  continue, "idx %lu ", i);
        ASSERT(devices, continue, "idx %lu ", i);
        const char *preq    = prestr ? CFStringGetCStringPtr(prestr, kCFStringEncodingUTF8) : "";
        const char *vers    = CFStringGetCStringPtr(cfvers,  kCFStringEncodingUTF8);
        const char *build   = CFStringGetCStringPtr(cfbuild, kCFStringEncodingUTF8);
        const char *base    = CFStringGetCStringPtr(cfbase,  kCFStringEncodingUTF8);
        const char *path    = CFStringGetCStringPtr(cfpath,  kCFStringEncodingUTF8);
        CFIndex dnum = CFArrayGetCount(devices);
        CFIndex mnum = models ? CFArrayGetCount(models) : 1;
        for(CFIndex d = 0; d < dnum; ++d)
        {
            const char *dev = CFStringGetCStringPtr(CFArrayGetValueAtIndex(devices, d), kCFStringEncodingUTF8);
            for(CFIndex m = 0; m < mnum; ++m)
            {
                printf("%11s %11s %11s %11s %11s %s%s\n", vers, build, preq, dev, models ? CFStringGetCStringPtr(CFArrayGetValueAtIndex(models, m), kCFStringEncodingUTF8) : "", base, path);
            }
        }
    }
    return 0;
}

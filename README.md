# misc

Stuff too small for its own repo.

### Tools

-   `bindump`  
    Prints a 64bit command line arg in binary.
-   `clz`  
    Clang's `__builtin_clz`, but for the command line.
-   `mesu`  
    Parses an Apple OTA update plist and prints it nicely.
-   `rand`  
    Generates random numbers or strings.  
-   `strerror`  
    Prints description for a Darwin error code.  
    Simply calls `strerror`, `mach_error_string` or `SecCopyErrorMessageString` with the given command line argument.
-   `vmacho`  
    Extracts a Mach-O into a raw, headless binary.
-   `xref`  
    Parses an arm64 Mach-O and tries to find xrefs to a specified address.

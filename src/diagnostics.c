#include <stdarg.h>
#include <stdio.h>

#include "ast.h"
#include "diagnostics.h"

void eprintln(const char *label, SourceLoc loc, const char *format,
             va_list args) {
    fprintf(stderr, "%zu:%zu: %s: ", loc.line, loc.column, label);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void errorf(SourceLoc loc, const char *format, ...) {
    va_list args;
    va_start(args, format);
    eprintln("error", loc, format, args);
    va_end(args);
}

void warnf(SourceLoc loc, const char *format, ...) {
    va_list args;
    va_start(args, format);
    eprintln("warning", loc, format, args);
    va_end(args);
}

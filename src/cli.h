#pragma once

#include <stddef.h>

#include "arena.h"

typedef struct {
    const char *file_path;
    char *file_content;
} InputFile;

typedef struct {
    InputFile *items;
    size_t count;
    size_t capacity;
} InputFiles;

typedef struct {
    const char *program_name;
    InputFiles input_files;
} CLI;

CLI cli_parse(Arena *arena, int argc, const char **argv);

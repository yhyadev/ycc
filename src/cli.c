#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "arena.h"
#include "cli.h"
#include "dynamic_string.h"
#include "process.h"

char *cli_read_file(Arena *arena, const char *file_path) {
    DynamicString file_content = {0};

    FILE *fd = fopen(file_path, "r");

    if (fd == NULL) {
        perror("error");
        process_exit(1);
    }

    while (true) {
        int ch = fgetc(fd);

        if (ferror(fd)) {
            perror("error");
            fclose(fd);
            process_exit(1);
        }

        if (ch == EOF) {
            arena_da_append(arena, &file_content, '\0');
            break;
        } else {
            arena_da_append(arena, &file_content, ch);
        }
    };

    fclose(fd);

    return file_content.items;
}

CLI cli_parse(Arena *arena, int argc, const char **argv) {
    CLI cli = {.program_name = argv[0]};

    for (int i = 1; i < argc; i++) {
        InputFile input_file = {.file_path = argv[i],
                                .file_content = cli_read_file(arena, argv[i])};

        arena_da_append(arena, &cli.input_files, input_file);
    }

    return cli;
}

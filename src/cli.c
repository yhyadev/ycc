#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "cli.h"
#include "dynamic_array.h"
#include "dynamic_string.h"

char *cli_read_file(const char *file_path) {
    DynamicString file_content = {0};

    FILE *fd = fopen(file_path, "r");

    if (fd == NULL) {
        perror("error");
        exit(1);
    }

    while (true) {
        int ch = fgetc(fd);

        if (ferror(fd)) {
            perror("error");
            fclose(fd);
            exit(1);
        }

        if (ch == EOF) {
            da_append(&file_content, '\0');
            break;
        } else {
            da_append(&file_content, ch);
        }
    };

    fclose(fd);

    return file_content.items;
}

CLI cli_parse(int argc, const char **argv) {
    CLI cli = {.program_name = argv[0]};

    for (int i = 1; i < argc; i++) {
        InputFile input_file = {.file_path = argv[i],
                                .file_content = cli_read_file(argv[i])};

        da_append(&cli.input_files, input_file);
    }

    return cli;
}

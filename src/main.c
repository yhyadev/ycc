#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "cli.h"
#include "driver.h"
#include "dynamic_string.h"
#include "process.h"

#define ARENA_IMPLEMENTATION
#include "arena.h"

static Arena global_arena = {0};

noreturn void process_exit(int status_code) {
    arena_free(&global_arena);
    exit(status_code);
}

int main(int argc, const char **argv) {
    CLI cli = cli_parse(&global_arena, argc, argv);

    if (cli.input_files.count == 0) {
        fprintf(stderr, "error: no input files provided\n");
        return 1;
    }

    if (cli.input_files.count > 1) {
        fprintf(stderr, "todo: multiple input files not handled yet\n");
        return 1;
    }

    driver_compile(&global_arena, cli.input_files.items[0].file_path,
                   cli.input_files.items[0].file_content);

    driver_link(&global_arena, "a.out");

    arena_free(&global_arena);
}

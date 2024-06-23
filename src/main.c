#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>

#include "cli.h"
#include "driver.h"

int main(int argc, const char **argv) {
    CLI cli = cli_parse(argc, argv);

    if (cli.input_files.count == 0) {
        fprintf(stderr, "error: no input files provided\n");
        return 1;
    }

    if (cli.input_files.count > 1) {
        fprintf(stderr, "todo: multiple input files not handled yet\n");
        return 1;
    }

    driver_compile(cli.input_files.items[0].file_path,
                   cli.input_files.items[0].file_content);

    driver_link("a.out");
}

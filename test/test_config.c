#include "unity.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct ConfigInfo config_info;

FILE *log_fp = NULL;

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config_file_path>\n", argv[0]);
        return 1;
    }
    int ret = parse_benchmark_cfg(argv[1], &config_info);
    if (ret) {
        printf("benchmark cfg %s is not valid\n", argv[1]);
    }
    print_benchmark_cfg(&config_info);
    return UNITY_END();
}


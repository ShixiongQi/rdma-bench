#include <stdio.h>
#include "config.h"
#include "ib.h"

int main() {
    struct ib_ctx ctx;
    struct user_param params = {
        .device_idx = 3,
        .sgid_idx = 3,
        .ib_port = 1,
        .mr_num = 2,
        .qp_num = 2,
    };
    init_ib_ctx(&ctx, &params);
    destroy_ib_ctx(&ctx);
    printf("Hello, World!\n");

    return 0;
}


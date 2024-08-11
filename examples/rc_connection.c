#include <stdio.h>
#include "ib.h"

int main() {
    printf("Hello, World!\n");
    struct ib_ctx ctx;
    init_ib_ctx(&ctx, 3);
    destroy_ib_ctx(&ctx);

    return 0;
}


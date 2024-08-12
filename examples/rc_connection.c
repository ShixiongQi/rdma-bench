#include "config.h"
#include "ib.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

int main()
{
    struct ib_ctx ctx;
    struct user_param params = {
        .device_idx = 3,
        .sgid_idx = 3,
        .ib_port = 1,
        .mr_num = 2,
        .qp_num = 2,
        .bf_size = 2048,
    };

    void **buffers = calloc(params.mr_num, sizeof(void *));
    assert(buffers);
    void *buf = calloc(params.mr_num, params.bf_size);
    assert(buf);
    for (size_t i = 0; i < params.mr_num; i++)
    {
        buffers[i] = buf + i * params.bf_size;
    }
    init_ib_ctx(&ctx, &params, buffers);
    destroy_ib_ctx(&ctx);
    free(buf);
    free(buffers);
    printf("Hello, World!\n");

    return 0;
}

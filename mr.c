#include "mr.h"
#include "debug.h"
#include "utils.h"
#include <infiniband/verbs.h>

int register_local_mr(struct ibv_pd *pd, void *addr, size_t length, struct ibv_mr **mr)
{
    *mr = ibv_reg_mr(pd, addr, length, IBV_ACCESS_LOCAL_WRITE);
    if (unlikely(!(*mr)))
    {
        log_error("register local memory region fail");
        return -1;
    }
    return 0;
}
int register_remote_mr(struct ibv_pd *pd, void *addr, size_t length, struct ibv_mr **mr)
{

    *mr = ibv_reg_mr(pd, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (unlikely(!(*mr)))
    {
        log_error("register remote memory region fail");
        return -1;
    }
    return 0;
}

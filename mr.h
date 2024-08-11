#ifndef MR_H_
#define MR_H_

#include "utils.h"
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <stdint.h>

struct MRInfo
{
    uint64_t addr;
    uint32_t lkey;
    uint32_t rkey;
    size_t length;
} __attribute__((packed));

int register_local_mr(struct ibv_pd *pd, void *addr, size_t length, struct ibv_mr **mr);
int register_remote_mr(struct ibv_pd *pd, void *addr, size_t length, struct ibv_mr **mr);

#endif // MR_H_

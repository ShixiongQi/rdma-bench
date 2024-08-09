#ifndef SETUP_IB_H_
#define SETUP_IB_H_

#include "rdma-bench_cfg.h"
#include <assert.h>
#include <infiniband/verbs.h>

struct IBRes
{
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq *cq;
    struct ibv_qp **qp;
    struct ibv_srq *srq;
    struct ibv_port_attr port_attr;
    struct ibv_device_attr dev_attr;

    int num_qps;
    char *ib_buf;
    size_t ib_buf_size;

    union ibv_gid sgid;

    /* Used for one-sided WRITE */
    uint32_t rkey;
    uint64_t raddr;
    uint32_t rsize;
};

int setup_ib(struct IBRes *ib_res);
void close_ib_connection(struct IBRes *ib_res);

#endif /*setup_ib.h*/

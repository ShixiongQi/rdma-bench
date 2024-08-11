#ifndef QP_H_
#define QP_H_

#include "debug.h"
#include "mr.h"
#include "utils.h"
#include <arpa/inet.h>
#include <infiniband/verbs.h>

#include <unistd.h>

#include <rdma/rdma_cma.h>
struct QP_res
{
    uint16_t lid;
    uint32_t qp_num;
    union ibv_gid gid;
    uint8_t sgid_index;
    uint8_t ib_port;
    uint32_t psn;
    uint32_t mr_num;
    struct MRInfo **mr;
} __attribute__((packed));

#endif /* QP_H_ */

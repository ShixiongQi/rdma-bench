#ifndef QP_H_
#define QP_H_

#include "debug.h"
#include "utils.h"
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <unistd.h>

#include <rdma/rdma_cma.h>
struct QPInfo
{
    uint16_t lid;
    uint32_t qp_num;
    union ibv_gid gid;
    uint8_t sgid_index;
    uint8_t ib_port;
    uint32_t rkey;
    uint64_t raddr;
    uint32_t rsize;
    uint32_t psn;

} __attribute__((packed));

void print_qp_info(struct QPInfo *qp_info);

int modify_qp_to_rts(struct ibv_qp *qp, struct QPInfo *local, struct QPInfo *remote);

#endif /* QP_H_ */

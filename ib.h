#ifndef IB_H_
#define IB_H_

#include "config.h"
#include <arpa/inet.h>
#include <byteswap.h>
#include <endian.h>
#include <infiniband/verbs.h>
#include <inttypes.h>
#include <rdma/rdma_cma.h>
#include <sys/types.h>

#define IB_WR_ID_STOP 0xE000000000000000
#define SIG_INTERVAL 1000
#define NUM_WC 20

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x)
{
    return bswap_64(x);
}
static inline uint64_t ntohll(uint64_t x)
{
    return bswap_64(x);
}
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x)
{
    return x;
}
static inline uint64_t ntohll(uint64_t x)
{
    return x;
}
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

struct ib_ctx
{
    struct ibv_device *device;
    int device_idx;
    struct ibv_context *context;
    struct ibv_pd *pd;
    uint32_t qp_num;
    struct ibv_qp **qps;
    uint32_t mr_num;
    struct ibv_mr **mrs;
    struct ibv_srq *srq;
    struct ibv_cq *send_cq;
    struct ibv_cq *recv_cq;
    struct ibv_comp_channel *send_channel;
    struct ibv_device_attr device_attr;
    struct ibv_port_attr port_attr;
    int sgid_idx;
    union ibv_gid gid;
    uint16_t lid;
    int send_cqe;
    int recv_cqe;
};

int init_ib_ctx(struct ib_ctx *ctx, struct user_param *params);
int destroy_ib_ctx(struct ib_ctx *ctx);
int post_send_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp,
                       char *buf);

int post_send_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp,
                         char *buf);

int post_srq_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_srq *srq, char *buf);

int post_write_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf, uint64_t raddr,
                        uint32_t rkey);

int post_write_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                          uint64_t raddr, uint32_t rkey);

int post_write_imm_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                            uint64_t raddr, uint32_t rkey, uint32_t imm_data);

int post_write_imm_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                              uint64_t raddr, uint32_t rkey, uint32_t imm_data);
#endif /*ib.h*/

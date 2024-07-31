#ifndef IB_H_
#define IB_H_

#include <arpa/inet.h>
#include <byteswap.h>
#include <endian.h>
#include <infiniband/verbs.h>
#include <inttypes.h>
#include <rdma/rdma_cma.h>
#include <sys/types.h>

#define IB_MTU IBV_MTU_1024
#define IB_SL 0
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

enum MsgType
{
    MSG_CTL_START = 100,
    MSG_CTL_STOP,
};

void print_ibv_gid(union ibv_gid gid);

void print_qp_info(struct QPInfo *qp_info);

int modify_qp_to_rts(struct ibv_qp *qp, struct QPInfo *local, struct QPInfo *remote);

int post_send_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp,
                       char *buf);

int post_send_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp,
                         char *buf);

int post_srq_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_srq *srq, char *buf);

int post_write_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf, uint64_t raddr,
                        uint32_t rkey);

int post_write_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                          uint64_t raddr, uint32_t rkey);

int post_write_imm_data(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf, uint64_t raddr,
                        uint32_t rkey, uint32_t imm_data);
#endif /*ib.h*/

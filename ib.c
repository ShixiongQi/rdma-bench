#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <unistd.h>

#include "debug.h"
#include "ib.h"

int post_send(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp, char *buf,
              int flag)
{
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {.addr = (uintptr_t)buf, .length = req_size, .lkey = lkey};

    struct ibv_send_wr send_wr = {.wr_id = wr_id,
                                  .sg_list = &list,
                                  .num_sge = 1,
                                  .opcode = IBV_WR_SEND_WITH_IMM,
                                  .send_flags = flag,
                                  .imm_data = htonl(imm_data)};

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_send_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp,

                       char *buf)

{
    return post_send(req_size, lkey, wr_id, imm_data, qp, buf, IBV_SEND_SIGNALED);
}

int post_send_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, uint32_t imm_data, struct ibv_qp *qp,
                         char *buf)
{
    return post_send(req_size, lkey, wr_id, imm_data, qp, buf, 0);
}

int post_srq_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_srq *srq, char *buf)
{
    int ret = 0;
    struct ibv_recv_wr *bad_recv_wr;

    struct ibv_sge list = {.addr = (uintptr_t)buf, .length = req_size, .lkey = lkey};

    struct ibv_recv_wr recv_wr = {.wr_id = wr_id, .sg_list = &list, .num_sge = 1};

    ret = ibv_post_srq_recv(srq, &recv_wr, &bad_recv_wr);
    return ret;
}

int post_write(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf, uint64_t raddr,
               uint32_t rkey, int send_flag)
{
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {.addr = (uintptr_t)buf, .length = req_size, .lkey = lkey};

    struct ibv_send_wr send_wr = {
        .wr_id = wr_id,
        .sg_list = &list,
        .num_sge = 1,
        .opcode = IBV_WR_RDMA_WRITE,
        .send_flags = send_flag,
        .wr.rdma.remote_addr = raddr,
        .wr.rdma.rkey = rkey,
    };

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_write_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf, uint64_t raddr,
                        uint32_t rkey)
{
    return post_write(req_size, lkey, wr_id, qp, buf, raddr, rkey, IBV_SEND_SIGNALED);
}

int post_write_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                          uint64_t raddr, uint32_t rkey)
{
    return post_write(req_size, lkey, wr_id, qp, buf, raddr, rkey, 0);
}

int post_write_imm(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf, uint64_t raddr,
                   uint32_t rkey, uint32_t imm_data, int flag)
{
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge sg_list = {.addr = (uintptr_t)buf, .length = req_size, .lkey = lkey};

    struct ibv_send_wr send_wr = {
        .wr_id = wr_id,
        .sg_list = &sg_list,
        .num_sge = 1,
        .opcode = IBV_WR_RDMA_WRITE_WITH_IMM,
        .send_flags = flag,
        .imm_data = htonl(imm_data),
        .wr.rdma.remote_addr = raddr,
        .wr.rdma.rkey = rkey,
    };

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_write_imm_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                            uint64_t raddr, uint32_t rkey, uint32_t imm_data)
{
    return post_write_imm(req_size, lkey, wr_id, qp, buf, raddr, rkey, imm_data, IBV_SEND_SIGNALED);
}

int post_write_imm_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id, struct ibv_qp *qp, char *buf,
                              uint64_t raddr, uint32_t rkey, uint32_t imm_data)
{
    return post_write_imm(req_size, lkey, wr_id, qp, buf, raddr, rkey, imm_data, 0);
}

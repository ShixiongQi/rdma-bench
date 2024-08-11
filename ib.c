#include <arpa/inet.h>
#include <assert.h>
#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdlib.h>

#include <unistd.h>

#include "debug.h"
#include "ib.h"
#include "utils.h"

int init_ib_ctx(struct ib_ctx *ctx, int dev_idx)
{
    int num_of_device;
    struct ibv_device **dev_list;

    dev_list = ibv_get_device_list(&num_of_device);

    if (unlikely(num_of_device <= 0))
    {
        log_error(" Did not detect devices \n");
        log_error(" If device exists, check if driver is up\n");
        goto error;
    }
    assert(dev_idx < num_of_device);
    ctx->device = dev_list[dev_idx];
    if (unlikely(!(ctx->device)))
    {
        log_error("Can not open device %d", dev_idx);
        goto error;
    }

    ctx->context = ibv_open_device(ctx->device);

    if (unlikely(!(ctx->context)))
    {
        log_error("Couldn't get context for the device\n");
        goto error;
    }

    ctx->pd = ibv_alloc_pd(ctx->context);

    if (unlikely(!(ctx->pd)))
    {
        log_error("Couldn't open protecttion domain\n");
        goto error;
    }

    ctx->send_channel = ibv_create_comp_channel(ctx->context);
    if (unlikely(!(ctx->send_channel)))
    {
        log_error("Error, ibv_create_comp_channel() failed\n");
        goto error;
    }
    ctx->send_cq = ibv_create_cq(ctx->context, 1000, NULL, ctx->send_channel, 0);
    if (unlikely(!(ctx->send_cq)))
    {
        log_error("Error, ibv_create_qp() send completion queue failed\n");
        goto error;
    }

    ctx->send_cqe = ctx->send_cq->cqe;

    ctx->recv_cq = ibv_create_cq(ctx->context, 1000, NULL, NULL, 0);
    if (unlikely(!(ctx->recv_cq)))
    {
        log_error("Error, ibv_create_qp() receive completion queue failed\n");
        goto error;
    }

    ctx->recv_cqe = ctx->recv_cq->cqe;

    struct ibv_srq_init_attr attr = {.attr = {/* when using sreq, rx_depth sets the max_wr */
                                              .max_wr = 1000,
                                              .max_sge = 1}};

    ctx->srq = ibv_create_srq(ctx->pd, &attr);
    if (unlikely(!(ctx->srq)))
    {
        log_error("Error, ibv_cratee_srq() failed\n");
        goto error;
    }

    ibv_free_device_list(dev_list);
    return 0;
error:
    ibv_free_device_list(dev_list);
    exit(1);
}

int destroy_ib_ctx(struct ib_ctx *ctx)
{
    if (ctx->pd != NULL)
    {
        ibv_dealloc_pd(ctx->pd);
    }

    if (ctx->context != NULL)
    {
        ibv_close_device(ctx->context);
    }
    if (ctx->send_channel)
    {
        ibv_destroy_comp_channel(ctx->send_channel);
    }
    if (ctx->send_cq)
    {
        ibv_destroy_cq(ctx->send_cq);
    }
    if (ctx->recv_cq)
    {
        ibv_destroy_cq(ctx->recv_cq);
    }
    if (ctx->srq)
    {
        ibv_destroy_srq(ctx->srq);
    }
    return 0;
}

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

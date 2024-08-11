#include "qp.h"
#include "debug.h"
#include "ib.h"
#include "utils.h"
#include <infiniband/verbs.h>
#include <stdint.h>

int init_rc_qp(struct ib_ctx *ctx, struct ibv_qp **qp)
{
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ctx->send_cq,
        .recv_cq = ctx->recv_cq,
        .srq = ctx->srq,
        .cap =
            {
                // TODO add retry to determine the max_send_wr
                .max_send_wr = 64,
                .max_recv_wr = 64,
                /* .max_recv_wr = ib_res->dev_attr.max_qp_wr, */
                .max_send_sge = 1,
                .max_recv_sge = 1,
                /* .max_recv_sge = 1, */
            },
        .qp_type = IBV_QPT_RC,
    };

    *qp = ibv_create_qp(ctx->pd, &qp_init_attr);
    if (unlikely(!(*qp)))
    {
        log_error("Error init qp");
        goto error;
    }

    return SUCCESS;
error:
    return FAILURE;
}

void destroy_qp(struct ibv_qp *qp)
{
    if (qp)
    {
        ibv_destroy_qp(qp);
    }
}

int modify_qp_init(struct ibv_qp *qp, uint8_t ib_port)
{
    int ret = 0;
    struct ibv_qp_attr qp_attr = {
        .qp_state = IBV_QPS_INIT,
        .pkey_index = 0,
        .port_num = ib_port,
        .qp_access_flags =
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE,
    };

    ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
    if (unlikely(!ret))
    {
        log_error("Failed to modify qp to INIT.");
        return -1;
    }
    return 0;
}

int modify_qp_init_to_rtr(struct ibv_qp *qp, uint32_t r_qp_num, uint32_t r_psn, uint16_t r_lid, uint8_t l_ib_port,
                          uint8_t l_sgid_index, union ibv_gid r_gid)

{
    int ret = 0;
    struct ibv_qp_attr qp_attr = {
        .qp_state = IBV_QPS_RTR,
        .path_mtu = IBV_MTU_1024,
        .dest_qp_num = r_qp_num,
        .rq_psn = r_psn,
        .max_dest_rd_atomic = 1,
        .min_rnr_timer = 12,
        .ah_attr.is_global = 0, // grh is invalid for none RoCE
        .ah_attr.dlid = r_lid,  // Not used for RoCEv2
        .ah_attr.sl = 0,        // Service level
        .ah_attr.src_path_bits = 0,
        .ah_attr.port_num = l_ib_port,
    };

    if (qp_attr.ah_attr.dlid == 0)
    {
        printf("Using RoCEv2 transport\n");
        qp_attr.ah_attr.is_global = 1; // grh should be configured for RoCEv2
        qp_attr.ah_attr.grh.sgid_index = l_sgid_index;
        qp_attr.ah_attr.grh.dgid = r_gid;
        qp_attr.ah_attr.grh.hop_limit = 0xFF;
        qp_attr.ah_attr.grh.traffic_class = 0;
        qp_attr.ah_attr.grh.flow_label = 0;
    }
    ret = ibv_modify_qp(qp, &qp_attr,
                        IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                            IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER | 0);
    if (unlikely(ret == 0))
    {
        log_error("Failed to change qp to rtr");
        return FAILURE;
    }
    return SUCCESS;
}

int modify_qp_rtr_to_rts(struct ibv_qp *qp, uint32_t l_psn)
{
    int ret = 0;
    struct ibv_qp_attr qp_attr = {
        .qp_state = IBV_QPS_RTS,
        .timeout = 14,
        .retry_cnt = 7,
        .rnr_retry = 7,
        .sq_psn = l_psn,
        .max_rd_atomic = 1,
    };

    ret = ibv_modify_qp(qp, &qp_attr,
                        IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                            IBV_QP_MAX_QP_RD_ATOMIC);
    if (unlikely(ret == 0))
    {
        log_error("Failed to change qp to rts");
        return FAILURE;
    }
    return SUCCESS;
}

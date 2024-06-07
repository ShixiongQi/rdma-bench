#include <arpa/inet.h>
#include <unistd.h>

#include "ib.h"
#include "debug.h"

int modify_qp_to_rts (struct ibv_qp *qp, uint32_t target_qp_num, uint16_t target_lid, union ibv_gid my_gid)
{
    int ret = 0;

    /* change QP state to INIT */
    struct ibv_qp_attr qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));

    qp_attr.qp_state        = IBV_QPS_INIT;
    qp_attr.pkey_index      = 0;
    qp_attr.port_num        = IB_PORT;
    qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE;

    ret = ibv_modify_qp (qp, &qp_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT  | IBV_QP_ACCESS_FLAGS); check (ret == 0, "Failed to modify qp to INIT.");

    // {
    //     struct ibv_qp_attr qp_attr = {
    //         .qp_state        = IBV_QPS_INIT,
    //         .pkey_index      = 0,
    //         .port_num        = IB_PORT,
    //         .qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
    //                            IBV_ACCESS_REMOTE_READ |
    //                            IBV_ACCESS_REMOTE_ATOMIC |
    //                            IBV_ACCESS_REMOTE_WRITE,
    //     };

    //     ret = ibv_modify_qp (qp, &qp_attr,
    //                      IBV_QP_STATE | IBV_QP_PKEY_INDEX |
    //                      IBV_QP_PORT  | IBV_QP_ACCESS_FLAGS);
    //     printf("ibv_modify_qp returns %d\n", ret);
    //     check (ret == 0, "Failed to modify qp to INIT.");
    // }

    // printf("IB_SL: %d\n", IB_SL);
    // struct ibv_qp_attr qp_attr;

    /* Transition QP to RTR (Ready to Receive) */
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state           = IBV_QPS_RTR;
    qp_attr.path_mtu           = IB_MTU;
    qp_attr.dest_qp_num        = target_qp_num;
    qp_attr.rq_psn             = 0;
    qp_attr.max_dest_rd_atomic = 1;
    qp_attr.min_rnr_timer      = 12;

    /* IB */
    // qp_attr.ah_attr.is_global  = 0;
    // qp_attr.ah_attr.dlid       = target_lid;
    // qp_attr.ah_attr.sl         = IB_SL;
    // qp_attr.ah_attr.src_path_bits = 0;
    // qp_attr.ah_attr.port_num      = IB_PORT;

    /* RoCEv2: Set up AH attributes for IP addressing */
    qp_attr.ah_attr.is_global = 1;
    memcpy(&qp_attr.ah_attr.grh.dgid, &my_gid, sizeof(my_gid));
    qp_attr.ah_attr.grh.flow_label = 0;
    qp_attr.ah_attr.grh.hop_limit = 1;
    qp_attr.ah_attr.grh.sgid_index = 0; // Use appropriate SGID index
    qp_attr.ah_attr.grh.traffic_class = 0;
    qp_attr.ah_attr.dlid = 0; // Not used for RoCEv2
    qp_attr.ah_attr.sl = 0;
    qp_attr.ah_attr.src_path_bits = 0;
    qp_attr.ah_attr.port_num = PORT_NUM;

    ret = ibv_modify_qp(qp, &qp_attr, 
                        IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | 
                        IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | 
                        IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
    check (ret == 0, "Failed to change qp to rtr.");

    /* Change QP state to RTR */
    // {
    //     struct ibv_qp_attr qp_attr = {
    //         .qp_state           = IBV_QPS_RTR,
    //         .path_mtu           = IB_MTU,
    //         .dest_qp_num        = target_qp_num,
    //         .rq_psn             = 0,
    //         .max_dest_rd_atomic = 1,
    //         .min_rnr_timer      = 12,
    //         .ah_attr.is_global  = 0,
    //         .ah_attr.dlid       = target_lid,
    //         .ah_attr.sl         = IB_SL,
    //         .ah_attr.src_path_bits = 0,
    //         .ah_attr.port_num      = IB_PORT,
    //     };

    //     // fprintf(stdout, "qp_state: %d\n", qp_attr.qp_state);
    //     // fprintf(stdout, "path_mtu: %d\n", qp_attr.path_mtu);

    //     // ret = ibv_modify_qp(qp, &qp_attr,
    //     //                     IBV_QP_STATE | IBV_QP_AV |
    //     //                     IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
    //     //                     IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
    //     //                     IBV_QP_MIN_RNR_TIMER);
    //     ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN);
    //     printf("ibv_modify_qp returns %d\n", ret);
    //     check (ret == 0, "Failed to change qp to rtr.");
    // }

    /* Change QP state to RTS */
    {
        struct ibv_qp_attr  qp_attr = {
            .qp_state      = IBV_QPS_RTS,
            .timeout       = 14,
            .retry_cnt     = 7,
            .rnr_retry     = 7,
            .sq_psn        = 0,
            .max_rd_atomic = 1,
        };

        ret = ibv_modify_qp (qp, &qp_attr,
                             IBV_QP_STATE | IBV_QP_TIMEOUT |
                             IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                             IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
        check (ret == 0, "Failed to modify qp to RTS.");
    }

    return 0;
 error:
    return -1;
}

int post_send (uint32_t req_size, uint32_t lkey, uint64_t wr_id,
               uint32_t imm_data, struct ibv_qp *qp, char *buf)
{
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {
        .addr   = (uintptr_t) buf,
        .length = req_size,
        .lkey   = lkey
    };

    struct ibv_send_wr send_wr = {
        .wr_id      = wr_id,
        .sg_list    = &list,
        .num_sge    = 1,
        .opcode     = IBV_WR_SEND_WITH_IMM,
        .send_flags = IBV_SEND_SIGNALED,
        .imm_data   = htonl (imm_data)
    };

    ret = ibv_post_send (qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_srq_recv (uint32_t req_size, uint32_t lkey, uint64_t wr_id, 
                   struct ibv_srq *srq, char *buf)
{
    int ret = 0;
    struct ibv_recv_wr *bad_recv_wr;

    struct ibv_sge list = {
        .addr   = (uintptr_t) buf,
        .length = req_size,
        .lkey   = lkey
    };

    struct ibv_recv_wr recv_wr = {
        .wr_id   = wr_id,
        .sg_list = &list,
        .num_sge = 1
    };

    ret = ibv_post_srq_recv (srq, &recv_wr, &bad_recv_wr);
    return ret;
}

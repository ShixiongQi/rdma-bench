#include "qp.h"
#include <infiniband/verbs.h>

void print_qp_info(struct QPInfo *qp_info)
{
    printf("LID: %u\n", qp_info->lid);
    printf("QP Number: %u\n", qp_info->qp_num);
    printf("GID Index: %u\n", qp_info->sgid_index);
    print_ibv_gid(qp_info->gid);
    printf("ib_port: %u\n", qp_info->ib_port);
    printf("rkey: %u\n", qp_info->rkey);
    printf("raddr: %ld\n", qp_info->raddr);
    printf("rsize: %d\n", qp_info->rsize);
    printf("psn: %d\n", qp_info->psn);
}

int modify_qp_to_rts(struct ibv_qp *qp, struct QPInfo *local, struct QPInfo *remote)
{
    int ret = 0;

    /* change QP state to INIT */
    {
        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_INIT,
            .pkey_index = 0,
            .port_num = local->ib_port,
            .qp_access_flags =
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE,
        };

        ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
        check(ret == 0, "Failed to modify qp to INIT.");
    }

    /* Change QP state to RTR */
    {
        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_RTR,
            .path_mtu = IBV_MTU_1024,
            .dest_qp_num = remote->qp_num,
            .rq_psn = remote->psn,
            .max_dest_rd_atomic = 1,
            .min_rnr_timer = 12,
            .ah_attr.is_global = 0,      // grh is invalid for none RoCE
            .ah_attr.dlid = remote->lid, // Not used for RoCEv2
            .ah_attr.sl = 0,             // Service level
            .ah_attr.src_path_bits = 0,
            .ah_attr.port_num = local->ib_port,
        };

        if (qp_attr.ah_attr.dlid == 0)
        {
            printf("Using RoCEv2 transport\n");
            qp_attr.ah_attr.is_global = 1; // grh should be configured for RoCEv2
            qp_attr.ah_attr.grh.sgid_index = local->sgid_index;
            qp_attr.ah_attr.grh.dgid = remote->gid;
            qp_attr.ah_attr.grh.hop_limit = 0xFF;
            qp_attr.ah_attr.grh.traffic_class = 0;
            qp_attr.ah_attr.grh.flow_label = 0;
        }
        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                                IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER | 0);
        check(ret == 0, "Failed to change qp to rtr.");
    }

    /* Change QP state to RTS */
    {
        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_RTS,
            .timeout = 14,
            .retry_cnt = 7,
            .rnr_retry = 7,
            .sq_psn = local->psn,
            .max_rd_atomic = 1,
        };

        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                                IBV_QP_MAX_QP_RD_ATOMIC);
        check(ret == 0, "Failed to modify qp to RTS.");
    }

    return 0;
error:
    return -1;
}

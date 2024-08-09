#include <arpa/inet.h>
#ifdef USE_RTE_MEMPOOL
#include <rte_branch_prediction.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#endif /* ifdef USE_RTE_MEMPOOL */
#include <malloc.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"
#include "setup_ib.h"

#ifdef USE_RTE_MEMPOOL
#define MEMPOOL_NAME "SPRIGHT_MEMPOOL"

static void *rte_shm_mgr(size_t ib_buf_size)
{
    int ret;
    void *buffer;

    config_info.mempool =
        rte_mempool_create(MEMPOOL_NAME, 1, ib_buf_size, 0, 0, NULL, NULL, NULL, NULL, rte_socket_id(), 0);
    if (unlikely(config_info.mempool == NULL))
    {
        fprintf(stderr, "rte_mempool_create() error: %s\n", rte_strerror(rte_errno));
        goto error_0;
    }

    // Allocate DPDK memory and register it
    ret = rte_mempool_get(config_info.mempool, (void **)&buffer);
    if (unlikely(ret < 0))
    {
        fprintf(stderr, "rte_mempool_get() error: %s\n", rte_strerror(-ret));
        goto error_1;
    }

    return buffer;

error_1:
    rte_mempool_put(config_info.mempool, buffer);
error_0:
    rte_mempool_free(config_info.mempool);
    return NULL;
}
#endif

int setup_ib(struct IBRes *ib_res)
{
    int ret = 0;
    int i = 0;
    int num_devices = 0;
    struct ibv_device **dev_list = NULL;
    memset(ib_res, 0, sizeof(struct IBRes));

    ib_res->num_qps = 1;
    /* get IB device list */
    dev_list = ibv_get_device_list(&num_devices);
    check(dev_list != NULL, "Failed to get ib device list.");

    /* create IB context */
    ib_res->ctx = ibv_open_device(dev_list[config_info.dev_index]);
    check(ib_res->ctx != NULL, "Failed to open ib device.");

    /* allocate protection domain */
    ib_res->pd = ibv_alloc_pd(ib_res->ctx);
    check(ib_res->pd != NULL, "Failed to allocate protection domain.");

    /* query IB port attribute */
    ret = ibv_query_port(ib_res->ctx, config_info.ib_port, &ib_res->port_attr);
    check(ret == 0, "Failed to query IB port information.");

    /* query GID (RoCEv2) */
    if (ib_res->port_attr.lid == 0 && ib_res->port_attr.link_layer == IBV_LINK_LAYER_ETHERNET)
    {
        ret = ibv_query_gid(ib_res->ctx, config_info.ib_port, config_info.sgid_index, &ib_res->sgid);
        check(!ret, "Failed to query GID.");

        print_ibv_gid(ib_res->sgid);
    }

    /* register mr */
    /* set the buf_size twice as large as msg_size * num_concurr_msgs */
    /* the recv buffer occupies the first half while the sending buffer */
    /* occupies the second half */
    /* assume all msgs are of the same content */
    ib_res->ib_buf_size = config_info.msg_size * config_info.num_concurr_msgs * ib_res->num_qps;
#ifdef USE_RTE_MEMPOOL
    ib_res->ib_buf = (char *)rte_shm_mgr(ib_res->ib_buf_size);
#else
    ib_res->ib_buf = (char *)memalign(4096, ib_res->ib_buf_size);
#endif
    check(ib_res->ib_buf != NULL, "Failed to allocate ib_buf");

    ib_res->mr = ibv_reg_mr(ib_res->pd, (void *)ib_res->ib_buf, ib_res->ib_buf_size,
                            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    check(ib_res->mr != NULL, "Failed to register mr");

    /* query IB device attr */
    ret = ibv_query_device(ib_res->ctx, &ib_res->dev_attr);
    check(ret == 0, "Failed to query device");

    /* create cq */
    ib_res->cq = ibv_create_cq(ib_res->ctx, ib_res->dev_attr.max_cqe - 1, NULL, NULL, 0);
    check(ib_res->cq != NULL, "Failed to create cq");

    assert(ib_res->dev_attr.max_srq != 0);
    /* create srq */
    struct ibv_srq_init_attr srq_init_attr = {
        .attr.max_wr = ib_res->dev_attr.max_srq_wr,
        .attr.max_sge = 1,
    };

    ib_res->srq = ibv_create_srq(ib_res->pd, &srq_init_attr);
    if (unlikely(!ib_res->srq))
    {
        log_error("Failed to create shared receive queue");
        goto error;
    }

    /* create qp */
    // when srq is used, the max_recv_wr and max_recv_sge is ignored
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res->cq,
        .recv_cq = ib_res->cq,
        .srq = ib_res->srq,
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

    ib_res->qp = (struct ibv_qp **)calloc(ib_res->num_qps, sizeof(struct ibv_qp *));
    check(ib_res->qp != NULL, "Failed to allocate qp array");

    for (i = 0; i < ib_res->num_qps; i++)
    {
        ib_res->qp[i] = ibv_create_qp(ib_res->pd, &qp_init_attr);
        check(ib_res->qp[i] != NULL, "Failed to create qp[%d]", i);
    }

    ibv_free_device_list(dev_list);
    return 0;

error:
    if (dev_list != NULL)
    {
        ibv_free_device_list(dev_list);
    }
    return -1;
}

void close_ib_connection(struct IBRes *ib_res)
{
    int i;

    if (ib_res->qp != NULL)
    {
        for (i = 0; i < ib_res->num_qps; i++)
        {
            if (ib_res->qp[i] != NULL)
            {
                ibv_destroy_qp(ib_res->qp[i]);
            }
        }
        free(ib_res->qp);
    }

    if (ib_res->srq != NULL)
    {
        ibv_destroy_srq(ib_res->srq);
    }

    if (ib_res->cq != NULL)
    {
        ibv_destroy_cq(ib_res->cq);
    }

    if (ib_res->mr != NULL)
    {
        ibv_dereg_mr(ib_res->mr);
    }

    if (ib_res->pd != NULL)
    {
        ibv_dealloc_pd(ib_res->pd);
    }

    if (ib_res->ctx != NULL)
    {
        ibv_close_device(ib_res->ctx);
    }

    if (config_info.peer_sockfds != NULL)
    {
        for (i = 0; i < 1; i++)
        {
            if (config_info.peer_sockfds[i] > 0)
            {
                close(config_info.peer_sockfds[i]);
            }
        }
        free(config_info.peer_sockfds);
    }
    if (config_info.self_sockfd > 0)
    {
        close(config_info.self_sockfd);
    }

    if (ib_res->ib_buf != NULL)
    {
#ifdef USE_RTE_MEMPOOL
        rte_mempool_put(config_info.mempool, ib_res->ib_buf);
#else
        free(ib_res->ib_buf);
#endif
    }

#ifdef USE_RTE_MEMPOOL
    /* Clean up rte mempool */
    rte_mempool_free(config_info.mempool);
#endif
}

#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>

#include <rte_mempool.h>
#include <rte_errno.h>

#include "sock.h"
#include "ib.h"
#include "debug.h"
#include "config.h"
#include "setup_ib.h"

struct IBRes ib_res;

void print_ibv_gid(union ibv_gid gid) {
    printf("Raw GID: ");
    for (int i = 0; i < 16; ++i) {
        printf("%02x", gid.raw[i]);
        if (i % 2 && i != 15) {
            printf(":");
        }
    }
    printf("\n");

    printf("Subnet Prefix: 0x%" PRIx64 "\n", (uint64_t) gid.global.subnet_prefix);
    printf("Interface ID: 0x%" PRIx64 "\n", (uint64_t) gid.global.interface_id);
}

void print_qp_info(struct QPInfo *qp_info) {
    printf("LID: %u\n", qp_info->lid);
    printf("QP Number: %u\n", qp_info->qp_num);
    printf("Rank: %u\n", qp_info->rank);
    printf("GID Index: %u\n", qp_info->sgid_index);
    print_ibv_gid(qp_info->gid);
}

int connect_qp_server() {
    int ret = 0, n = 0, i = 0;
    int num_peers = config_info.num_clients;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len	= sizeof(struct sockaddr_in);
    char sock_buf[64] = {'\0'};
    struct QPInfo *local_qp_info = NULL;
    struct QPInfo *remote_qp_info = NULL;

    config_info.self_sockfd = sock_create_bind(config_info.sock_port);
    check(config_info.self_sockfd > 0, "Failed to create server socket.");
    listen(config_info.self_sockfd, 5);

    config_info.peer_sockfds = (int *) calloc (num_peers, sizeof(int));
    check(config_info.peer_sockfds != NULL, "Failed to allocate peer_sockfd");

    for (i = 0; i < num_peers; i++) {
        config_info.peer_sockfds[i] = accept(config_info.self_sockfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        check(config_info.peer_sockfds[i] > 0, "Failed to create peer_sockfd[%d]", i);
    }

    /* init local qp_info */
    local_qp_info = (struct QPInfo *) calloc (num_peers, sizeof(struct QPInfo));
    check(local_qp_info != NULL, "Failed to allocate local_qp_info");

    for (i = 0; i < num_peers; i++) {
        local_qp_info[i].lid       = ib_res.port_attr.lid; 
        local_qp_info[i].qp_num    = ib_res.qp[i]->qp_num;
        local_qp_info[i].rank      = config_info.rank;
        local_qp_info[i].sgid_index = config_info.sgid_index;
        local_qp_info[i].gid       = ib_res.sgid;
        local_qp_info[i].rkey = ib_res.mr->rkey;
        local_qp_info[i].raddr = (uint64_t)ib_res.mr->addr;
        local_qp_info[i].rsize = ib_res.mr->length;
    }

    /* get qp_info from client */
    remote_qp_info = (struct QPInfo *) calloc (num_peers, sizeof(struct QPInfo));
    check(remote_qp_info != NULL, "Failed to allocate remote_qp_info");

    for (i = 0; i < num_peers; i++) {
        ret = sock_get_qp_info (config_info.peer_sockfds[i], &remote_qp_info[i]);
        check(ret == 0, "Failed to get qp_info from client[%d]", i);
    }
    // TODO temporary setting for one server one client benchmark
    assert(num_peers == 1);
    ib_res.raddr = remote_qp_info[0].raddr;
    ib_res.rkey = remote_qp_info[0].rkey;
    ib_res.rsize = remote_qp_info[0].rsize;
    
    /* send qp_info to client */
    int peer_ind = -1;
    int j = 0;
    for (i = 0; i < num_peers; i++) {
        peer_ind = -1;
        for (j = 0; j < num_peers; j++) {
            if (remote_qp_info[j].rank == i) {
                peer_ind = j;
                break;
            }
        }
        ret = sock_set_qp_info (config_info.peer_sockfds[i], &local_qp_info[peer_ind]);
        check(ret == 0, "Failed to send qp_info to client[%d]", peer_ind);
    }

    /* change send QP state to RTS */
    log (LOG_SUB_HEADER, "Start of IB Config");
    for (i = 0; i < num_peers; i++) {
        peer_ind = -1;
        for (j = 0; j < num_peers; j++) {
            if (remote_qp_info[j].rank == i) {
                peer_ind = j;
                break;
            }
        }

        printf("Loca qp_num: %"PRIu32", Remote qp_num %"PRIu32"\n", local_qp_info[peer_ind].qp_num, remote_qp_info[i].qp_num);

        printf("Local QP info: \n");
        print_qp_info(&local_qp_info[peer_ind]);
        printf("\n");
        printf("Remote QP info: \n");
        print_qp_info(&remote_qp_info[i]);
        printf("\n");

        ret = modify_qp_to_rts (ib_res.qp[peer_ind], &local_qp_info[peer_ind], &remote_qp_info[i]);
        check(ret == 0, "Failed to modify qp[%d] to rts", peer_ind);
        log ("\tLocal qp[%"PRIu32"] <-> Remote qp[%"PRIu32"]", ib_res.qp[peer_ind]->qp_num, remote_qp_info[i].qp_num);
    }
    log (LOG_SUB_HEADER, "End of IB Config");

    /* sync with clients */
    for (i = 0; i < num_peers; i++) {
        n = sock_read (config_info.peer_sockfds[i], sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    }
    
    for (i = 0; i < num_peers; i++) {
        n = sock_write (config_info.peer_sockfds[i], sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    }

    return 0;

 error:
    if (config_info.peer_sockfds != NULL) {
        for (i = 0; i < num_peers; i++) {
            if (config_info.peer_sockfds[i] > 0) {
                close (config_info.peer_sockfds[i]);
            }
        }
        free (config_info.peer_sockfds);
    }
    if (config_info.self_sockfd > 0) {
        close (config_info.self_sockfd);
    }
    
    return -1;
}

int connect_qp_client() {
    int ret = 0, n = 0, i = 0;
    int num_peers = ib_res.num_qps;
    config_info.self_sockfd = -1;
    char sock_buf[64] = {'\0'};

    struct QPInfo *local_qp_info  = NULL;
    struct QPInfo *remote_qp_info = NULL;

    config_info.peer_sockfds = (int *) calloc (num_peers, sizeof(int));
    check(config_info.peer_sockfds != NULL, "Failed to allocate peer_sockfd");

    for (i = 0; i < num_peers; i++) {
        config_info.peer_sockfds[i] = sock_create_connect (config_info.servers[i], config_info.sock_port);
        check(config_info.peer_sockfds[i] > 0, "Failed to create peer_sockfd[%d]", i);
    }

    /* init local qp_info */
    local_qp_info = (struct QPInfo *) calloc (num_peers, sizeof(struct QPInfo));
    check(local_qp_info != NULL, "Failed to allocate local_qp_info");

    for (i = 0; i < num_peers; i++) {
        local_qp_info[i].lid       = ib_res.port_attr.lid; 
        local_qp_info[i].qp_num    = ib_res.qp[i]->qp_num; 
        local_qp_info[i].rank      = config_info.rank;
        local_qp_info[i].sgid_index = config_info.sgid_index;
        local_qp_info[i].gid       = ib_res.sgid;
        local_qp_info[i].rkey = ib_res.mr->rkey;
        local_qp_info[i].raddr = (uint64_t)ib_res.mr->addr;
        local_qp_info[i].rsize = ib_res.mr->length;
    }

    /* send qp_info to server */
    for (i = 0; i < num_peers; i++) {
        ret = sock_set_qp_info (config_info.peer_sockfds[i], &local_qp_info[i]);
        check(ret == 0, "Failed to send qp_info[%d] to server", i);
    }

    /* get qp_info from server */    
    remote_qp_info = (struct QPInfo *) calloc (num_peers, sizeof(struct QPInfo));
    check(remote_qp_info != NULL, "Failed to allocate remote_qp_info");

    for (i = 0; i < num_peers; i++) {
        ret = sock_get_qp_info (config_info.peer_sockfds[i], &remote_qp_info[i]);
        check(ret == 0, "Failed to get qp_info[%d] from server", i);
    }
    
    // TODO temporary setting for one server one client benchmark
    assert(num_peers == 1);
    ib_res.raddr = remote_qp_info[0].raddr;
    ib_res.rkey = remote_qp_info[0].rkey;
    ib_res.rsize = remote_qp_info[0].rsize;

    /* change QP state to RTS */
    /* send qp_info to client */
    int peer_ind = -1;
    int j        = 0;
    log (LOG_SUB_HEADER, "IB Config");
    for (i = 0; i < num_peers; i++) {
        peer_ind = -1;
        for (j = 0; j < num_peers; j++) {
            if (remote_qp_info[j].rank == i) {
                peer_ind = j;
                break;
            }
        }

        printf("Loca qp_num: %"PRIu32", Remote qp_num %"PRIu32"\n", local_qp_info[peer_ind].qp_num, remote_qp_info[i].qp_num);

        printf("Local QP info: \n");
        print_qp_info(&local_qp_info[peer_ind]);
        printf("\n");
        printf("Remote QP info: \n");
        print_qp_info(&remote_qp_info[i]);
        printf("\n");

        ret = modify_qp_to_rts(ib_res.qp[peer_ind], &local_qp_info[peer_ind], &remote_qp_info[i]);
        check(ret == 0, "Failed to modify qp[%d] to rts", peer_ind);
        log ("\tLocal qp[%"PRIu32"] <-> Remote qp[%"PRIu32"]", ib_res.qp[peer_ind]->qp_num, remote_qp_info[i].qp_num);
    }
    log (LOG_SUB_HEADER, "End of IB Config");

    /* sync with server */
    for (i = 0; i < num_peers; i++) {
        n = sock_write (config_info.peer_sockfds[i], sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client[%d]", i);
    }
    
    for (i = 0; i < num_peers; i++) {
        n = sock_read (config_info.peer_sockfds[i], sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    }

    free (local_qp_info);
    free (remote_qp_info);
    return 0;

 error:
    if (config_info.peer_sockfds != NULL) {
        for (i = 0; i < num_peers; i++) {
            if (config_info.peer_sockfds[i] > 0) {
                close (config_info.peer_sockfds[i]);
            }
        }
        free (config_info.peer_sockfds);
    }

    if (local_qp_info != NULL) {
        free (local_qp_info);
    }

    if (remote_qp_info != NULL) {
        free (remote_qp_info);
    }
    
    return -1;
}

#ifdef USE_RTE_MEMPOOL
    #define MEMPOOL_NAME "SPRIGHT_MEMPOOL"

    static void* rte_shm_mgr(size_t ib_buf_size) {
        int ret;
        void *buffer;

        config_info.mempool = rte_mempool_create(MEMPOOL_NAME, 1,
                                        ib_buf_size, 0, 0,
                                        NULL, NULL, NULL, NULL,
                                        rte_socket_id(), 0);
        if (unlikely(config_info.mempool == NULL)) {
            fprintf(stderr, "rte_mempool_create() error: %s\n",
                    rte_strerror(rte_errno));
            goto error_0;
        }

        // Allocate DPDK memory and register it
        ret = rte_mempool_get(config_info.mempool, (void **)&buffer);
        if (unlikely(ret < 0)) {
            fprintf(stderr, "rte_mempool_get() error: %s\n",
                    rte_strerror(-ret));
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

int setup_ib() {
    int	ret	= 0;
    int i = 0;
    int num_devices = 0;
    struct ibv_device **dev_list = NULL;    
    memset(&ib_res, 0, sizeof(struct IBRes));

    if (config_info.is_server) {
        ib_res.num_qps = config_info.num_clients;
    } else {
        ib_res.num_qps = config_info.num_servers;
    }

    /* get IB device list */
    dev_list = ibv_get_device_list(&num_devices);
    check(dev_list != NULL, "Failed to get ib device list.");

    /* create IB context */
    ib_res.ctx = ibv_open_device(dev_list[config_info.dev_index]);
    check(ib_res.ctx != NULL, "Failed to open ib device.");

    /* allocate protection domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    check(ib_res.pd != NULL, "Failed to allocate protection domain.");

    /* query IB port attribute */
    ret = ibv_query_port(ib_res.ctx, config_info.sgid_index, &ib_res.port_attr);
    check(ret == 0, "Failed to query IB port information.");

    /* query GID (RoCEv2) */
    if (ib_res.port_attr.lid == 0 && ib_res.port_attr.link_layer == IBV_LINK_LAYER_ETHERNET) {
        ret = ibv_query_gid(ib_res.ctx, config_info.sgid_index, config_info.dev_index, &ib_res.sgid);
        check(!ret, "Failed to query GID.");

        print_ibv_gid(ib_res.sgid);
    }
    
    /* register mr */
    /* set the buf_size twice as large as msg_size * num_concurr_msgs */
    /* the recv buffer occupies the first half while the sending buffer */
    /* occupies the second half */
    /* assume all msgs are of the same content */
    ib_res.ib_buf_size = config_info.msg_size * config_info.num_concurr_msgs * ib_res.num_qps;
#ifdef USE_RTE_MEMPOOL
    ib_res.ib_buf      = (char *) rte_shm_mgr(ib_res.ib_buf_size);
#else
    ib_res.ib_buf      = (char *) memalign (4096, ib_res.ib_buf_size);
#endif
    check(ib_res.ib_buf != NULL, "Failed to allocate ib_buf");

    ib_res.mr = ibv_reg_mr (ib_res.pd, (void *)ib_res.ib_buf,
                            ib_res.ib_buf_size,
                            IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ |
                            IBV_ACCESS_REMOTE_WRITE);
    check(ib_res.mr != NULL, "Failed to register mr");
    
    /* query IB device attr */
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    check(ret==0, "Failed to query device");

    /* create cq */
    ib_res.cq = ibv_create_cq(ib_res.ctx, ib_res.dev_attr.max_cqe - 1, NULL, NULL, 0);
    check(ib_res.cq != NULL, "Failed to create cq");

    /* create srq */
    struct ibv_srq_init_attr srq_init_attr = {
        .attr.max_wr  = ib_res.dev_attr.max_srq_wr,
        .attr.max_sge = 1,
    };

    ib_res.srq = ibv_create_srq (ib_res.pd, &srq_init_attr);

    /* create qp */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .srq     = ib_res.srq,
        .cap = {
            .max_send_wr = ib_res.dev_attr.max_qp_wr,
            .max_recv_wr = ib_res.dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ib_res.qp = (struct ibv_qp **) calloc (ib_res.num_qps, sizeof(struct ibv_qp *));
    check(ib_res.qp != NULL, "Failed to allocate qp");

    for (i = 0; i < ib_res.num_qps; i++) {
        ib_res.qp[i] = ibv_create_qp (ib_res.pd, &qp_init_attr);
        check(ib_res.qp[i] != NULL, "Failed to create qp[%d]", i);
    }

    /* connect QP */
    if (config_info.is_server) {
        ret = connect_qp_server();
    } else {
        ret = connect_qp_client();
    }
    check(ret == 0, "Failed to connect qp");

    ibv_free_device_list (dev_list);
    return 0;

 error:
    if (dev_list != NULL) {
        ibv_free_device_list (dev_list);
    }
    return -1;
}

void close_ib_connection() {
    int i;

    if (ib_res.qp != NULL) {
        for (i = 0; i < ib_res.num_qps; i++) {
            if (ib_res.qp[i] != NULL) {
                ibv_destroy_qp (ib_res.qp[i]);
            }
        }
        free (ib_res.qp);
    }

    if (ib_res.srq != NULL) {
        ibv_destroy_srq (ib_res.srq);
    }

    if (ib_res.cq != NULL) {
        ibv_destroy_cq (ib_res.cq);
    }

    if (ib_res.mr != NULL) {
        ibv_dereg_mr (ib_res.mr);
    }

    if (ib_res.pd != NULL) {
        ibv_dealloc_pd (ib_res.pd);
    }

    if (ib_res.ctx != NULL) {
        ibv_close_device (ib_res.ctx);
    }

    if (config_info.peer_sockfds != NULL) {
        for (i = 0; i < config_info.num_clients; i++) {
            if (config_info.peer_sockfds[i] > 0) {
                close (config_info.peer_sockfds[i]);
            }
        }
        free (config_info.peer_sockfds);
    }
    if (config_info.self_sockfd > 0) {
        close (config_info.self_sockfd);
    }

    if (ib_res.ib_buf != NULL) {
#ifdef USE_RTE_MEMPOOL
        rte_mempool_put(config_info.mempool, ib_res.ib_buf);
#else
        free (ib_res.ib_buf);
#endif
    }

#ifdef USE_RTE_MEMPOOL
    /* Clean up rte mempool */
    rte_mempool_free(config_info.mempool);
#endif
}

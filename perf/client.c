#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

#include "client.h"
#include "config.h"
#include "debug.h"
#include "ib.h"
#include "setup_ib.h"

struct args
{
    struct IBRes *ib_res;
};

void *client_thread_write_signaled(void *arg)
{

    int ret = 0;
    struct args *args = (struct args *)arg;
    struct IBRes *ib_res = args->ib_res;
    assert(ib_res->num_qps == 1);
    int msg_size = config_info.msg_size;
    int num_concurr_msgs = config_info.num_concurr_msgs;

    struct ibv_qp **qp = ib_res->qp;
    struct ibv_cq *cq = ib_res->cq;
    struct ibv_srq *srq = ib_res->srq;
    struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res->mr->lkey;

    char *buf_ptr = ib_res->ib_buf;
    char *buf_base = ib_res->ib_buf;
    int buf_offset = 0;
    size_t buf_size = ib_res->ib_buf_size;
    int num_completion = 0;

    uint32_t rkey = ib_res->rkey;
    uint64_t raddr = ib_res->raddr;
    uint64_t rptr = raddr;
    uint32_t rsize = ib_res->rsize;
    int roffset = 0;

    struct timeval start, end;
    double duration = 0.0;
    double throughput = 0.0;
    double latency = 0.0;

    wc = (struct ibv_wc *)calloc(NUM_WC, sizeof(struct ibv_wc));
    check(wc != NULL, "thread: failed to allocate wc.");

    for (int j = 0; j < num_concurr_msgs; j++)
    {
        ret = post_srq_recv(msg_size, lkey, (uint64_t)buf_ptr, srq, buf_ptr);
        if (unlikely(ret != 0))
        {
            log_error("post shared receive request fail");
            goto error;
        }
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr = buf_base + buf_offset;
    }

    printf("Client thread wait for start signal...\n");
    /* wait for start signal */

    bool start_sending = false;
    while (!start_sending)
    {
        num_completion = ibv_poll_cq(cq, NUM_WC, wc);
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (unlikely(wc[i].status != IBV_WC_SUCCESS))
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
            if (wc[i].opcode == IBV_WC_RECV)
            {
                /* post a receive */
                post_srq_recv(msg_size, lkey, wc[i].wr_id, srq, buf_base);
                if ((wc[i].wc_flags & IBV_WC_WITH_IMM) && (ntohl(wc[i].imm_data) == MSG_CTL_START))
                {
                    start_sending = true;
                }
            }
        }
    }

    log_debug("thread: ready to send");

    buf_offset = 0;
    roffset = 0;
    debug("buf_ptr = %" PRIx64 "", (uint64_t)buf_ptr);

    long int opt_count = 0;
    bool stop = false;
    while (!stop)
    {
        ret = post_write_signaled(msg_size, lkey, 1, *qp, buf_ptr, rptr, rkey);

        while ((num_completion = ibv_poll_cq(cq, NUM_WC, wc)) == 0)
        {
        };
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (unlikely(wc[i].status != IBV_WC_SUCCESS))
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
            opt_count++;
            if (opt_count == NUM_WARMING_UP_OPS)
            {
                gettimeofday(&start, NULL);
            }
            if (opt_count == TOT_NUM_OPS)
            {
                gettimeofday(&end, NULL);
                stop = true;
            }
        }
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr = buf_base + buf_offset;
        roffset = (roffset + msg_size) % rsize;
        rptr = raddr + roffset;
    }

    duration = (double)((end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1000000);
    throughput = (double)(opt_count - NUM_WARMING_UP_OPS) / duration;
    latency = duration * 1000000 / (double)(opt_count - NUM_WARMING_UP_OPS);

    log_info("thread: throughput = %f (ops/s)", throughput);
    printf("thread: throughput = %f (ops/s) %f (Bytes/s); ops_count:%ld, duration: %f seconds \n", throughput,
           throughput * msg_size, opt_count - NUM_WARMING_UP_OPS, duration);
    printf("latency: %f\n", latency);

    ret = post_send(0, lkey, IB_WR_ID_STOP, MSG_CTL_STOP, qp[0], ib_res->ib_buf);
    bool finish = false;
    while (!finish)
    {
        num_completion = ibv_poll_cq(cq, NUM_WC, wc);
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
            if (wc[i].opcode == IBV_WC_SEND)
            {
                finish = true;
            }
        }
    }
    free(wc);
    pthread_exit((void *)0);

error:
    free(wc);
    pthread_exit((void *)-1);
}

void *client_thread_write_unsignaled(void *arg)
{
    struct args *args = (struct args *)arg;
    struct IBRes *ib_res = args->ib_res;
    assert(ib_res->num_qps == 1);
    int ret = 0;
    int msg_size = config_info.msg_size;
    int num_concurr_msgs = config_info.num_concurr_msgs;

    struct ibv_qp **qp = ib_res->qp;
    struct ibv_cq *cq = ib_res->cq;
    struct ibv_srq *srq = ib_res->srq;
    struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res->mr->lkey;

    char *buf_ptr = ib_res->ib_buf;
    char *buf_base = ib_res->ib_buf;
    int buf_offset = 0;
    size_t buf_size = ib_res->ib_buf_size;
    int num_completion = 0;

    // remote key and address
    uint32_t rkey = ib_res->rkey;
    uint64_t raddr = ib_res->raddr;
    uint64_t rptr = raddr;
    uint32_t rsize = ib_res->rsize;
    int roffset = 0;

    struct timeval start, end;
    double duration = 0.0;
    double latency = 0.0;

    wc = (struct ibv_wc *)calloc(NUM_WC, sizeof(struct ibv_wc));
    check(wc != NULL, "thread: failed to allocate wc.");

    for (int j = 0; j < num_concurr_msgs; j++)
    {
        ret = post_srq_recv(msg_size, lkey, (uint64_t)buf_ptr, srq, buf_ptr);
        if (unlikely(ret != 0))
        {
            log_error("post shared receive request fail");
            goto error;
        }
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr = buf_base + buf_offset;
    }

    log_debug("thread: ready to send");

    buf_offset = 0;
    roffset = 0;
    debug("buf_ptr = %" PRIx64 "", (uint64_t)buf_ptr);
    long int warm_up_iter = config_info.warm_up_iter;
    long int total_iter = config_info.total_iter;
    int signal_freq = config_info.signal_freq;
    long int opt_count = 0;
    while (true)
    {
        for (int i = 0; i < signal_freq; i++)
        {
            ret = post_write_unsignaled(msg_size, lkey, 1, *qp, buf_ptr, rptr, rkey);
            roffset = (roffset + msg_size) % rsize;
            rptr = raddr + roffset;
        }

        ret = post_write_signaled(msg_size, lkey, 1, *qp, buf_ptr, rptr, rkey);
        roffset = (roffset + msg_size) % rsize;
        rptr = raddr + roffset;

        do
        {
            num_completion = ibv_poll_cq(cq, NUM_WC, wc);
        } while (num_completion == 0);
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (unlikely(wc[i].status != IBV_WC_SUCCESS))
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
        }
        opt_count++;
        if (opt_count == warm_up_iter)
        {
            gettimeofday(&start, NULL);
        }
        if (opt_count == total_iter)
        {
            gettimeofday(&end, NULL);
            break;
        }
    }

    duration = (double)((end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1000000);
    latency = duration * 1000000 / (double)(total_iter - warm_up_iter);

    printf("latency: %f for %d unsignaled operations plus a signaled operation\n", latency, signal_freq);

    ret = post_send(0, lkey, IB_WR_ID_STOP, MSG_CTL_STOP, qp[0], ib_res->ib_buf);
    bool finish = false;
    while (!finish)
    {
        num_completion = ibv_poll_cq(cq, NUM_WC, wc);
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
            if (wc[i].opcode == IBV_WC_SEND)
            {
                finish = true;
            }
        }
    }
    free(wc);
    pthread_exit((void *)0);

error:
    free(wc);
    pthread_exit((void *)-1);
}

void *client_thread_write_imm(void *arg)
{
    struct args *args = (struct args *)arg;
    struct IBRes *ib_res = args->ib_res;
    assert(ib_res->num_qps == 1);
    int ret = 0;
    int msg_size = config_info.msg_size;
    int num_concurr_msgs = config_info.num_concurr_msgs;

    struct ibv_qp **qp = ib_res->qp;
    struct ibv_cq *cq = ib_res->cq;
    struct ibv_srq *srq = ib_res->srq;
    struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res->mr->lkey;

    char *buf_ptr = ib_res->ib_buf;
    char *buf_base = ib_res->ib_buf;
    int buf_offset = 0;
    size_t buf_size = ib_res->ib_buf_size;

    uint32_t rkey = ib_res->rkey;
    uint64_t raddr = ib_res->raddr;
    uint64_t rptr = raddr;
    uint32_t rsize = ib_res->rsize;
    int roffset = 0;

    bool stop = false;

    wc = (struct ibv_wc *)calloc(NUM_WC, sizeof(struct ibv_wc));
    check(wc != NULL, "thread: failed to allocate wc.");

    for (int j = 0; j < num_concurr_msgs; j++)
    {
        ret = post_srq_recv(msg_size, lkey, (uint64_t)buf_ptr, srq, buf_ptr);
        if (unlikely(ret != 0))
        {
            log_error("post shared receive request fail");
            goto error;
        }
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr = buf_base + buf_offset;
    }

    printf("Client thread- wait for start signal...\n");
    /* wait for start signal */

    int num_completion = 0;
    stop = false;
    while (!stop)
    {
        num_completion = ibv_poll_cq(cq, NUM_WC, wc);
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (unlikely(wc[i].status != IBV_WC_SUCCESS))
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
            if (wc[i].opcode == IBV_WC_RECV)
            {
                /* post a receive */
                post_srq_recv(msg_size, lkey, wc[i].wr_id, srq, buf_base);

                if ((wc[i].wc_flags & IBV_WC_WITH_IMM) && (ntohl(wc[i].imm_data) == MSG_CTL_START))
                {
                    stop = true;
                }
            }
        }
    }

    log_debug("thread: ready to send");

    debug("buf_ptr = %" PRIx64 "", (uint64_t)buf_ptr);

    stop = false;
    buf_offset = 0;
    roffset = 0;
    while (!stop)
    {
        ret = post_write_imm_data(msg_size, lkey, 0, *qp, buf_ptr, rptr, rkey, 0);
        if (unlikely(ret != 0))
        {
            log_error("send write imme_data failed, error ret: %d", ret);
            goto error;
        }

        while ((num_completion = ibv_poll_cq(cq, NUM_WC, wc)) == 0)
        {
        };
        if (unlikely(num_completion < 0))
        {
            log_error("failed to poll cq");
            goto error;
        }
        for (int i = 0; i < num_completion; i++)
        {
            if (unlikely(wc[i].status != IBV_WC_SUCCESS))
            {
                log_error("wc failed status: %s.", ibv_wc_status_str(wc[i].status));
                goto error;
            }
            if (wc[i].opcode == IBV_WC_RDMA_WRITE)
            {
            }
            if (wc[i].opcode == IBV_WC_RECV)
            {
                if ((wc[i].wc_flags & IBV_WC_WITH_IMM) && ntohl(wc[i].imm_data) == MSG_CTL_STOP)
                {
                    stop = true;
                }
            }
            post_srq_recv(msg_size, lkey, wc[i].wr_id, srq, buf_base);
        }
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr = buf_base + buf_offset;
        roffset = (roffset + msg_size) % rsize;
        rptr = raddr + roffset;
    }

    pthread_exit((void *)0);

error:
    pthread_exit((void *)-1);
    return NULL;
}

void *client_thread_send(void *arg)
{
    struct args *args = (struct args *)arg;
    struct IBRes *ib_res = args->ib_res;
    int ret = 0, n = 0;
    int msg_size = config_info.msg_size;
    int num_concurr_msgs = config_info.num_concurr_msgs;
    int num_peers = 1;

    pthread_t self;
    cpu_set_t cpuset;

    struct ibv_qp **qp = ib_res->qp;
    struct ibv_cq *cq = ib_res->cq;
    struct ibv_srq *srq = ib_res->srq;
    struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res->mr->lkey;

    char *buf_ptr = ib_res->ib_buf;
    char *buf_base = ib_res->ib_buf;
    int buf_offset = 0;
    size_t buf_size = ib_res->ib_buf_size;

    uint32_t imm_data = 0;
    int num_acked_peers = 0;
    bool start_sending = false;
    bool stop = false;
    struct timeval start, end;
    long ops_count = 0;
    double duration = 0.0;
    double throughput = 0.0;

    /* set thread affinity */
    self = pthread_self();
    ret = pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset);
    check(ret == 0, "thread: failed to set thread affinity");

    /* pre-post recvs */
    wc = (struct ibv_wc *)calloc(NUM_WC, sizeof(struct ibv_wc));
    check(wc != NULL, "thread: failed to allocate wc.");

    for (int i = 0; i < num_peers; i++)
    {
        for (int j = 0; j < num_concurr_msgs; j++)
        {
            ret = post_srq_recv(msg_size, lkey, (uint64_t)buf_ptr, srq, buf_ptr);
            if (unlikely(ret != 0))
            {
                log_error("post shared receive request fail");
                goto error;
            }
            buf_offset = (buf_offset + msg_size) % buf_size;
            buf_ptr = buf_base + buf_offset;
        }
    }

    printf("Client thread wait for start signal...\n");
    /* wait for start signal */
    while (start_sending != true)
    {
        do
        {
            n = ibv_poll_cq(cq, NUM_WC, wc);
        } while (n < 1);
        check(n > 0, "thread: failed to poll cq");

        for (int i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                check(0, "thread: wc failed status: %s.", ibv_wc_status_str(wc[i].status));
            }
            if (wc[i].opcode == IBV_WC_RECV)
            {
                /* post a receive */
                post_srq_recv(msg_size, lkey, wc[i].wr_id, srq, (char *)wc[i].wr_id);

                if (ntohl(wc[i].imm_data) == MSG_CTL_START)
                {
                    log_debug("received start signal");
                    num_acked_peers += 1;
                    if (num_acked_peers == num_peers)
                    {
                        start_sending = true;
                        break;
                    }
                }
            }
        }
    }

    log_debug("thread: ready to send");

    /* pre-post sends */
    buf_offset = 0;
    log_debug("buf_ptr = %" PRIx64 "", (uint64_t)buf_ptr);
    for (int i = 0; i < num_peers; i++)
    {
        for (int j = 0; j < num_concurr_msgs; j++)
        {
            ret = post_send(msg_size, lkey, (uint64_t)buf_ptr, (uint32_t)i, qp[i], buf_ptr);
            check(ret == 0, "thread: failed to post send");
            buf_offset = (buf_offset + msg_size) % buf_size;
            buf_ptr = buf_base + buf_offset;
        }
    }

    log_debug("pre-post send finished");
    num_acked_peers = 0;
    while (stop != true)
    {
        /* poll cq */
        n = ibv_poll_cq(cq, NUM_WC, wc);
        if (n < 0)
        {
            check(0, "thread: Failed to poll cq");
        }

        for (int i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                if (wc[i].opcode == IBV_WC_SEND)
                {
                    check(0, "thread: send failed status: %s; wr_id = %" PRIx64 "", ibv_wc_status_str(wc[i].status),
                          wc[i].wr_id);
                }
                else
                {
                    check(0, "thread: recv failed status: %s; wr_id = %" PRIx64 "", ibv_wc_status_str(wc[i].status),
                          wc[i].wr_id);
                }
            }

            if (wc[i].opcode == IBV_WC_RECV)
            {
                ops_count += 1;

                if (ops_count == NUM_WARMING_UP_OPS)
                {
                    gettimeofday(&start, NULL);
                }

                imm_data = ntohl(wc[i].imm_data);
                char *msg_ptr = (char *)wc[i].wr_id;

                if (imm_data == MSG_CTL_STOP)
                {
                    num_acked_peers += 1;
                    if (num_acked_peers == num_peers)
                    {
                        gettimeofday(&end, NULL);
                        stop = true;
                        break;
                    }
                }
                else
                {
                    /* echo the message back */
                    post_send(msg_size, lkey, 0, imm_data, qp[imm_data], msg_ptr);
                }

                /* post a new receive */
                ret = post_srq_recv(msg_size, lkey, wc[i].wr_id, srq, msg_ptr);
            }
        } /* loop through all wc */
    }

    /* dump statistics */
    duration = (double)((end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1000000);
    throughput = (double)(ops_count - NUM_WARMING_UP_OPS) / duration;

    log("thread: throughput = %f (ops/s)", throughput);
    printf("thread: throughput = %f (ops/s) %f (Bytes/s)\n", throughput, throughput * msg_size);

    free(wc);
    pthread_exit((void *)0);

error:
    if (wc != NULL)
    {
        free(wc);
    }
    pthread_exit((void *)-1);
}

int run_client(struct IBRes *ib_res)
{
    int ret = 0;

    pthread_t *client_threads = NULL;
    pthread_attr_t attr;
    void *status;
    void *(*client_thread_func)(void *) = NULL;
    int benchmark_type = config_info.benchmark_type;

    log(LOG_SUB_HEADER, "Run Client");

    /* initialize threads */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    client_threads = (pthread_t *)malloc(sizeof(pthread_t));
    check(client_threads != NULL, "Failed to allocate client_threads.");

    if (benchmark_type == SEND)
    {
        client_thread_func = client_thread_send;
    }
    else if (benchmark_type == WRITE_SIGNALED)
    {
        client_thread_func = client_thread_write_signaled;
    }
    else if (benchmark_type == WRITE_UNSIGNALED)
    {
        client_thread_func = client_thread_write_unsignaled;
    }
    else if (benchmark_type == WRITE_IMM)
    {
        client_thread_func = client_thread_write_imm;
    }
    else
    {
        log_error("The benchmark_type is illegal, %d", benchmark_type);
    }
    struct args args = {.ib_res = ib_res};

    ret = pthread_create(client_threads, &attr, client_thread_func, &args);
    check(ret == 0, "Failed to create client_thread");

    bool thread_ret_normally = true;
    ret = pthread_join(*client_threads, &status);
    if ((long)status != 0)
    {
        thread_ret_normally = false;
    }

    if (thread_ret_normally == false)
    {
        goto error;
    }

    pthread_attr_destroy(&attr);
    free(client_threads);
    return 0;

error:
    if (client_threads != NULL)
    {
        free(client_threads);
    }

    pthread_attr_destroy(&attr);
    return -1;
}

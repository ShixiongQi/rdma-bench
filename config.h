#ifndef CONFIG_H_
#define CONFIG_H_

#include <assert.h>
#include <inttypes.h>
#include <libconfig.h>
#include <stdbool.h>

#ifdef USE_RTE_MEMPOOL
#include <rte_branch_prediction.h>
#endif

#ifndef USE_RTE_MEMPOOL
#define unlikely(x) (!!(x))
#endif // !USE_RTE_MEMPOOL

#define MAX_HOSTNAME_LEN 1024
#define NUM_WARMING_UP_OPS 50000
#define TOT_NUM_OPS 2000000

enum BenchMarkType
{
    SEND_SIGNALED = 1,
    SEND_UNSIGNALED,
    WRITE_SIGNALED,
    WRITE_UNSIGNALED,
    WRITE_IMM,

};

struct ConfigInfo
{

    char *server_ip;
    int self_sockfd;   /* self's socket fd */
    int *peer_sockfds; /* peers' socket fd */

    bool is_server; /* if the current node is server */

    char name[64];

    int msg_size;         /* the size of each echo message */
    int num_concurr_msgs; /* the number of messages can be sent concurrently */
    int n_nodes;
    struct
    {
        int id;
        char hostname[64];
        int peers[UINT8_MAX + 1];
        int n_peers;
    } nodes[UINT8_MAX + 1];

    int current_node_idx;

    int benchmark_type;
    int sgid_index; /* local GID index of in ibv_devinfo -v */
    int dev_index;  /* device index of in ibv_devinfo */
    int ib_port;
    int warm_up_iter;
    int total_iter;
    int signal_freq;

    char *sock_port; /* socket port number */

    struct rte_mempool *mempool;
    void *rte_mr; // TODO: save a list of registered MRs in rte_mempool
} __attribute__((aligned(64)));

extern struct ConfigInfo config_info;

void init_config_info(struct ConfigInfo *config);
void free_config_info(struct ConfigInfo *config);
int parse_benchmark_cfg(char *cfg_file, struct ConfigInfo *config);
void print_benchmark_cfg(struct ConfigInfo *config);
void print_config_info();

#endif /* CONFIG_H_*/

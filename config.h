#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>
#include <inttypes.h>
#include <libconfig.h>
#include <rte_branch_prediction.h>
#include <assert.h>

#define USE_RTE_MEMPOOL 1

#if USE_RTE_MEMPOOL == 0
#define unlikely(x) (!!x)
#endif // !USE_RTE_MEMPOOL
 
#define MAX_HOSTNAME_LEN 1024

enum ConfigFileAttr {
    ATTR_SERVERS = 1,
    ATTR_CLIENTS,
    ATTR_MSG_SIZE,
    ATTR_NUM_CONCURR_MSGS,
    ATTR_BENCHMARK_TYPE,
};

enum BenchMarkType {
    SEND = 1,
    WRITE_SIGNALED,
    WRITE_UNSIGNALED,
    WRITE_IMM,
};

struct ConfigInfo {
    int  num_servers;
    int  num_clients;
    char **servers;          /* list of servers */
    char **clients;          /* list of clients */

    int self_sockfd;         /* self's socket fd */
    int *peer_sockfds;       /* peers' socket fd */
    
    bool is_server;          /* if the current node is server */
    bool is_client;          /* if the current node is client */
    int  rank;               /* the rank of the node */

    char name[64];

    int  msg_size;           /* the size of each echo message */
    int  num_concurr_msgs;   /* the number of messages can be sent concurrently */
    int n_nodes;
    struct {
        int id;
        char hostname[64];
        int peers[UINT8_MAX + 1];
        int n_peers;
    } nodes[UINT8_MAX + 1];

    int current_node_idx;

    int benchmark_type;
    int  sgid_index;         /* local GID index of in ibv_devinfo -v */
    int  dev_index;          /* device index of in ibv_devinfo */

    char *sock_port;         /* socket port number */

    struct rte_mempool *mempool;
    void *rte_mr; // TODO: save a list of registered MRs in rte_mempool
}__attribute__((aligned(64)));

extern struct ConfigInfo config_info;

int  parse_config_file   (char *fname);
void destroy_config_info ();
int parse_benchmark_cfg (char *cfg_file, struct ConfigInfo *config);
void print_benchmark_cfg (struct ConfigInfo *config);
void print_config_info ();

#endif /* CONFIG_H_*/

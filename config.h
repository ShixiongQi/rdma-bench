#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>
#include <inttypes.h>

#define USE_RTE_MEMPOOL 1

enum ConfigFileAttr {
    ATTR_SERVERS = 1,
    ATTR_CLIENTS,
    ATTR_MSG_SIZE,
    ATTR_NUM_CONCURR_MSGS,
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

    int  msg_size;           /* the size of each echo message */
    int  num_concurr_msgs;   /* the number of messages can be sent concurrently */
    int  sgid_index;         /* local GID index of in ibv_devinfo -v */
    int  dev_index;          /* device index of in ibv_devinfo */

    char *sock_port;         /* socket port number */

    struct rte_mempool *mempool;
    void *rte_mr; // TODO: save a list of registered MRs in rte_mempool
}__attribute__((aligned(64)));

extern struct ConfigInfo config_info;

int  parse_config_file   (char *fname);
void destroy_config_info ();

void print_config_info ();

#endif /* CONFIG_H_*/

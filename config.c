#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>

#include "debug.h"
#include "config.h"

struct ConfigInfo config_info;

int benchmark_type = 0;

/* remove space, tab and line return from the line */
void clean_up_line (char *line)
{
    char *i = line;
    char *j = line;

    while (*j != 0) {
        *i = *j;
        j += 1;
        if (*i != ' ' && *i != '\t' && *i != '\r' && *i != '\n') {
            i += 1;
        }
    }
    *i = 0;
}

int parse_node_list (char *line, char ***hosts) {
    int numHosts = 0;

    // Create a temporary copy of the line
    char *lineCopy = strdup(line);
    if (lineCopy == NULL) {
        perror("Memory allocation error");
        exit(1);
    }

    // Count the number of hostnames first
    char *token = strtok(line, ",");
    while (token != NULL) {
        if (strlen(token) > 0) { // Check for empty tokens
            numHosts++;
        }
        token = strtok(NULL, ",");
    }

    // Allocate memory for the hostnames
    *hosts = (char **) malloc(numHosts * sizeof(char *));
    if (*hosts == NULL) {
        perror("Memory allocation error");
        exit(1);
    }

    // Reset the temporary line copy
    strcpy(line, lineCopy);

    // Copy hostnames to the hosts array
    token = strtok(line, ",");
    int index = 0;
    while (token != NULL) {
        if (strlen(token) > 0) { // Check for empty tokens
            (*hosts)[index] = strdup(token);
            if ((*hosts)[index] == NULL) {
                perror("Memory allocation error");
                exit(1);
            }
            index++;
        }
        token = strtok(NULL, ",");
    }

    // Free the temporary copy of the line
    free(lineCopy);

    return numHosts;
}

int get_rank () {
    int	     ret         = 0;
    uint32_t i           = 0;
    uint32_t num_servers = config_info.num_servers;
    uint32_t num_clients = config_info.num_clients;
    struct   utsname utsname_buf;
    char     hostname[64];

    /* get hostname */
    ret = uname (&utsname_buf);
    check(ret == 0, "Failed to call uname");

    strncpy (hostname, utsname_buf.nodename, sizeof(hostname));

    config_info.rank = -1;
    for (i = 0; i < num_servers; i++) {
        if (strstr(hostname, config_info.servers[i])) {
            config_info.rank      = i;
            break;
        }
    }

    for (i = 0; i < num_clients; i++) {
        if (strstr(hostname, config_info.clients[i])) {
            config_info.rank      = i;
            break;
        }
    }
    check(config_info.rank >= 0, "Failed to get rank for node: %s", hostname);

    return 0;
 error:
    return -1;
}
int parse_benchmark_cfg (char *cfg_file) {
    config_t config;
    int ret = 0;
    char hostname[MAX_HOSTNAME_LEN];
    int is_hostname_matched = 0;
    const char* name;
    const char* node_hostname;
    config_setting_t *nodes = NULL;
    config_setting_t *node = NULL;
    config_setting_t *peers = NULL;

    if (unlikely(gethostname(hostname, MAX_HOSTNAME_LEN) == -1)) {
        log_error("gethostname failt");
        goto error_1;
    }

    ret = config_read_file(&config, cfg_file);
    if (unlikely(ret == CONFIG_FALSE)) {
        log_error("parse_benchmark_cfg() error: line %d: %s",
                config_error_line(&config), config_error_text(&config));
        goto error_1;
    }

    ret = config_lookup_string(&config, "name", &name);
    if (unlikely(ret == CONFIG_FALSE)) {
        /* TODO: Error message */
        goto error_1;
    }

    strcpy(config_info.name, name);

    ret = config_lookup_int(&config, "num_concurr_msgs", &config_info.num_concurr_msgs);
    if (unlikely(ret == CONFIG_FALSE)) {
        log_error("parse_benchmark_cfg() error: ");
        goto error_1;
    }

    ret = config_lookup_int(&config, "msg_size", &config_info.msg_size);
    if (unlikely(ret == CONFIG_FALSE)) {
        log_error("parse_benchmark_cfg() error: ");
        goto error_1;
    }

    nodes = config_lookup(&config, "nodes");
    if (unlikely(nodes == NULL)) {
        goto error_1;
    }

    ret = config_setting_is_list(nodes);
    if (unlikely(ret == CONFIG_FALSE)) {
        goto error_1;
    }

    config_info.n_nodes = config_setting_length(nodes);

    for(int i = 0; i < config_info.n_nodes; i++) {
        node = config_setting_get_elem(nodes, i);
        // Get the node id
        ret = config_setting_lookup_int(node, "id", &config_info.nodes[i].id);
        if (unlikely(ret == CONFIG_FALSE)) {
            goto error_1;
        }

        ret = config_setting_lookup_string(node, "hostname", &node_hostname);
        if (unlikely(ret == CONFIG_FALSE)) {
            goto error_1;
        }
        strcpy(config_info.nodes[i].hostname, node_hostname);
        if (strcmp(hostname, config_info.nodes[i].hostname) == 0) {
            config_info.current_node_idx = i;
            is_hostname_matched = 1;
            log_info("Hostnames match: %s, node index: %u", node_hostname, i);
        } else {
            log_debug("Hostnames do not match. Got: %s, Expected: %s", node_hostname, hostname);
        }

        // Get the peers array
        peers = config_setting_lookup(node, "peers");
        if (unlikely(peers == NULL)) {
            goto error_1;
        }

        ret = config_setting_is_array(peers);
        if (unlikely(ret == CONFIG_FALSE)) {
            goto error_1;
        }

        config_info.nodes[i].n_peers = config_setting_length(peers);

        for (int j = 0; j < config_info.nodes[i].n_peers; j++) {
            config_info.nodes[i].peers[j] = config_setting_get_int_elem(peers, j);
        }
    }
    if(unlikely(!is_hostname_matched)) {
        log_error("hostname not matched");
        goto error_1;
    }

    ret = config_lookup_int(&config, "benchmark_type", &benchmark_type);
    if (unlikely(ret == CONFIG_FALSE)) {
        log_error("parse_benchmark_cfg() error: ");
        goto error_1;
    }

    config_info.benchmark_type = benchmark_type;

    return 0;

error_1:
    config_destroy(&config);
    return -1;

}

int parse_config_file (char *fname)
{
    int ret = 0;
    FILE *fp = NULL;
    char line[256] = {'\0'};
    int  attr = 0;

    fp = fopen (fname, "r");
    check(fp != NULL, "Failed to open config file %s", fname);

    while (fgets(line, 256, fp) != NULL) {
        // skip comments
        if (strstr(line, "#") != NULL) {
            continue;
        }

        clean_up_line (line);

        if (strstr (line, "servers:")) {
            attr = ATTR_SERVERS;
            continue;
        } else if (strstr (line, "clients:")) {
            attr = ATTR_CLIENTS;
            continue;
        } else if (strstr (line, "msg_size:")) {
            attr = ATTR_MSG_SIZE;
            continue;
        } else if (strstr (line, "num_concurr_msgs:")) {
            attr = ATTR_NUM_CONCURR_MSGS;
            continue;
        } else if (strstr (line, "benchmark_type:")) {
            attr = ATTR_BENCHMARK_TYPE;
            continue;
        }

        if (attr == ATTR_SERVERS) {
            ret = parse_node_list (line, &config_info.servers);
            check(ret > 0, "Failed to get server list");
            config_info.num_servers = ret;
        } else if (attr == ATTR_CLIENTS) {
            ret = parse_node_list (line, &config_info.clients);
            check(ret > 0, "Failed to get client list");
            config_info.num_clients = ret;
        } else if (attr == ATTR_MSG_SIZE) {
            config_info.msg_size = atoi(line);
            check(config_info.msg_size > 0,
                   "Invalid Value: msg_size = %d",
                   config_info.msg_size);
        } else if (attr == ATTR_NUM_CONCURR_MSGS) {
            config_info.num_concurr_msgs = atoi(line);
            check(config_info.num_concurr_msgs > 0,
                   "Invalid Value: num_concurr_msgs = %d",
                   config_info.num_concurr_msgs);
        } else if (attr == ATTR_BENCHMARK_TYPE) {
            benchmark_type = atoi(line);
        }

        attr = 0;
    }

    ret = get_rank ();
    check(ret == 0, "Failed to get rank");

    fclose (fp);

    return 0;

 error:
    if (fp != NULL) {
        fclose (fp);
    }
    return -1;
}

void destroy_config_info ()
{
    int num_servers = config_info.num_servers;
    int num_clients = config_info.num_clients;
    int i;

    if (config_info.servers != NULL) {
        for (i = 0; i < num_servers; i++) {
            if (config_info.servers[i] != NULL) {
                free (config_info.servers[i]);
            }
        }
        free (config_info.servers);
    }

    if (config_info.clients != NULL) {
        for (i = 0; i < num_clients; i++) {
            if (config_info.clients[i] != NULL) {
                free (config_info.clients[i]);
            }
        }
        free (config_info.clients);
    }
}

void print_config_info () {
    log (LOG_SUB_HEADER, "Configuraion");

    if (config_info.is_server) {
        log ("is_server = %s", "true");
    } else if (config_info.is_client) {
        log ("is_client = %s", "true");
    } else {
        perror("Not server or client");
        exit(1);
    }

    log ("rank                      = %d", config_info.rank);
    log ("msg_size                  = %d", config_info.msg_size);
    log ("num_concurr_msgs          = %d", config_info.num_concurr_msgs);
    log ("sock_port                 = %s", config_info.sock_port);
    
    log (LOG_SUB_HEADER, "End of Configuraion");
}

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>

#include "debug.h"
#include "config.h"

struct ConfigInfo config_info;

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
    check (ret == 0, "Failed to call uname");

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
    check (config_info.rank >= 0, "Failed to get rank for node: %s", hostname);

    return 0;
 error:
    return -1;
}

int parse_config_file (char *fname)
{
    int ret = 0;
    FILE *fp = NULL;
    char line[256] = {'\0'};
    int  attr = 0;

    fp = fopen (fname, "r");
    check (fp != NULL, "Failed to open config file %s", fname);

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
        }

        if (attr == ATTR_SERVERS) {
            ret = parse_node_list (line, &config_info.servers);
            check (ret > 0, "Failed to get server list");
            config_info.num_servers = ret;
        } else if (attr == ATTR_CLIENTS) {
            ret = parse_node_list (line, &config_info.clients);
            check (ret > 0, "Failed to get client list");
            config_info.num_clients = ret;
        } else if (attr == ATTR_MSG_SIZE) {
            config_info.msg_size = atoi(line);
            check (config_info.msg_size > 0,
                   "Invalid Value: msg_size = %d",
                   config_info.msg_size);
        } else if (attr == ATTR_NUM_CONCURR_MSGS) {
            config_info.num_concurr_msgs = atoi(line);
            check (config_info.num_concurr_msgs > 0,
                   "Invalid Value: num_concurr_msgs = %d",
                   config_info.num_concurr_msgs);
        }

        attr = 0;
    }

    ret = get_rank ();
    check (ret == 0, "Failed to get rank");

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

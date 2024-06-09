#include <stdio.h>

#include <rte_branch_prediction.h>
#include <rte_eal.h>
#include <rte_errno.h>

#include "debug.h"
#include "config.h"
#include "ib.h"
#include "setup_ib.h"
#include "client.h"
#include "server.h"

FILE *log_fp = NULL;

int  init_env    ();
void destroy_env ();

int main (int argc, char *argv[])
{
    int	ret = 0;

#ifdef USE_RTE_MEMPOOL
	ret = rte_eal_init(argc, argv);
	if (unlikely(ret == -1)) {
		fprintf(stderr, "rte_eal_init() error: %s\n",
		        rte_strerror(rte_errno));
	    return 1;
	}

	argc -= ret;
	argv += ret;
#endif

    if (argc != 4) {
#ifdef USE_RTE_MEMPOOL
        printf("Usage: %s l 0 --file-prefix=$UNIQUE_NAME --proc-type=primary --no-telemetry --no-pci -- config_file sock_port is_server | is_client\n", argv[0]);
#else
        printf("Usage: %s config_file sock_port is_server | is_client\n", argv[0]);
#endif
        return 0;
    }

    ret = parse_config_file (argv[1]);
    check (ret == 0, "Failed to parse config file");
    config_info.sock_port = argv[2];

    if (strstr("is_server", argv[3])) {
        config_info.is_server = true;
        config_info.is_client = false;
    } else if (strstr("is_client", argv[3])) {
        config_info.is_server = false;
        config_info.is_client = true;
    } else {
#ifdef USE_RTE_MEMPOOL
        printf("Usage: %s l 0 --file-prefix=$UNIQUE_NAME --proc-type=primary --no-telemetry --no-pci -- config_file sock_port is_server | is_client\n", argv[0]);
#else
        printf("Usage: %s config_file sock_port is_server | is_client\n", argv[0]);
#endif
        return 0;
    }

    ret = init_env ();
    check (ret == 0, "Failed to init env");

    ret = setup_ib ();
    check (ret == 0, "Failed to setup IB");

    if (config_info.is_server) {
        printf("Running Server...\n");
        ret = run_server ();
    } else {
        printf("Running Client...\n");
        ret = run_client ();
    }
    check (ret == 0, "Failed to run workload");

 error:
    close_ib_connection ();
    destroy_env         ();
#ifdef USE_RTE_MEMPOOL
    rte_eal_cleanup();
#endif
    return ret;
}    

int init_env ()
{
    char fname[64] = {'\0'};

    if (config_info.is_server) {
        sprintf (fname, "server-%d.log", config_info.rank);
    } else {
        sprintf (fname, "client-%d.log", config_info.rank);
    }
    log_fp = fopen (fname, "w");
    check (log_fp != NULL, "Failed to open log file");

    log (LOG_HEADER, "IB Echo Server");
    print_config_info ();

    return 0;
 error:
    return -1;
}

void destroy_env ()
{
    log (LOG_HEADER, "Run Finished");
    if (log_fp != NULL) {
        fclose (log_fp);
    }
}

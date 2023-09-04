#include <stdio.h>

#include "debug.h"
#include "config.h"
#include "ib.h"
#include "setup_ib.h"
#include "client.h"
#include "server.h"

FILE	*log_fp	     = NULL;

int	init_env    ();
void	destroy_env ();

int main (int argc, char *argv[])
{
    int	ret = 0;

    if (argc != 4) {
        printf ("Usage: %s config_file sock_port is_server|is_client\n", argv[0]);
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
        printf ("Usage: %s config_file sock_port is_server|is_client\n", argv[0]);
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

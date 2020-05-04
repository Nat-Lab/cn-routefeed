#include "feeder.h"
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>

using namespace cnrf;

static Feeder *feeder = nullptr;

void handle_sig(__attribute__((unused)) int sig) {
    fprintf(stderr, "Got SIGINT/SIGTERM, stopping...\n");
    if (feeder != nullptr) feeder->stop();
}

void help() {
    fprintf(stderr, "usage: routefeed [-l HOST] [-p PORT] [-t INTERVAL] -a ASN -i BGP_ID -n NEXTHOP\n");
    fprintf(stderr, "                 [-c CONFIG] [-v]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "cn-routefeed is a BGP speaker that feeds all China IPv4 delegations to peer. \n");
    fprintf(stderr, "Delegation information is fetch from APNIC.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "required arguments:\n");
    fprintf(stderr, "  -a ASN       Local ASN of the BGP speaker.\n");
    fprintf(stderr, "  -i BGP_ID    BGP ID (Router ID) of the BGP speaker.\n");
    fprintf(stderr, "  -n NEXTHOP   Nexthop to use when sending routes.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "optional arguments:\n");
    fprintf(stderr, "  -l HOST      IP address to bind the BGP speaker on. (default: 0.0.0.0)\n");
    fprintf(stderr, "  -p PORT      TCP port number to bind the BGP speaker on. (default: 179)\n");
    fprintf(stderr, "  -t INTERVAL  Time in second to wait between fetching update from APNIC.\n");
    fprintf(stderr, "               (default: 86400)\n");
    fprintf(stderr, "  -c CONFIG    Read command line arguments from file.\n");
    fprintf(stderr, "  -v           Enable debug mode (verbose).\n");
}

void free_argv(int argc, char **argv) {
    if (argv == NULL || argc == 0) return;
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }

    free(argv);
}

int parse_config(int argc, char **argv, FeederConfiguration &config) {
    if (argv == NULL || argc == 0) return 0;

    for (int i = 0; i < argc; ++i) {
        if (i + 1 < argc && strncmp("-l", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            if (inet_pton(AF_INET, argv[++i], &config.host) != 1) {
                fprintf(stderr, "invalid host: \"%s\".\n", argv[i]);
                return 1;
            }
        } else if (i + 1 < argc && strncmp("-p", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            config.port = atoi(argv[++i]);
        } else if (i + 1 < argc && strncmp("-a", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            config.my_asn = atoi(argv[++i]);
        } else if (i + 1 < argc && strncmp("-i", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            if (inet_pton(AF_INET, argv[++i], &config.bgp_id) != 1) {
                fprintf(stderr, "invalid bgp_id: \"%s\".\n", argv[i]);
                return 1;
            }
        } else if (i + 1 < argc && strncmp("-n", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            if (inet_pton(AF_INET, argv[++i], &config.nexthop) != 1) {
                fprintf(stderr, "invalid nexthop: \"%s\".\n", argv[i]);
                return 1;
            }
        } else if (i + 1 < argc && strncmp("-t", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            config.update_interval = atoi(argv[++i]);
        } else if (i + 1 < argc && strncmp("-c", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            
            FILE *fp = fopen(argv[++i], "r");
            if(!fp) {
                fprintf(stderr, "can't open config file: \"%s\".\n", argv[i]);
                return 1;
            }

            fseek(fp, 0L, SEEK_END);
            long sz = ftell(fp);
            rewind(fp);

            char *buf = (char *) malloc(sz+1);

            fread(buf, sz, 1, fp);
            fclose(fp);

            char **new_argv = NULL, *cur;
            int new_argc = 0;
            while ((cur = strsep(&buf, " \n\t")) != NULL) {
                if (new_argc == 0) {
                    new_argv = (char **) malloc(sizeof(char *));
                } else {
                    new_argv = (char **) realloc(new_argv, (new_argc + 1) * sizeof(char *));
                }

                new_argv[new_argc] = (char *) malloc(strlen(cur) + 1);
                strcpy(new_argv[new_argc], cur);
                ++new_argc;
            };

            free(buf);

            if (parse_config(new_argc, new_argv, config) == 1) {
                fprintf(stderr, "error parsing configuration.\n");
                free_argv(new_argc, new_argv);
                return 1;
            }
            free_argv(new_argc, new_argv);

        } else if (strncmp("-v", argv[i], 2) == 0 && strlen(argv[i]) == 2) {
            config.verbose = true;
        } else {
            fprintf(stderr, "unknow command line argument: \"%s\".\n", argv[i]);
            return 1;
        }
    }

    return 0;
}

int main (int argc, char **argv) {
    FeederConfiguration config;

    if (parse_config(argc - 1, argv + 1, config) == 1) {
        help();
        return 1;
    }

    if (config.my_asn == 0 || config.nexthop == 0 || config.bgp_id == 0) {
        help();
        return 1;
    }

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    Feeder feeder(config); 
    ::feeder = &feeder;

    feeder.start();
    feeder.join();   

    return 0;
}
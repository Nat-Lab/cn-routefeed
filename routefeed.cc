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
    fprintf(stderr, "                 [-v]\n");
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
    fprintf(stderr, "  -v           Enable debug mode (verbose).\n");


}

int main (int argc, char **argv) {
    in_addr_t host = INADDR_ANY;
    in_port_t port = 179;
    uint32_t asn = 0;
    uint32_t nexthop = 0;
    uint32_t router_id = 0;
    uint32_t interval = 86400;
    bool verbose = false;

    char opt;

    while ((opt = getopt(argc, argv, "l:p:a:i:n:t:v")) != -1) {
        switch (opt) {
            case 'l': host = inet_addr(optarg); break;
            case 'p': port = atoi(optarg); break;
            case 'a': asn = atoi(optarg); break;
            case 'i': inet_pton(AF_INET, optarg, &router_id); break;
            case 'n': inet_pton(AF_INET, optarg, &nexthop); break;
            case 't': interval = atoi(optarg); break;
            case 'v': verbose = true; break;
            default: 
                help();
                return 1;
        }
    }

    if (asn == 0 || nexthop == 0 || router_id == 0) {
        help();
        return 1;
    }

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    Feeder feeder(asn, router_id, nexthop, interval, host, port, verbose); 
    ::feeder = &feeder;

    feeder.start();
    feeder.join();   

    return 0;
}
#include "feeder.h"
#include <arpa/inet.h>

using namespace cnrf;

static Feeder *feeder = nullptr;

void handle_sig(__attribute__((unused)) int sig) {
    fprintf(stderr, "Got SIGINT/SIGTERM, stopping...\n");
    if (feeder != nullptr) feeder->stop();
}

int main () {
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    uint32_t router_id;
    uint32_t nexthop;
    inet_pton(AF_INET, "172.31.0.1", &router_id);
    inet_pton(AF_INET, "172.31.0.1", &nexthop);

    Feeder feeder(65000, router_id, nexthop, 3600); 
    ::feeder = &feeder;

    feeder.start();
    feeder.join();   

    return 0;
}
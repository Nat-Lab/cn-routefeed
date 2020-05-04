#ifndef FEEDER_H
#define FEEDER_H
#include "fetcher.h"
#include <stdint.h>
#include <libbgp/bgp-fsm.h>
#include <thread>
#include <vector>
#include <mutex>

namespace cnrf {

struct FeederConfiguration {
    uint32_t my_asn = 0;
    uint32_t bgp_id = 0;
    uint32_t nexthop = 0;
    uint32_t update_interval = 86400;
    in_addr_t host = INADDR_ANY;
    in_port_t port = 179;
    bool verbose = false;
};

class Feeder {
public:
    Feeder(const FeederConfiguration &feder_config);
    bool start();
    void stop();
    void join();

private:
    void tick();
    void handleAccept();
    void handleSession(const char *peer_addr, int fd);

    int fd_sock;
    bool running;
    in_addr_t host;
    in_port_t port;

    libbgp::BgpLogHandler logger;
    libbgp::BgpLogHandler fsm_logger;
    libbgp::BgpConfig config_template;
    libbgp::BgpRib4 rib;
    libbgp::RouteEventBus bus;

    std::mutex list_mtx;
    std::vector<libbgp::BgpFsm*> fsms;
    std::vector<std::thread> threads;
    //std::vector<int> client_fds;

    Fetcher fetcher;

    uint32_t update_interval;
};

}

#endif // FEEDER_H
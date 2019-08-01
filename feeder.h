#ifndef FEEDER_H
#define FEEDER_H
#include "fetcher.h"
#include <stdint.h>
#include <libbgp/bgp-fsm.h>
#include <thread>
#include <vector>
#include <mutex>

namespace cnrf {

class Feeder {
public:
    Feeder(uint32_t my_asn, uint32_t bgp_id, uint32_t nexthop, uint32_t update_interval);
    bool start();
    void stop();
    void join();

private:
    void tick();
    void handleAccept();
    void handleSession(const char *peer_addr, int fd);

    int fd_sock;
    bool running;

    libbgp::BgpLogHandler logger;
    libbgp::BgpConfig config_template;
    libbgp::BgpRib4 rib;
    libbgp::RouteEventBus bus;

    std::mutex list_mtx;
    std::vector<libbgp::BgpFsm*> fsms;
    std::vector<std::thread> threads;
    std::vector<int> client_fds;

    Fetcher fetcher;

    uint32_t update_interval;
};

}

#endif // FEEDER_H
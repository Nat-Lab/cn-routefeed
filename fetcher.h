#ifndef FETCHER_H
#define FETCHER_H
#include <utility>
#include <vector>
#include <algorithm>
#include <stdint.h>
#include <libbgp/bgp-rib4.h>
#include <curl/curl.h>

namespace cnrf {

class Fetcher {
public:
    Fetcher(libbgp::BgpRib4 *rib, libbgp::RouteEventBus *rev_bus, uint32_t nexthop);
    ~Fetcher();
    bool updateRib();
    bool getRoutesDiff(std::vector<libbgp::Prefix4> &added, std::vector<libbgp::Prefix4> &dropped);
    size_t handleRead(const char* buffer, size_t size);

private:
    char line_buffer[256];
    char read_buffer[65536];
    size_t buffer_left;
    uint32_t nexthop;

    std::vector<libbgp::Prefix4> last_allocs;
    std::vector<libbgp::Prefix4> cur_allocs;
    libbgp::BgpRib4 *rib;
    libbgp::RouteEventBus *rev_bus;
    libbgp::BgpLogHandler logger;

    CURL *curl;
};

}

#endif // FETCHER_H
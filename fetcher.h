#ifndef FETCHER_H
#define FETCHER_H
#include <utility>
#include <vector>
#include <algorithm>
#include <stdint.h>
#include <libbgp/prefix4.h>
#include <curl/curl.h>

namespace cnrf {

class Fetcher {
public:
    Fetcher();
    ~Fetcher();
    bool getRoutesDiff(std::vector<libbgp::Prefix4> &added, std::vector<libbgp::Prefix4> &dropped);
    size_t handleRead(const char* buffer, size_t size);

private:
    char line_buffer[256];
    char *read_buffer;
    size_t buffer_left;

    std::vector<libbgp::Prefix4> last_allocs;
    std::vector<libbgp::Prefix4> cur_allocs;

    CURL *curl;
};

}

#endif // FETCHER_H
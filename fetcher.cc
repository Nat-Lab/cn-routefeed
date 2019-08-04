#include <stdlib.h>
#include <string.h>
#include "fetcher.h"
#include "math.h"
#define DELEGATE_DB "http://ftp.apnic.net/apnic/stats/apnic/delegated-apnic-latest"

namespace cnrf {

static size_t do_write(void *ptr, size_t size, size_t nmemb, Fetcher *fetcher) {
    return fetcher->handleRead((char *) ptr, size * nmemb);
}

Fetcher::Fetcher(libbgp::BgpRib4 *rib, libbgp::RouteEventBus *rev_bus, uint32_t nexthop) {
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, DELEGATE_DB);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 65535L);
    this->rib = rib;
    this->rev_bus = rev_bus;
    this->nexthop = nexthop;
    buffer_left = 0;
}

Fetcher::~Fetcher() {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
}

size_t Fetcher::handleRead(const char* buffer, size_t size) {
    size_t buf_sz = size;

    if (buffer_left != 0) {
        memcpy(read_buffer + buffer_left, buffer, size);
        buf_sz = size + buffer_left;
    }

    const char *cur = buffer_left != 0 ? read_buffer : buffer;
    const char *ptr = cur;
    size_t read = 0;

    while (read < buf_sz) {
        if (*ptr != '\n' || *ptr == 0) {
            ptr++;
            read++;
        } else {
            if (*cur != '#') {
                size_t line_len = ptr - cur;
                if (line_len > 128) throw "invalid_line_len";
                memcpy(line_buffer, cur, line_len);
                line_buffer[line_len] = 0;

                char *rest = line_buffer;

                (void) strtok_r(rest, "|", &rest); // skip first field
                char *cc = strtok_r(rest, "|", &rest);
                if (strncmp("CN", cc, 2) != 0) goto nextline;
                char *type = strtok_r(rest, "|", &rest);
                if (strncmp("ipv4", type, 4) != 0) goto nextline;
                char *prefix = strtok_r(rest, "|", &rest);
                uint8_t length = 32 - log(atoi(strtok_r(rest, "|", &rest)))/log(2);
                cur_allocs.push_back(libbgp::Prefix4(prefix, length));

                size_t cur_sz = cur_allocs.size();
                if (cur_sz % 1000 == 0 && cur_sz != 0) {
                    logger.log(libbgp::INFO, "Fetcher::handleRead: updating: %zu delegations loaded.\n", cur_sz);
                }
            }

nextline:
            cur = ptr + 1;
            ptr++;
            read++;
        }
    }

    buffer_left = ptr - cur;

    if (buffer_left > 0) {
        memcpy(read_buffer, cur, buffer_left);
        read_buffer[buffer_left] = 0;
    }

    return size;
}

bool Fetcher::updateRib() {
    std::vector<libbgp::Prefix4> added;
    std::vector<libbgp::Prefix4> dropped;
    cur_allocs.clear();
    long response_code;
    double elapsed;
    logger.log(libbgp::INFO, "Fetcher::updateRib: fetching latest delegations...\n");
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &elapsed);
    
    if (response_code == 200) {
        logger.log(libbgp::INFO, "Fetcher::updateRib: update fetch completed in %f seconds, number of delegations: %zu\n", elapsed, cur_allocs.size());

        //std::set_difference(cur_allocs.begin(), cur_allocs.end(), last_allocs.begin(), last_allocs.end(), std::inserter(added, added.begin()));
        //std::set_difference(last_allocs.begin(), last_allocs.end(), cur_allocs.begin(), cur_allocs.end(), std::inserter(dropped, dropped.begin()));

        logger.log(libbgp::INFO, "Fetcher::updateRib: computing diffs...\n");

        for (const libbgp::Prefix4 &prefix : cur_allocs) {
            if (std::find(last_allocs.begin(), last_allocs.end(), prefix) == last_allocs.end()) {
                added.push_back(prefix);
            }
        }

        for (const libbgp::Prefix4 &prefix : last_allocs) {
            if (std::find(cur_allocs.begin(), cur_allocs.end(), prefix) == cur_allocs.end()) {
                dropped.push_back(prefix);
            }
        }

        logger.log(libbgp::INFO, "Fetcher::updateRib: diff: %zu routes added, %zu routes dropped.\n", added.size(), dropped.size());

        last_allocs = cur_allocs;
        rib->insert(&logger, added, nexthop, rev_bus);
        rib->withdraw(0, dropped, rev_bus);
    } else {
        logger.log(libbgp::WARN, "Fetcher::updateRib: failed to fetch delegations: HTTP %ld.\n", response_code);
        return false;
    }
    
    return true;
}

}
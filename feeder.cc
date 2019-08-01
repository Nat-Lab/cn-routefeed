#include "feeder.h"
#include <string.h>
#include <libbgp/fd-out-handler.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <chrono>

namespace cnrf {

Feeder::Feeder(uint32_t my_asn, uint32_t bgp_id, uint32_t nexthop, uint32_t update_interval) : 
    rib(&logger), fetcher(&rib, &bus, nexthop) {
    logger.setLogLevel(libbgp::WARN);
    running = false;
    config_template.asn = my_asn;
    config_template.default_nexthop4 = nexthop;
    config_template.forced_default_nexthop4 = true;
    config_template.in_filters4 = libbgp::BgpFilterRules(libbgp::REJECT);
    config_template.log_handler = &logger;
    config_template.peer_asn = 0;
    config_template.rib4 = &rib;
    config_template.router_id = bgp_id;
    this->update_interval = update_interval;
}

bool Feeder::start() {
    int fd_sock = -1, fd_conn = -1;
    struct sockaddr_in server_addr, client_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(179);

    fd_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (fd_sock < 0) {
        logger.log(libbgp::FATAL, "socket(): %s\n", strerror(errno));
        return false;
    }

    int bind_ret = bind(fd_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));

    if (bind_ret < 0) {
        logger.log(libbgp::FATAL, "bind(): %s\n", strerror(errno));
        close(fd_sock);
        return false;
    }

    int listen_ret = listen(fd_sock, 1);

    if (listen_ret < 0) {
        logger.log(libbgp::FATAL, "listen(): %s\n", strerror(errno));
        close(fd_sock);
        return false;
    }

    logger.log(libbgp::INFO, "waiting for any client...\n");

    socklen_t caddr_len = sizeof(client_addr);

    char client_addr_str[INET_ADDRSTRLEN];

    std::thread ticker_thread(&Feeder::tick, this);
    ticker_thread.detach();

    running = true;
    
    while (true) {
        fd_conn = accept(fd_sock, (struct sockaddr *) &client_addr, &caddr_len);

        if (fd_conn < 0) {
            logger.log(libbgp::ERROR, "accept(): %s\n", strerror(errno));
            close(fd_conn);
            continue;
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), client_addr_str, INET_ADDRSTRLEN);

        logger.log(libbgp::INFO, "accept(): new client from %s.\n", client_addr_str);

        std::thread handler_thread(&Feeder::handleSession, this, client_addr_str, fd_conn);
        handler_thread.detach();
    }
}

void Feeder::stop() {

}

void Feeder::join() {

}

void Feeder::tick() {
    uint32_t time = 0;

    while (running) {
        time %= update_interval;
        if (time == 0) fetcher.updateRib();
        time++;
        fsm_list_mtx.lock();
        for (libbgp::BgpFsm *fsm : fsms) fsm->tick();
        fsm_list_mtx.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Feeder::handleSession(const char *peer_addr, int fd) {
    uint8_t this_buffer[4096];
    libbgp::FdOutHandler this_out(fd);
    libbgp::BgpConfig this_config(config_template);
    this_config.out_handler = &this_out;
    libbgp::BgpFsm this_fsm(this_config);

    ssize_t read_ret = -1;

    fsm_list_mtx.lock();
    fsms.push_back(&this_fsm);
    fsm_list_mtx.unlock();

    logger.log(libbgp::INFO, "%zu routes in rib.\n", this_config.rib4->get().size());

    while ((read_ret = read(fd, this_buffer, 4096)) > 0) {
        int fsm_ret = this_fsm.run(this_buffer, (size_t) read_ret);

        // ret 0: fatal error/reset by peer.
        // ret 2: notification sent to peer
        if (fsm_ret <= 0 || fsm_ret == 2) break;
    }

    close(fd);

    this_fsm.resetHard();

    fsm_list_mtx.lock();
    for (std::vector<libbgp::BgpFsm *>::const_iterator iter = fsms.begin(); iter != fsms.end(); iter++) {
        if (*iter == &this_fsm) {
            fsms.erase(iter);
            break;
        }
    }
    fsm_list_mtx.unlock();
    logger.log(libbgp::INFO, "session with AS%d (%s) closed.\n", this_fsm.getPeerAsn(), peer_addr);
}

}
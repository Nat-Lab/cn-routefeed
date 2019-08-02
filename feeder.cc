#include "feeder.h"
#include <string.h>
#include <libbgp/fd-out-handler.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <utility>
#include <chrono>

namespace cnrf {

Feeder::Feeder(uint32_t my_asn, uint32_t bgp_id, uint32_t nexthop, uint32_t update_interval, in_addr_t host, in_port_t port) : 
    rib(&logger), fetcher(&rib, &bus, nexthop) {
    logger.setLogLevel(libbgp::INFO);
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
    this->host = host;
    this->port = port;
}

bool Feeder::start() {
    fd_sock = -1;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = host;
    server_addr.sin_port = htons(port);

    fd_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (fd_sock < 0) {
        logger.log(libbgp::FATAL, "Feeder::start: socket(): %s\n", strerror(errno));
        return false;
    }

    int bind_ret = bind(fd_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));

    if (bind_ret < 0) {
        logger.log(libbgp::FATAL, "Feeder::start: bind(): %s\n", strerror(errno));
        close(fd_sock);
        return false;
    }

    int listen_ret = listen(fd_sock, 1);

    if (listen_ret < 0) {
        logger.log(libbgp::FATAL, "Feeder::start: listen(): %s\n", strerror(errno));
        close(fd_sock);
        return false;
    }

    logger.log(libbgp::INFO, "Feeder::start: ready.\n");

    threads.push_back(std::thread (&Feeder::tick, this));
    threads.push_back(std::thread (&Feeder::handleAccept, this));

    running = true;
    return true;
}

void Feeder::stop() {
    running = false;
    logger.log(libbgp::INFO, "Feeder::stop: shutting down...\n");
    int fd_sock_temp = fd_sock;
    fd_sock = -1;
    shutdown(fd_sock_temp, SHUT_RDWR);
    close(fd_sock_temp);
    for (int fd : client_fds) shutdown(fd, SHUT_RDWR);
}

void Feeder::join() {
    for (std::thread &t : threads) {
        if (t.joinable()) t.join();
    }
}

void Feeder::tick() {
    uint32_t time = 0;

    while (running) {
        time %= update_interval;
        if (time == 0) fetcher.updateRib();
        time++;
        list_mtx.lock();
        for (libbgp::BgpFsm *fsm : fsms) fsm->tick();
        list_mtx.unlock();
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

    list_mtx.lock();
    client_fds.push_back(fd);
    fsms.push_back(&this_fsm);
    list_mtx.unlock();

    while ((read_ret = read(fd, this_buffer, 4096)) > 0) {
        int fsm_ret = this_fsm.run(this_buffer, (size_t) read_ret);
        if (fsm_ret <= 0 || fsm_ret == 2) break;
    }

    this_fsm.resetHard();

    list_mtx.lock();
    for (std::vector<int>::const_iterator iter = client_fds.begin(); iter != client_fds.end(); iter++) {
        if (*iter == fd) {
            client_fds.erase(iter);
            break;
        }
    }
    for (std::vector<libbgp::BgpFsm *>::const_iterator iter = fsms.begin(); iter != fsms.end(); iter++) {
        if (*iter == &this_fsm) {
            fsms.erase(iter);
            break;
        }
    }
    list_mtx.unlock();
    close(fd);
    logger.log(libbgp::INFO, "Feeder::handleSession: session with AS%d (%s) closed.\n", this_fsm.getPeerAsn(), peer_addr);
}

void Feeder::handleAccept() {
    struct sockaddr_in client_addr;
    socklen_t caddr_len = sizeof(client_addr);
    char client_addr_str[INET_ADDRSTRLEN];

    while (running) {
        int fd_conn = accept(fd_sock, (struct sockaddr *) &client_addr, &caddr_len);

        if (fd_sock <= 0) {
            if (running) {
                logger.log(libbgp::FATAL, "Feeder::handleAccept: socket unexpectedly closed, stopping.\n");
                close(fd_sock);
                stop();
                return;
            }
            break;
        }

        if (fd_conn < 0) {
            logger.log(libbgp::ERROR, "Feeder::handleAccept: accept(): %s\n", strerror(errno));
            close(fd_conn);
            continue;
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), client_addr_str, INET_ADDRSTRLEN);

        logger.log(libbgp::INFO, "Feeder::handleAccept: new client from %s.\n", client_addr_str);
        threads.push_back(std::thread (&Feeder::handleSession, this, client_addr_str, fd_conn));
    }

    logger.log(libbgp::INFO, "Feeder::handleAccept: accept handler stopped.\n");
    if (fd_sock > 0) close(fd_sock);
}

}
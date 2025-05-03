#include "../include/http_webhook_server.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#endif

HttpWebhookServer::HttpWebhookServer(uint16_t port, Handler handler)
    : port(port), request_handler(handler) {
    setupSocket();
    setupEpoll();
}

HttpWebhookServer::~HttpWebhookServer() {
    stop();
    if (server_fd != -1) ::close(server_fd);
    if (epoll_fd != -1) ::close(epoll_fd);
}

void HttpWebhookServer::setupSocket() {
    server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        throw std::system_error(errno, std::generic_category());

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (::fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::system_error(errno, std::generic_category());

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        throw std::system_error(errno, std::generic_category());

    if (::listen(server_fd, 128) == -1)
        throw std::system_error(errno, std::generic_category());
}

void HttpWebhookServer::setupEpoll() {
#ifdef __linux__
    epoll_fd = ::epoll_create1(0);
    if (epoll_fd == -1)
        throw std::system_error(errno, std::generic_category());

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = server_fd;

    if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
        throw std::system_error(errno, std::generic_category());

#elif defined(__APPLE__)
    epoll_fd = ::kqueue();
    if (epoll_fd == -1)
        throw std::system_error(errno, std::generic_category());

    struct kevent ev_set;
    EV_SET(&ev_set, server_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (kevent(epoll_fd, &ev_set, 1, nullptr, 0, nullptr) == -1)
        throw std::system_error(errno, std::generic_category());
#endif
}

void HttpWebhookServer::registerFdForRead(int fd) {
#ifdef __linux__
    epoll_event event{};
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    event.data.fd = fd;
    ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
#elif defined(__APPLE__)
    struct kevent ev_set;
    EV_SET(&ev_set, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    kevent(epoll_fd, &ev_set, 1, nullptr, 0, nullptr);
#endif
}

void HttpWebhookServer::modifyFdToWrite(int fd) {
#ifdef __linux__
    epoll_event event{};
    event.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
    event.data.fd = fd;
    ::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
#elif defined(__APPLE__)
    struct kevent ev_set[2];
    EV_SET(&ev_set[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&ev_set[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    kevent(epoll_fd, ev_set, 2, nullptr, 0, nullptr);
#endif
}

void HttpWebhookServer::start() {
    running = true;

#ifdef __linux__
    epoll_event events[64];
#elif defined(__APPLE__)
    struct kevent events[64];
#endif

    while (running) {
#ifdef __linux__
        int nfds = ::epoll_wait(epoll_fd, events, 64, -1);
#elif defined(__APPLE__)
        int nfds = ::kevent(epoll_fd, nullptr, 0, events, 64, nullptr);
#endif
        if (nfds == -1 && errno != EINTR)
            throw std::system_error(errno, std::generic_category());

        for (int i = 0; i < nfds; ++i) {
#ifdef __linux__
            int fd = events[i].data.fd;
            bool isWritable = events[i].events & EPOLLOUT;
#elif defined(__APPLE__)
            int fd = static_cast<int>(events[i].ident);
            bool isWritable = events[i].filter == EVFILT_WRITE;
#endif

            if (fd == server_fd) {
                while (true) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);

#ifdef __linux__
                    int client_fd = ::accept4(server_fd, (sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK);
#elif defined(__APPLE__)
                    int client_fd = ::accept(server_fd, (sockaddr*)&client_addr, &client_len);
                    if (client_fd != -1) {
                        int flags = fcntl(client_fd, F_GETFL, 0);
                        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                    }
#endif
                    if (client_fd == -1) break;

                    clients[client_fd] = ClientContext{};
                    registerFdForRead(client_fd);
                }
            } else if (isWritable) {
                sendToClient(fd);
            } else {
                handleClient(fd);
            }
        }
    }
}

void HttpWebhookServer::stop() {
    running = false;
}

void HttpWebhookServer::closeClient(int fd) {
#ifdef __linux__
    ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
#elif defined(__APPLE__)
    struct kevent ev_del[2];
    EV_SET(&ev_del[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&ev_del[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(epoll_fd, ev_del, 2, nullptr, 0, nullptr);
#endif
    ::close(fd);
    clients.erase(fd);
}

void HttpWebhookServer::sendToClient(int fd) {
    if (!clients.count(fd)) return;

    auto& ctx = clients[fd];
    if (!ctx.output_buffer.empty()) {
        ssize_t sent = ::send(fd,
                              ctx.output_buffer.data() + ctx.bytes_written,
                              ctx.output_buffer.size() - ctx.bytes_written,
                              MSG_DONTWAIT);

        if (sent > 0) ctx.bytes_written += sent;
        if (ctx.bytes_written == ctx.output_buffer.size()) {
            closeClient(fd);
        }
    }
}

void HttpWebhookServer::handleClient(int fd) {
    char buffer[4096];
    ssize_t count = ::recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);

    if (count > 0) {
        auto& ctx = clients[fd];
        ctx.input_buffer.append(buffer, count);

        if (ctx.input_buffer.find("\r\n\r\n") != std::string::npos) {
            HttpRequest req;
            parseHttpRequest(ctx, req);

            HttpResponse res = request_handler(req);
            buildHttpResponse(res, ctx.output_buffer);

            modifyFdToWrite(fd);
        }
    } else if (count == 0 || (count == -1 && errno != EAGAIN)) {
        closeClient(fd);
        return;
    }
}

void HttpWebhookServer::parseHttpRequest(ClientContext& ctx, HttpRequest& req) {
    std::istringstream stream(ctx.input_buffer);
    std::string line;

    std::getline(stream, line);
    std::istringstream req_line(line);
    req_line >> req.method >> req.path;

    while (std::getline(stream, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            req.headers[key] = value;
        }
    }

    size_t header_end = ctx.input_buffer.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        req.body = ctx.input_buffer.substr(header_end + 4);
        if (req.headers.count("Content-Length")) {
            size_t content_length = std::stoul(req.headers["Content-Length"]);
            req.body = req.body.substr(0, content_length);
        }
    }
}

void HttpWebhookServer::buildHttpResponse(const HttpResponse& res, std::string& output) {
    output = "HTTP/1.1 " + std::to_string(res.status_code) + " OK\r\n";

    for (const auto& [key, value] : res.headers) {
        output += key + ": " + value + "\r\n";
    }

    if (!res.headers.count("Content-Length")) {
        output += "Content-Length: " + std::to_string(res.body.size()) + "\r\n";
    }

    output += "\r\n" + res.body;
}

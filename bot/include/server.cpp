#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <algorithm>  // Nécessaire pour std::remove_if
#include <functional>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#ifdef __APPLE__
#define SOCKET_PATH_MAX 104
#else
#define SOCKET_PATH_MAX 108
#endif

class UnixSocketServer {
public:
    using MessageHandler = std::function<void(const std::string&)>;

    UnixSocketServer(const std::string& path) :
        socket_path_(path.substr(0, SOCKET_PATH_MAX - 1)),
        running_(false) {}

    ~UnixSocketServer() { stop(); }

    void start(MessageHandler handler) {
        if (running_) return;

        server_fd_ = create_socket();
        setup_socket();

        running_ = true;
        server_thread_ = std::thread(&UnixSocketServer::event_loop, this, handler);
    }

    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        cleanup();
    }

    void send(const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            queue_message(client.fd, message);
        }
    }

private:
    struct Client {
        int fd;
        std::queue<std::string> write_queue;
    };

    int create_socket() {
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) throw std::runtime_error("::socket() failed");
        return fd;
    }

    void setup_socket() {
        struct sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        unlink(socket_path_.c_str());

        if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(server_fd_);
            throw std::runtime_error("bind() failed");
        }

        if (listen(server_fd_, 5) == -1) {
            close(server_fd_);
            throw std::runtime_error("listen() failed");
        }

        set_nonblocking(server_fd_);
    }

    void set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    void event_loop(MessageHandler handler) {
        std::vector<struct pollfd> fds;
        fds.push_back({server_fd_, POLLIN, 0});

        while (running_) {
            int ready = ::poll(fds.data(), fds.size(), 250);
            if (ready == -1) break;

            for (size_t i = 0; i < fds.size(); ++i) {
                if (fds[i].revents & POLLIN) {
                    if (fds[i].fd == server_fd_) {
                        accept_new_connection(fds);
                    } else {
                        handle_client_input(fds[i].fd, handler);
                    }
                }

                if (fds[i].revents & POLLOUT) {
                    handle_client_output(fds[i].fd);
                }
            }
        }
    }

    void accept_new_connection(std::vector<struct pollfd>& fds) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd == -1) return;

        set_nonblocking(client_fd);
        fds.push_back({client_fd, POLLIN | POLLOUT, 0});

        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.push_back({client_fd, {}});
    }

    void handle_client_input(int fd, MessageHandler handler) {
        char buffer[4096];
        ssize_t count = recv(fd, buffer, sizeof(buffer), 0);

        if (count > 0) {
            handler(std::string(buffer, count));
        } else {
            remove_client(fd);
        }
    }

    void handle_client_output(int fd) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            if (client.fd == fd && !client.write_queue.empty()) {
                const std::string& msg = client.write_queue.front();
                ssize_t sent = ::send(fd, msg.data(), msg.size(), 0);
                if (sent > 0) {
                    client.write_queue.pop();
                }
            }
        }
    }

    void queue_message(int fd, const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            if (client.fd == fd) {
                client.write_queue.push(message);
                return;
            }
        }
    }

    void remove_client(int fd) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                [fd](const Client& c) { return c.fd == fd; }),
            clients_.end()
        );
        close(fd);
    }

    void cleanup() {
        close(server_fd_);
        unlink(socket_path_.c_str());
    }

    std::string socket_path_;
    int server_fd_ = -1;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::mutex clients_mutex_;
    std::vector<Client> clients_;
};

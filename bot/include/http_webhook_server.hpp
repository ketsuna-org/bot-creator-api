#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <system_error>
#include <sstream>
#include <string>
#include <netinet/in.h>

#ifdef __linux__
    #include <sys/epoll.h>
#elif defined(__APPLE__)
    #include <sys/event.h>
#endif

class HttpWebhookServer {
public:
    struct HttpRequest {
        std::string method;
        std::string path;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    struct HttpResponse {
        int status_code = 200;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    HttpWebhookServer(uint16_t port, Handler handler);
    ~HttpWebhookServer();
    void setupEpoll();
    void handleEvents();
    void start();
    void stop();

private:
    struct ClientContext {
        std::string input_buffer;
        std::string output_buffer;
        size_t bytes_written = 0;
    };

    void setupSocket();
    void setupEventLoop();
    void handleEvent(int fd, uint32_t events);
    void handleClient(int fd);
    void closeClient(int fd);
    void registerFdForRead(int fd);
    void modifyFdToWrite(int fd);
    void sendToClient(int fd);
    void parseHttpRequest(ClientContext& ctx, HttpRequest& req);
    void buildHttpResponse(const HttpResponse& res, std::string& output);

    int server_fd = -1;
    int event_fd = -1;
    int epoll_fd = -1;
    bool running = false;
    uint16_t port;
    Handler request_handler;
    std::unordered_map<int, ClientContext> clients;

#ifdef __linux__
    static constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
#elif defined(__APPLE__)
    static constexpr int MAX_EVENTS = 64;
    struct kevent events[MAX_EVENTS];
#endif
};

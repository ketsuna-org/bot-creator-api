#include <sys/epoll.h>
#include <netinet/in.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <system_error>
#include <sstream>

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

    void start();
    void stop();

private:
    struct ClientContext {
        std::string input_buffer;
        std::string output_buffer;
        size_t bytes_written = 0;
    };

    void setupSocket();
    void setupEpoll();
    void handleEvent(struct epoll_event* event);
    void handleClient(int fd);
    void closeClient(int fd);
    void parseHttpRequest(ClientContext& ctx, HttpRequest& req);
    void buildHttpResponse(const HttpResponse& res, std::string& output);

    int server_fd = -1;
    int epoll_fd = -1;
    bool running = false;
    uint16_t port;
    Handler request_handler;
    std::unordered_map<int, ClientContext> clients;
};

#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <ctime>
#include <atomic>

class Server {
public:
    Server(int port = 8080);
    ~Server();
    
    bool initialize();
    void run();
    void stop();

private:
    int port_;
    int tcp_fd_;
    int udp_fd_;
    int epoll_fd_;
    bool running_;
    
    std::atomic<int> total_clients_;
    std::atomic<int> current_clients_;
    
    bool create_tcp_socket();
    bool create_udp_socket();
    bool setup_epoll();
    void handle_tcp_connection();
    void handle_udp_data();
    void handle_client_data(int client_fd);
    std::string process_command(const std::string& command);
    std::string get_current_time();
    std::string get_stats();
    void close_client(int client_fd);
};

#endif

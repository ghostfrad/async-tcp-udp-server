#include "server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_EVENTS 64
#define BUFFER_SIZE 1024

Server::Server(int port) 
    : port_(port), 
      tcp_fd_(-1), 
      udp_fd_(-1), 
      epoll_fd_(-1),
      running_(false),
      total_clients_(0),
      current_clients_(0) {
}

Server::~Server() {
    stop();
}

bool Server::initialize() {
    signal(SIGPIPE, SIG_IGN);
    
    if (!create_tcp_socket()) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return false;
    }
    
    if (!create_udp_socket()) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return false;
    }
    
    if (!setup_epoll()) {
        std::cerr << "Failed to setup epoll" << std::endl;
        return false;
    }
    
    std::cout << "Server initialized on port " << port_ << std::endl;
    return true;
}

bool Server::create_tcp_socket() {
    tcp_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (tcp_fd_ == -1) {
        return false;
    }
    
    int opt = 1;
    if (setsockopt(tcp_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(tcp_fd_);
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(tcp_fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(tcp_fd_);
        return false;
    }
    
    if (listen(tcp_fd_, SOMAXCONN) == -1) {
        close(tcp_fd_);
        return false;
    }
    
    return true;
}

bool Server::create_udp_socket() {
    udp_fd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (udp_fd_ == -1) {
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(udp_fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(udp_fd_);
        return false;
    }
    
    return true;
}

bool Server::setup_epoll() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        return false;
    }
    
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = tcp_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tcp_fd_, &event) == -1) {
        close(epoll_fd_);
        return false;
    }
    
    event.data.fd = udp_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, udp_fd_, &event) == -1) {
        close(epoll_fd_);
        return false;
    }
    
    return true;
}

void Server::run() {
    running_ = true;
    epoll_event events[MAX_EVENTS];
    
    std::cout << "Server started" << std::endl;
    
    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        
        if (nfds == -1) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == tcp_fd_) {
                handle_tcp_connection();
            } else if (events[i].data.fd == udp_fd_) {
                handle_udp_data();
            } else {
                handle_client_data(events[i].data.fd);
            }
        }
    }
}

void Server::handle_tcp_connection() {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept4(tcp_fd_, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK);
    if (client_fd == -1) {
        return;
    }
    
    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = client_fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event) == -1) {
        close(client_fd);
        return;
    }
    
    total_clients_++;
    current_clients_++;
    
    std::cout << "New TCP connection from " 
              << inet_ntoa(client_addr.sin_addr) << ":" 
              << ntohs(client_addr.sin_port) 
              << " (total: " << total_clients_ 
              << ", current: " << current_clients_ << ")" << std::endl;
}

void Server::handle_udp_data() {
    char buffer[BUFFER_SIZE];
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    ssize_t bytes_received = recvfrom(udp_fd_, buffer, BUFFER_SIZE - 1, 0,
                                     (sockaddr*)&client_addr, &addr_len);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string message(buffer);
        std::string response;
        
        if (message[0] == '/') {
            response = process_command(message);
        } else {
            response = message;
        }
        
        sendto(udp_fd_, response.c_str(), response.length(), 0,
               (sockaddr*)&client_addr, addr_len);
    }
}

void Server::handle_client_data(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        std::string message(buffer);
        std::string response;
        
        if (message[0] == '/') {
            response = process_command(message);
        } else {
            response = message;
        }
        
        if (message.find("/shutdown") == 0) {
            send(client_fd, response.c_str(), response.length(), 0);
            close_client(client_fd);
            stop();
            return;
        }
        
        send(client_fd, response.c_str(), response.length(), 0);
    }
    
    if (bytes_received == 0 || (bytes_received == -1 && errno != EAGAIN)) {
        close_client(client_fd);
    }
}

std::string Server::process_command(const std::string& command) {
    if (command.find("/time") == 0) {
        return get_current_time();
    } else if (command.find("/stats") == 0) {
        return get_stats();
    } else if (command.find("/shutdown") == 0) {
        return "Server shutting down...";
    } else {
        return "Unknown command";
    }
}

std::string Server::get_current_time() {
    time_t now = time(nullptr);
    tm* local_time = localtime(&now);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
    
    return std::string(buffer);
}

std::string Server::get_stats() {
    return "Total clients: " + std::to_string(total_clients_) + 
           ", Current clients: " + std::to_string(current_clients_);
}

void Server::close_client(int client_fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    current_clients_--;
    
    std::cout << "Client disconnected (current: " << current_clients_ << ")" << std::endl;
}

void Server::stop() {
    running_ = false;
    
    if (tcp_fd_ != -1) {
        close(tcp_fd_);
        tcp_fd_ = -1;
    }
    
    if (udp_fd_ != -1) {
        close(udp_fd_);
        udp_fd_ = -1;
    }
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    std::cout << "Server stopped" << std::endl;
}

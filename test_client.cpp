#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to connect" << std::endl;
        close(sock);
        return 1;
    }
    
    std::cout << "Connected to server. Enter commands:" << std::endl;
    
    char buffer[1024];
    while (true) {
        std::cout << "> ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "quit") break;
        
        send(sock, input.c_str(), input.length(), 0);
        
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "Server: " << buffer << std::endl;
        }
    }
    
    close(sock);
    return 0;
}

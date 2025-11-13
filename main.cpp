#include "server.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    int port = 8080;
    
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    Server server(port);
    
    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    server.run();
    
    return 0;
}

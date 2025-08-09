#include "server.h"

int main()
{
    std::cout<<"Enter port: ";
    std::string port;
    std::cin>>port;
    Server server(port);
    server.Init();
     while (true) 
    {
        if (!server.WaitForConnection())
        {
            std::cerr << "Failed to accept client connection. Shutting down server." << std::endl;
            break; 
        }
        while (true)
        {
            if (!server.GetCommandFromClient())
            {
                std::cout << "Client disconnected" << std::endl;
                break; 
            }
        }
        server.DisconnectClient(); 

        std::cout << "Ready for new connection\n" << std::endl;
    }
    server.Shutdown();
    return 0;
}
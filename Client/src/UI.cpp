#include "UI.h"
#include <iomanip>
void DrawGetIPPORT(std::string &ip, std::string &port)
{
    std::cout<<"Enter your device IP and Port: \n";
    std::cout<<"Enter IP: ";
    std::cin>>ip;
    std::cout<<"Enter Port: ";
    std::cin>>port;
}

void DrawMenu()
{
    std::cout << "\n========================================\n";
    std::cout << "         Remote Control Main Menu       \n";
    std::cout << "========================================\n\n";

    std::cout << std::left << std::setw(3) << "1" << "COPYFILE                (COPYFILE <path>)\n";
    std::cout << std::setw(3) << "2" << "TOGGLE_VIDEO             (Start/Stop video capture)\n";
    std::cout << std::setw(3) << "3" << "GET_VIDEO                (Download last recorded video)\n";
    std::cout << std::setw(3) << "4" << "TOGGLE_KEYLOGGER         (Start/Stop keylogger)\n";
    std::cout << std::setw(3) << "5" << "GET_KEYLOGGER            (Retrieve logged keys)\n";
    std::cout << std::setw(3) << "6" << "GET_RUNNING_PROCESSS     (List all running processes)\n";
    std::cout << std::setw(3) << "7" << "RUN_PROCESS              (RUN_PROCESS <name>)\n";
    std::cout << std::setw(3) << "8" << "SHUTDOWN_PROCESS         (SHUTDOWN_PROCESS <name>)\n";
    std::cout << std::setw(3) << "9" << "SLEEP                    (Put system to sleep)\n";
    std::cout << std::setw(3) << "10" << "RESTART                  (Restart system)\n";
    std::cout << std::setw(3) << "11" << "SHUTDOWN                 (Shutdown system)\n";
    std::cout << std::setw(3) << "12" << "EXIT CLIENT                               \n";

    std::cout << "\n----------------------------------------\n";
    std::cout << "Enter your choice (1-12): ";
}

void Clear()
{
    system("cls");
}

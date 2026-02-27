#include <iostream>
#include <conio.h>
#include "bluetooth.hpp"
#include "p2p.hpp"


int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    char choice;

    std::cout << "=== Chat Program ===\n";
    std::cout << "Select chat type:\n";
    std::cout << "1. P2P Chat\n";
    std::cout << "2. Bluetooth Chat(beta test)\n";
    std::cout << "Enter choice (1 or 2): ";
    std::cin >> choice;
    std::cin.ignore();

    if (choice == '1')
    {
        std::string username;
        unsigned short port = 0;

        std::cout << "Enter your username: ";
        std::getline(std::cin, username);

        std::cout << "Enter listening port (0 for auto): ";
        std::string portStr;
        std::getline(std::cin, portStr);

        if (!portStr.empty()) port = std::stoi(portStr);
        
        P2PHandler p2pChat(username, port);
        p2pChat.run();
    }
    else if (choice == '2')
    {
        BluetoothChat bt;
        bt.run();
    }
    else
    {
        std::cout << "Invalid choice!\n";
    }

    std::cout << "Program ended.\n";

    std::cout << "Press any key to exit...";
    _getch();

    return 0;
}
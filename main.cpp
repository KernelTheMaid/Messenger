#include <iostream>
#include <conio.h>
#include "tcp.hpp"
#include "bluetooth.hpp"
#include "p2p.hpp"
#include "repeater.hpp"
#include "upnp.hpp"

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    char choice;

    std::cout << "=== Chat Program ===\n";
    std::cout << "Select chat type:\n";
    std::cout << "1. TCP Chat\n";
    std::cout << "2. Bluetooth Chat\n";
    std::cout << "3. P2P Chat\n";
    std::cout << "Enter choice (1, 2 or 3): ";
    std::cin >> choice;
    std::cin.ignore();

    if (choice == '1')
    {
        TCPSocketHandler tcp;
        tcp.run();
    }
    else if (choice == '2')
    {
        BluetoothChat bt;
        bt.run();
    }

    else if (choice == '3')
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
    else
    {
        std::cout << "Invalid choice!\n";
    }

    std::cout << "Program ended.\n";

    std::cout << "Press any key to exit...";
    _getch();

    return 0;
}
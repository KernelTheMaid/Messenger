#include <iostream>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include "bluetooth.hpp"
#endif
#include "p2p.hpp"

int main()
{
    #ifdef _WIN32	
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif
    char choice;

    std::cout << "=== Chat Program ===\n";
    std::cout << "Select chat type:\n";
    std::cout << "1. P2P Chat\n";
    #ifdef _WIN32
    std::cout << "2. Bluetooth Chat(beta test)\n";
    #endif
    std::cout << "Enter choice (1 or 2): ";
    std::cin >> choice;
    std::cin.ignore();
    switch(choice){
	case '1':{
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
		break;
	}
	
	case '2':{
	        #ifdef _WIN32
        	BluetoothChat bt;
        	bt.run();
		#endif
		break;
	}
	
	default:
		std::cout << "Invalid choice" << std::endl;
		break;
    }
        
    

    std::cout << "Program ended.\n";

    std::cout << "Press any key to exit...";
    #ifdef _WIN32
    _getch();
    #else
    std::cin.get();
    #endif

    return 0;
}

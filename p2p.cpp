#include "p2p.hpp"
#include <iostream>
#include <conio.h>
#include <sstream>
#include <optional>
#include <SFML/Network.hpp>
#include <string>

std::string getPublicIp() {
    sf::Http http("http://api.ipify.org");
    sf::Http::Request request;
    request.setMethod(sf::Http::Request::Method::Get);
    request.setUri("/");

    sf::Http::Response response = http.sendRequest(request);

    if (response.getStatus() == sf::Http::Response::Status::Ok) {
        return response.getBody();
    } else {
        return "0.0.0.0 (Error)";
    }
}

struct Peer {
    std::string username;
    sf::IpAddress ip;
    unsigned short port;
    sf::Clock lastSeen;
};

P2PHandler::P2PHandler(const std::string& name, unsigned short port)
    : username(name), listeningPort(port), isRunning(true) {
    
    if (listeningPort == 0) listeningPort = 5000 + (rand() % 1000);

    // Привязываем сокет к порту
    if (udpSocket.bind(listeningPort) != sf::Socket::Status::Done) {
        std::cerr << "Failed to bind UDP socket to port " << listeningPort << "\n";
    }
    udpSocket.setBlocking(false); 
}

P2PHandler::~P2PHandler() {
    isRunning = false;
    if (receiveThread.joinable()) receiveThread.join();
}

void P2PHandler::receiveLoop() {
    while (isRunning) {
        sf::Packet packet;
        std::optional<sf::IpAddress> remoteIp;
        unsigned short remotePort = 0;

        if (udpSocket.receive(packet, remoteIp, remotePort) == sf::Socket::Status::Done) {
            if (remoteIp.has_value()) {
                handleIncomingPacket(packet, remoteIp.value(), remotePort);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


void P2PHandler::handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort) {
    std::string type, senderName, content;
    if (packet >> type >> senderName) {
        std::lock_guard<std::mutex> lock(peersMutex);
        
        // Авто-регистрация пира (Discovery)
        knownPeers[senderName] = {senderName, remoteIp, remotePort, sf::Clock()};

        if (type == "MSG") {
            if (packet >> content) {
                std::cout << "\n" << senderName << ": " << content << "\n> ";
                std::cout.flush();
            }
        } else if (type == "PING") {
            // Просто подтверждение связи для пробивки NAT
            std::cout << "\n[System] Node " << senderName << " is reachable.\n> ";
        }
    }
}

void P2PHandler::broadcastMessage(const std::string& text) {
    sf::Packet packet;
    packet << std::string("MSG") << username << text;

    std::lock_guard<std::mutex> lock(peersMutex);
    for (auto& [name, peer] : knownPeers) {
        udpSocket.send(packet, peer.ip, peer.port);
    }
}

void P2PHandler::run() {
    receiveThread = std::thread(&P2PHandler::receiveLoop, this);

    std::cout << "\n=== P2P UDP Chat Started ===\n";
    std::cout << "Your local username: " << username << "\n";
    
    // Автоматическое получение внешнего IP
    std::cout << "[System] Fetching your public IP... ";
    std::string publicIp = getPublicIp();
    std::cout << "Done!\n";
    
    std::cout << "-------------------------------------------\n";
    std::cout << "SHARE THIS WITH YOUR FRIEND:\n";
    std::cout << "IP:   " << publicIp << "\n";
    std::cout << "PORT: " << listeningPort << "\n";
    std::cout << "-------------------------------------------\n\n";

    std::cout << "Commands:\n";
    std::cout << "  add <ip> <port> - Start punching hole to peer\n";
    std::cout << "  exit            - Quit chat\n\n> ";

    std::string input;
    while (isRunning) {
        if (_kbhit()) {
            std::getline(std::cin, input);
            if (input == "exit") break;

            if (input.substr(0, 3) == "add") {
                std::istringstream iss(input);
                std::string cmd, ipStr;
                unsigned short port;
                if (iss >> cmd >> ipStr >> port) {
                    sf::Packet ping;
                    ping << std::string("PING") << username;
                    udpSocket.send(ping, sf::IpAddress::resolve(ipStr).value(), port);
                    std::cout << "Punching hole to " << ipStr << ":" << port << "...\n";
                }
            } else {
                broadcastMessage(input);
                std::cout << "You: " << input << "\n> ";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
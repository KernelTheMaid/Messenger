#include "p2p.hpp"
#include <iostream>
#include <conio.h>
#include <sstream>

std::string P2PHandler::getPublicIp() {
    sf::Http http("api.ipify.org"); 
    sf::Http::Request request;
    request.setMethod(sf::Http::Request::Method::Get);
    request.setUri("/");

    sf::Http::Response response = http.sendRequest(request);
    if (response.getStatus() == sf::Http::Response::Status::Ok) {
        return response.getBody();
    }
    return "0.0.0.0 (Error)";
}

P2PHandler::P2PHandler(const std::string& name, unsigned short port)
    : username(name), listeningPort(port), isRunning(true) {
    
    if (listeningPort == 0) listeningPort = 5000 + (rand() % 1000);

    if (udpSocket.bind(listeningPort) != sf::Socket::Status::Done) {
        std::cerr << "Failed to bind port " << listeningPort << "\n";
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
            if (remoteIp) { // Проверка optional
                handleIncomingPacket(packet, *remoteIp, remotePort);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void P2PHandler::handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort) {
    std::string type, senderName, content;
    if (packet >> type >> senderName) {
        std::lock_guard<std::mutex> lock(peersMutex);
        
        Peer newPeer{senderName, remoteIp, remotePort, std::chrono::steady_clock::now()};

        knownPeers.insert_or_assign(senderName, newPeer);

        if (type == "MSG" && (packet >> content)) {
            std::cout << "\n" << senderName << ": " << content << "\n> ";
            std::cout.flush();
        } else if (type == "PING") {
            std::cout << "\n[System] Node " << senderName << " is reachable.\n> ";
            std::cout.flush();
        }
    }
}

void P2PHandler::broadcastMessage(const std::string& text) {
    sf::Packet packet;
    packet << std::string("MSG") << username << text;

    std::lock_guard<std::mutex> lock(peersMutex);
    for (auto& [name, peer] : knownPeers) {
        (void)udpSocket.send(packet, peer.ip, peer.port);
    }
}

void P2PHandler::run() {
    receiveThread = std::thread(&P2PHandler::receiveLoop, this);

    std::cout << "\n[System] Public IP: " << getPublicIp() << " | Port: " << listeningPort << "\n";
    std::cout << "Commands: add <ip> <port> | exit\n\n> ";

    std::string input;
    while (isRunning) {
        if (_kbhit()) {
            std::getline(std::cin, input);
            if (input == "exit") break;

            if (input.substr(0, 3) == "add") {
                std::istringstream iss(input);
                std::string cmd, ipStr;
                unsigned short p;
                if (iss >> cmd >> ipStr >> p) {
                    auto targetIp = sf::IpAddress::resolve(ipStr);
                    if (targetIp) {
                        sf::Packet ping;
                        ping << std::string("PING") << username;
                        (void)udpSocket.send(ping, *targetIp, p);
                        std::cout << "Punching hole to " << ipStr << ":" << p << "...\n> ";
                    }
                }
            } else if (!input.empty()) {
                broadcastMessage(input);
                std::cout << "You: " << input << "\n> ";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
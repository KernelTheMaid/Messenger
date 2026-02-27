#include "p2p.hpp"
#include <iostream>
#include <conio.h>
#include <sstream>
#include <vector>

P2PHandler::P2PHandler(const std::string& name, unsigned short port)
    : username(name), listeningPort(port), isRunning(true) {
    
    if (listeningPort == 0) listeningPort = 5000 + (rand() % 1000);

    // Привязываем сокет. Если порт занят, SFML выберет другой (если port был 0)
    if (udpSocket.bind(listeningPort) != sf::Socket::Status::Done) {
        std::cerr << "[Error] Could not bind port " << listeningPort << "\n";
    }
    udpSocket.setBlocking(false);
}

P2PHandler::~P2PHandler() {
    isRunning = false;
    if (receiveThread.joinable()) {
        receiveThread.join();
    }
}

// Вспомогательная функция для STUN (узнаем внешний порт)
void P2PHandler::discoverExternalEndpoint(unsigned short& publicPort, std::string& publicIp) {
    publicIp = getPublicIp();
    publicPort = listeningPort; // По умолчанию

    // STUN Binding Request
    std::vector<uint8_t> stunReq = {
        0x00, 0x01, 0x00, 0x00, // Type: Binding Request
        0x21, 0x12, 0xA4, 0x42, // Magic Cookie
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c // Transaction ID
    };

    auto stunAddr = sf::IpAddress::resolve("stun.l.google.com");
    if (!stunAddr) return;

    // Отправляем через наш основной сокет!
    udpSocket.send(stunReq.data(), stunReq.size(), *stunAddr, 19302);

    sf::Clock timer;
    while (timer.getElapsedTime().asSeconds() < 1.5f) {
        sf::Packet packet;
        std::optional<sf::IpAddress> sender;
        unsigned short senderPort;
        if (udpSocket.receive(packet, sender, senderPort) == sf::Socket::Status::Done) {
            const uint8_t* data = (const uint8_t*)packet.getData();
            if (packet.getDataSize() >= 32) {
                // Извлекаем XOR-MAPPED-ADDRESS порт (упрощенно для примера)
                // Обычно он в атрибуте 0x0020
                unsigned short rawPort = (data[26] << 8) | data[27];
                publicPort = rawPort ^ 0x2112; // XOR с Magic Cookie по стандарту RFC
                break;
            }
        }
    }
}

void P2PHandler::sendPing(const sf::IpAddress& ip, unsigned short port) {
    sf::Packet ping;
    ping << std::string("PING") << username;
    udpSocket.send(ping, ip, port);
}

void P2PHandler::receiveLoop() {
    while (isRunning) {
        sf::Packet packet;
        std::optional<sf::IpAddress> remoteIp;
        unsigned short remotePort = 0;

        if (udpSocket.receive(packet, remoteIp, remotePort) == sf::Socket::Status::Done) {
            if (remoteIp) {
                handleIncomingPacket(packet, *remoteIp, remotePort);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void P2PHandler::handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort) {
    std::string type, senderName, content;
    if (packet >> type >> senderName) {
        // Если это не STUN ответ, а наше сообщение
        if (type == "MSG" || type == "PING") {
            std::lock_guard<std::mutex> lock(peersMutex);
            Peer p{ senderName, remoteIp, remotePort, std::chrono::steady_clock::now() };
            if (knownPeers.find(senderName) == knownPeers.end()) {
                std::cout << "\n[System] Connection established with " << senderName << "!\n> ";
            }
            knownPeers.insert_or_assign(senderName, p);

            if (type == "MSG" && (packet >> content)) {
                std::cout << "\n[" << senderName << "]: " << content << "\n> ";
                std::cout.flush();
            }
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

std::string P2PHandler::getPublicIp() {
    sf::Http http("api.ipify.org");
    sf::Http::Request request("/", sf::Http::Request::Method::Get);
    auto response = http.sendRequest(request);
    if (response.getStatus() == sf::Http::Response::Status::Ok) return response.getBody();
    return "Unknown";
}

void P2PHandler::run() {
    std::cout << "[System] Discovering NAT mapping via STUN...\n";
    unsigned short publicPort;
    std::string publicIp;
    discoverExternalEndpoint(publicPort, publicIp);

    receiveThread = std::thread(&P2PHandler::receiveLoop, this);

    std::cout << "\n======================================\n";
    std::cout << "  YOUR PUBLIC ENDPOINT:\n";
    std::cout << "  IP:   " << publicIp << "\n";
    std::cout << "  PORT: " << publicPort << "\n";
    std::cout << "======================================\n";
    std::cout << "Give these to your friend.\n";
    std::cout << "Commands: add <ip> <port> | exit\n\n> ";

    auto lastHeartbeat = std::chrono::steady_clock::now();

    while (isRunning) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() > 5) {
            std::lock_guard<std::mutex> lock(peersMutex);
            for (auto& [name, peer] : knownPeers) {
                sendPing(peer.ip, peer.port);
            }
            lastHeartbeat = now;
        }

        if (_kbhit()) {
            std::string input;
            std::getline(std::cin, input);
            if (input == "exit") break;

            if (input.substr(0, 3) == "add") {
                std::istringstream iss(input);
                std::string cmd, ipStr;
                unsigned short p;
                if (iss >> cmd >> ipStr >> p) {
                    auto target = sf::IpAddress::resolve(ipStr);
                    if (target) {
                        std::cout << "[System] Punching hole to " << ipStr << ":" << p << "...\n> ";
                        for(int i=0; i<10; ++i) { // 10 попыток для надежности
                            sendPing(*target, p);
                            std::this_thread::sleep_for(std::chrono::milliseconds(30));
                        }
                    }
                }
            } else if (!input.empty()) {
                broadcastMessage(input);
                std::cout << "You: " << input << "\n> ";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
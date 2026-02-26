#pragma once
#include <SFML/Network.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>

struct Peer {
    std::string username;
    sf::IpAddress ip;
    unsigned short port;
    sf::Clock lastSeen; 
};

class P2PHandler {
public:
    P2PHandler(const std::string& name, unsigned short port = 0);
    ~P2PHandler();
    
    void run();

private:
    std::string username;
    unsigned short listeningPort;
    sf::UdpSocket udpSocket; // Один сокет для всего
    
    std::map<std::string, Peer> knownPeers; // Список друзей
    mutable std::mutex peersMutex;
    
    bool isRunning;
    std::thread receiveThread;

    void receiveLoop();
    void sendPacket(sf::Packet& packet, const sf::IpAddress& ip, unsigned short port);
    void broadcastMessage(const std::string& text);
    void handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort);
};
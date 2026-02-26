#pragma once
#include <SFML/Network.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <chrono>

struct Peer {
    std::string username;
    sf::IpAddress ip;
    unsigned short port;
    std::chrono::steady_clock::time_point lastSeen;
};

class P2PHandler {
public:
    P2PHandler(const std::string& name, unsigned short port = 0);
    ~P2PHandler();
    void run();

private:
    std::string username;
    unsigned short listeningPort;
    sf::UdpSocket udpSocket;
    std::map<std::string, Peer> knownPeers;
    mutable std::mutex peersMutex;
    bool isRunning;
    std::thread receiveThread;

    void receiveLoop();
    void handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort);
    void broadcastMessage(const std::string& text);
    std::string getPublicIp();
};
#pragma once
#include <SFML/Network.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <memory>

struct PeerInfo {
    std::string username;
    sf::IpAddress ip;
    unsigned short port;
    bool isConnected;
};

struct ChatMessage {
    std::string sender;
    std::string content;
    long long timestamp;
};

class P2PHandler {
public:
    P2PHandler(const std::string& name, unsigned short listenPort = 0);
    ~P2PHandler();
    
    void run();
    void startListening();
    void connectToPeer(const std::string& peerIp, unsigned short peerPort, const std::string& peerName);
    void sendMessage(const std::string& message);
    void disconnectPeer(const std::string& peerId);
    
private:
    std::string username;
    unsigned short listeningPort;
    sf::TcpListener listener;
    
    std::map<std::string, std::shared_ptr<sf::TcpSocket>> peers;
    std::vector<PeerInfo> peerList;
    std::queue<ChatMessage> messageQueue;
    
    std::thread listeningThread;
    std::thread receivingThread;
    mutable std::mutex peersMutex;
    mutable std::mutex messageMutex;
    
    bool isRunning;
    
    void acceptConnections();
    void receiveMessages();
    void chatLoop();
    void displayPeerList() const;
    std::string getPeerId(const std::string& username);
};
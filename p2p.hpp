#pragma once
#include <SFML/Network.hpp>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>
#include <optional>

// Структура данных о другом пользователе
struct Peer {
    std::string username;
    sf::IpAddress ip;
    unsigned short port;
    std::chrono::steady_clock::time_point lastSeen;
};

class P2PHandler {
public:
    // Конструктор и деструктор
    P2PHandler(const std::string& name, unsigned short port = 0);
    ~P2PHandler(); // Исправлено имя

    // Основной цикл программы
    void run();

private:
    // Личные данные
    std::string username;
    unsigned short listeningPort;
    sf::UdpSocket udpSocket;
    
    // Список активных соединений и защита потоков
    std::map<std::string, Peer> knownPeers;
    mutable std::mutex peersMutex;
    
    // Управление потоками
    bool isRunning;
    std::thread receiveThread;

    // Внутренняя логика сети
    void receiveLoop();
    void handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort);
    
    // Сетевые команды
    void sendPing(const sf::IpAddress& ip, unsigned short port);
    void broadcastMessage(const std::string& text);
    
    // Вспомогательные функции для обхода NAT (STUN)
    std::string getPublicIp();
    void discoverExternalEndpoint(unsigned short& publicPort, std::string& publicIp);
};
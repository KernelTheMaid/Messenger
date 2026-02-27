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
    std::string username; // Имя пользователя
    sf::IpAddress ip; // IP-адрес
    unsigned short port; // Порт
    std::chrono::steady_clock::time_point lastSeen; // Время последнего получения сообщения
};

class P2PHandler {
public:
    // Конструктор и деструктор
    P2PHandler(const std::string& name, unsigned short port = 0); // Принимает имя пользователя и порт для прослушивания
    ~P2PHandler(); // Деструктор для корректного завершения потоков

    // Основной цикл программы
    void run();

private:
    // Личные данные
    std::string username; // Имя текущего пользователя
    unsigned short listeningPort; // Порт, на котором слушает сокет
    sf::UdpSocket udpSocket; // UDP-сокет для обмена данными
    
    // Список активных соединений и защита потоков
    std::map<std::string, Peer> knownPeers; // Ассоциативный массив: имя пира → структура Peer
    mutable std::mutex peersMutex; // Мьютекс для безопасного доступа к knownPeers из разных потоков
    
    // Управление потоками
    bool isRunning; // Флаг, указывающий, должен ли поток приёма работать
    std::thread receiveThread; // Поток, в котором работает receiveLoop()

    // Внутренняя логика сети
    void receiveLoop(); // Бесконечный цикл приёма входящих пакетов (выполняется в отдельном потоке)
    void handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort); // Обрабатывает полученный пакет: разбирает тип и обновляет список пиров
    
    // Сетевые команды
    void sendPing(const sf::IpAddress& ip, unsigned short port); // Отправляет Ping-сообщение указанному адресату
    void broadcastMessage(const std::string& text); // Рассылает текстовое сообщение всем известным пирам
    
    // Вспомогательные функции для обхода NAT (STUN)
    std::string getPublicIp(); // Получает внешний IP через HTTP-запрос к api.ipify.org
    void discoverExternalEndpoint(unsigned short& publicPort, std::string& publicIp); // Пытается узнать внешний порт с помощью STUN-запроса
};
#include "p2p.hpp"
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

static bool inputAvailable(){
	struct timeval tv = {0,0};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO + 1, &fds, nullptr,nullptr,&tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}
#endif

#include <sstream>
#include <vector>
#include "crypto.h"
P2PHandler::P2PHandler(const std::string& name, unsigned short port)
    : username(name), listeningPort(port), isRunning(true) { // Конструктор инициализирует поля: имя, порт, флаг работы.
    
    if (listeningPort == 0) listeningPort = 5000 + (rand() % 1000);
    // Если порт не задан (0), генерируем случайный в диапазоне 5000-5999.

    // Привязываем сокет.
    if (udpSocket.bind(listeningPort) != sf::Socket::Status::Done) {
        std::cerr << "[Error] Could not bind port " << listeningPort << "\n";
    }
    udpSocket.setBlocking(false);
}

P2PHandler::~P2PHandler() {
    isRunning = false; // Сигнал завершения потока приёма
    if (receiveThread.joinable()) { // Если поток был создан и ещё не завершён
        receiveThread.join(); // Ожидаем его завершения
    }
}

// Вспомогательная функция для STUN (узнаем внешний порт)
void P2PHandler::discoverExternalEndpoint(unsigned short& publicPort, std::string& publicIp) {
    publicIp = getPublicIp(); // Получаем внешний IP через HTTP-сервис
    publicPort = listeningPort; // По умолчанию

    // Узнаём внешний адрес
    std::vector<uint8_t> stunReq = {
        0x00, 0x01, 0x00, 0x00, // Тип сообщения: Binding Request (0x0001), длина 0
        0x21, 0x12, 0xA4, 0x42, // Magic Cookie (фиксированное значение для STUN)
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c // Transaction ID
    };

    sf::IpAddress stunAddr("stun.l.google.com"); // Разрешаем доменное имя STUN-сервера Google
    if (stunAddr == sf::IpAddress::None) return;

    // Отправляем STUN-запрос через наш основной сокет
    udpSocket.send(stunReq.data(), stunReq.size(), stunAddr, 19302); // Порт STUN-сервера 19302

    sf::Clock timer; // Таймер для ограничения времени ожидания ответа
    while (timer.getElapsedTime().asSeconds() < 1.5f) { // Ждём ответ не более 1.5 секунды
        sf::Packet packet;
	sf::IpAddress sender;
        unsigned short senderPort;
        if (udpSocket.receive(packet, sender, senderPort) == sf::Socket::Done) {
            // Получаем пакет от STUN-сервера
            const uint8_t* data = (const uint8_t*)packet.getData(); // Получаем указатель на данные пакета
            if (packet.getDataSize() >= 32) {
                // Извлекаем XOR-MAPPED-ADDRESS порт
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
    ping << std::string("PING") << username; // Формируем пакет: тип "PING" и имя текущего пользователя
    udpSocket.send(ping, ip, port); // Отправляем пакет по указанному адресу
}

void P2PHandler::receiveLoop() {
    while (isRunning) {
        sf::Packet packet;
	sf::IpAddress remoteIp; // IP отправителя
        unsigned short remotePort = 0;

        // Пытаемся получить пакет
        if (udpSocket.receive(packet, remoteIp, remotePort) == sf::Socket::Done) {
            
                handleIncomingPacket(packet, remoteIp, remotePort); // Обрабатываем пакет
            
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Небольшая пауза для снижения нагрузки на процессор
    }
}

void P2PHandler::handleIncomingPacket(sf::Packet& packet, sf::IpAddress remoteIp, unsigned short remotePort) {
    std::string type, senderName, content;
    if (packet >> type >> senderName) { // Извлекаем тип сообщения и имя отправителя
        // Если это не STUN ответ, а наше сообщение
        if (type == "MSG" || type == "PING") {
            std::lock_guard<std::mutex> lock(peersMutex);  // Захватываем мьютекс для работы с knownPeers
            Peer p{ senderName, remoteIp, remotePort, std::chrono::steady_clock::now() }; // Создаём запись о пире
            if (knownPeers.find(senderName) == knownPeers.end()) {
                std::cout << "\n[System] Connection established with " << senderName << "!\n> ";
            }
            knownPeers.insert_or_assign(senderName, p); // Добавляем или обновляем информацию о пире

            if (type == "MSG" && (packet >> content)) { // Если это сообщение и удалось извлечь текст
                std::cout << "\n[" << senderName << "]: " << content << "\n> ";
                std::cout.flush(); // Выводим буфер
            }
        }
    }
}

void P2PHandler::broadcastMessage(const std::string& text) {
    sf::Packet packet;
    packet << std::string("MSG") << username << text; // Формируем пакет с текстом
    std::lock_guard<std::mutex> lock(peersMutex); // Захватываем мьютекс для безопасного перебора пиров
    for (auto& [name, peer] : knownPeers) { // Проходим по всем известным пирам
        udpSocket.send(packet, peer.ip, peer.port);  // Отправляем каждому сообщение
    }
}

std::string P2PHandler::getPublicIp() {
    sf::Http http("api.ipify.org"); // Создаём HTTP-клиент для сервиса определения IP
    sf::Http::Request request("/", sf::Http::Request::Method::Get); // GET-запрос к корневому пути
    auto response = http.sendRequest(request); // Отправляем запрос и получаем ответ
    if (response.getStatus() == sf::Http::Response::Status::Ok) return response.getBody(); // Если успешно, возвращаем IP
    return "Unknown";
}

void P2PHandler::run() {
    std::cout << "[System] Discovering NAT mapping via STUN...\n";
    unsigned short publicPort;
    std::string publicIp;
    discoverExternalEndpoint(publicPort, publicIp); // Определяем внешний адрес

    receiveThread = std::thread(&P2PHandler::receiveLoop, this); // Запускаем поток приёма сообщений

    std::cout << "\n======================================\n";
    std::cout << "  YOUR PUBLIC ENDPOINT:\n";
    std::cout << "  IP:   " << publicIp << "\n";
    std::cout << "  PORT: " << publicPort << "\n";
    std::cout << "======================================\n";
    std::cout << "Give these to your friend.\n";
    std::cout << "Commands: add <ip> <port> | exit\n\n> ";

    auto lastHeartbeat = std::chrono::steady_clock::now(); // Запоминаем время последней рассылки Ping

    while (isRunning) {
        auto now = std::chrono::steady_clock::now();
	//Antidebug check
	antidebug();
        // Каждые 5 секунд отправляем Ping всем известным пирам, чтобы поддерживать открытыми NAT-дыры
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() > 5) {
            std::lock_guard<std::mutex> lock(peersMutex);
            for (auto& [name, peer] : knownPeers) {
                sendPing(peer.ip, peer.port); // Отправляем Ping
            }
            lastHeartbeat = now; // Обновляем время последнего heartbeat
        }
        #ifdef _WIN32
        if (_kbhit()) {
	#else
	if(inputAvailable()){
	#endif
            std::string input;
            std::getline(std::cin, input); // Считываем всю строку до Enter
            if (input == "exit") break; // Команда выхода – прерываем цикл

            if (input.substr(0, 3) == "add") { // Команда "add <ip> <port>"
                std::istringstream iss(input);
                std::string cmd, ipStr;
                unsigned short p;
                if (iss >> cmd >> ipStr >> p) { // Разбираем IP и порт
                    sf::IpAddress target(ipStr); // Разрешаем IP-адрес
                    if (target != sf::IpAddress::None) {
                        std::cout << "[System] Punching hole to " << ipStr << ":" << p << "...\n> ";
                        for(int i=0; i<10; ++i) { // 10 попыток для надежности, чтобы пробить NAT
                            sendPing(target, p);
                            std::this_thread::sleep_for(std::chrono::milliseconds(30)); // Небольшая пауза между попытками
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

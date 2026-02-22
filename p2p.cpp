#include "p2p.hpp"
#include <iostream>
#include <chrono>
#include <conio.h>
#include <algorithm>
#include <sstream>

P2PHandler::P2PHandler(const std::string& name, unsigned short listenPort)
    : username(name), listeningPort(listenPort), isRunning(true) {
    if (listeningPort == 0) {
        // Генерируем случайный порт в диапазоне 5000-6000
        listeningPort = 5000 + (rand() % 1000);
    }
}

P2PHandler::~P2PHandler() {
    isRunning = false;
    if (listeningThread.joinable()) {
        listeningThread.join();
    }
    if (receivingThread.joinable()) {
        receivingThread.join();
    }
}

void P2PHandler::run() {
    // Запускаем прослушивание входящих соединений
    startListening();
    
    std::cout << "\n=== P2P Chat Started ===\n";
    std::cout << "Your username: " << username << "\n";
    std::cout << "Listening on port: " << listeningPort << "\n\n";
    
    chatLoop();
}

void P2PHandler::startListening() {
    // Начинаем прослушивать входящие соединения
    if (listener.listen(listeningPort) != sf::Socket::Status::Done) {
        std::cout << "Error: Cannot listen on port " << listeningPort << "\n";
        isRunning = false;
        return;
    }
    
    // Запускаем поток для принятия соединений
    listeningThread = std::thread(&P2PHandler::acceptConnections, this);
    
    // Запускаем поток для приема сообщений
    receivingThread = std::thread(&P2PHandler::receiveMessages, this);
}

void P2PHandler::acceptConnections() {
    while (isRunning) {
        auto peerSocket = std::make_shared<sf::TcpSocket>();
        
        if (listener.accept(*peerSocket) == sf::Socket::Status::Done) {
            // Получаем информацию о подключившемся пиру
            sf::Packet packet;
            if (peerSocket->receive(packet) == sf::Socket::Status::Done) {
                std::string peerName;
                if (packet >> peerName) {
                    std::string peerId = peerName + "_" + std::to_string(rand());
                    
                    {
                        std::lock_guard<std::mutex> lock(peersMutex);
                        peers[peerId] = peerSocket;
                    }
                    
                    // Отправляем подтверждение
                    sf::Packet response;
                    response << username;
                    if (peerSocket->send(response) != sf::Socket::Status::Done) {
                        std::cerr << "Не удалось отправить подтверждение новому пиру\n";
                        // можно также закрыть соединение или повторить попытку
                    }
                    
                    std::cout << "\n>>> " << peerName << " connected!\n";
                    peerSocket->setBlocking(false);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void P2PHandler::connectToPeer(const std::string& peerIp, unsigned short peerPort, const std::string& peerName) {
    auto peerSocket = std::make_shared<sf::TcpSocket>();
    
    // resolve возвращает std::optional<sf::IpAddress>
    auto ipAddressOpt = sf::IpAddress::resolve(peerIp);
    
    // Проверяем, удалось ли разрешить IP-адрес
    if (!ipAddressOpt.has_value()) {
        std::cout << "Error: Cannot resolve IP address: " << peerIp << "\n";
        return;
    }
    
    // Получаем значение из optional
    sf::IpAddress ipAddress = ipAddressOpt.value();
    
    if (peerSocket->connect(ipAddress, peerPort) == sf::Socket::Status::Done) {
        // Отправляем свое имя
        sf::Packet packet;
        packet << username;
        
        if (peerSocket->send(packet) == sf::Socket::Status::Done) {
            // Получаем ответ
            sf::Packet response;
            if (peerSocket->receive(response) == sf::Socket::Status::Done) {
                std::string receivedName;
                if (response >> receivedName) {
                    std::string peerId = peerName + "_" + std::to_string(rand());
                    
                    {
                        std::lock_guard<std::mutex> lock(peersMutex);
                        peers[peerId] = peerSocket;
                    }
                    
                    peerSocket->setBlocking(false);
                    std::cout << "\n>>> Connected to " << peerName << " at " << peerIp << ":" << peerPort << "\n";
                }
            }
        }
    } else {
        std::cout << "Error: Cannot connect to " << peerName << "\n";
    }
}

void P2PHandler::receiveMessages() {
    while (isRunning) {
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            
            for (auto it = peers.begin(); it != peers.end(); ) {
                auto& peerSocket = it->second;
                sf::Packet packet;
                
                sf::Socket::Status status = peerSocket->receive(packet);
                
                if (status == sf::Socket::Status::Done) {
                    std::string senderName;
                    std::string message;
                    
                    if (packet >> senderName >> message) {
                        ChatMessage chatMsg;
                        chatMsg.sender = senderName;
                        chatMsg.content = message;
                        chatMsg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                        
                        {
                            std::lock_guard<std::mutex> msgLock(messageMutex);
                            messageQueue.push(chatMsg);
                        }
                    }
                    ++it;
                } else if (status == sf::Socket::Status::Disconnected) {
                    std::cout << "\n>>> Peer disconnected: " << it->first << "\n";
                    it = peers.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void P2PHandler::sendMessage(const std::string& message) {
    if (message.empty()) return;
    
    sf::Packet packet;
    packet << username << message;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        
        int successCount = 0;
        for (auto& peer : peers) {
            if (peer.second->send(packet) == sf::Socket::Status::Done) {
                successCount++;
            }
        }
        
        if (successCount == 0 && !peers.empty()) {
            std::cout << "Error: Message not sent to any peer\n";
        }
    }
}

void P2PHandler::chatLoop() {
    std::cout << "Commands:\n";
    std::cout << "  connect <ip> <port> <name> - Connect to a peer\n";
    std::cout << "  list - Show connected peers\n";
    std::cout << "  exit - Quit chat\n";
    std::cout << "Or just type a message to broadcast to all peers\n\n";
    
    std::string input;
    
    while (isRunning) {
        // Процесс полученных сообщений
        {
            std::lock_guard<std::mutex> lock(messageMutex);
            while (!messageQueue.empty()) {
                ChatMessage msg = messageQueue.front();
                messageQueue.pop();
                std::cout << "\n" << msg.sender << ": " << msg.content << "\n";
                std::cout << "> ";
                std::cout.flush();
            }
        }
        
        // Проверяем, была ли нажата клавиша
        if (_kbhit()) {
            std::getline(std::cin, input);
            
            if (!input.empty()) {
                if (input == "exit" || input == "quit") {
                    isRunning = false;
                    break;
                } else if (input == "list") {
                    displayPeerList();
                } else if (input.substr(0, 7) == "connect" && input.length() > 8) {
                    std::istringstream iss(input);
                    std::string cmd, ip, name;
                    unsigned short port;
                    
                    if (iss >> cmd >> ip >> port >> name) {
                        connectToPeer(ip, port, name);
                    } else {
                        std::cout << "Usage: connect <ip> <port> <name>\n";
                    }
                } else {
                    // Отправляем сообщение всем пирам
                    std::cout << "You: " << input << "\n";
                    sendMessage(input);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "\n=== Chat ended ===\n";
}

void P2PHandler::displayPeerList() const {
    std::lock_guard<std::mutex> lock(peersMutex);
    
    if (peers.empty()) {
        std::cout << "No connected peers\n";
    } else {
        std::cout << "\n=== Connected Peers ===\n";
        for (const auto& peer : peers) {
            std::cout << "  - " << peer.first << "\n";
        }
        std::cout << "=======================\n";
    }
}

std::string P2PHandler::getPeerId(const std::string& username) {
    std::lock_guard<std::mutex> lock(peersMutex);
    
    for (const auto& peer : peers) {
        if (peer.first.find(username) != std::string::npos) {
            return peer.first;
        }
    }
    
    return "";
}
#include "p2p.hpp"
#include <iostream>
#include <chrono>
#include <conio.h>
#include <algorithm>
#include <sstream>

P2PHandler::P2PHandler(const std::string &name, unsigned short listenPort)
    : username(name), listeningPort(listenPort), isRunning(true), connectedToRelay(false)
{
    if (listeningPort == 0)
    {
        listeningPort = 5000 + (rand() % 1000);
    }
}

P2PHandler::~P2PHandler()
{
    isRunning = false;
    if (listeningThread.joinable())
        listeningThread.join();
    if (receivingThread.joinable())
        receivingThread.join();

    if (connectedToRelay)
    {
        relaySocket.disconnect();
    }
}

void P2PHandler::run()
{
    startListening();

    std::cout << "\n=== P2P Chat Started ===\n";
    std::cout << "Your username: " << username << "\n";
    std::cout << "Listening on port: " << listeningPort << "\n\n";

    chatLoop();
}

void P2PHandler::startListening()
{
    if (listener.listen(listeningPort) != sf::Socket::Status::Done)
    {
        std::cout << "Error: Cannot listen on port " << listeningPort << "\n";
        isRunning = false;
        return;
    }
    listeningThread = std::thread(&P2PHandler::acceptConnections, this);
    receivingThread = std::thread(&P2PHandler::receiveMessages, this);
}

void P2PHandler::acceptConnections()
{
    while (isRunning)
    {
        auto peerSocket = std::make_shared<sf::TcpSocket>();
        if (listener.accept(*peerSocket) == sf::Socket::Status::Done)
        {
            sf::Packet packet;
            if (peerSocket->receive(packet) == sf::Socket::Status::Done)
            {
                std::string peerName;
                if (packet >> peerName)
                {
                    std::string peerId = peerName + "_" + std::to_string(rand());
                    {
                        std::lock_guard<std::mutex> lock(peersMutex);
                        peers[peerId] = peerSocket;
                    }
                    sf::Packet response;
                    response << username;
                    peerSocket->send(response);

                    std::cout << "\n>>> " << peerName << " connected directly!\n> ";
                    std::cout.flush();
                    peerSocket->setBlocking(false);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void P2PHandler::connectToPeer(const std::string &peerIp, unsigned short peerPort, const std::string &peerName)
{
    auto peerSocket = std::make_shared<sf::TcpSocket>();
    auto ipAddressOpt = sf::IpAddress::resolve(peerIp);

    if (!ipAddressOpt.has_value())
    {
        std::cout << "Error: Cannot resolve IP address: " << peerIp << "\n";
        return;
    }

    if (peerSocket->connect(ipAddressOpt.value(), peerPort, sf::seconds(5)) == sf::Socket::Status::Done)
    {
        sf::Packet packet;
        packet << username;
        if (peerSocket->send(packet) == sf::Socket::Status::Done)
        {
            sf::Packet response;
            if (peerSocket->receive(response) == sf::Socket::Status::Done)
            {
                std::string receivedName;
                if (response >> receivedName)
                {
                    std::string peerId = peerName + "_" + std::to_string(rand());
                    {
                        std::lock_guard<std::mutex> lock(peersMutex);
                        peers[peerId] = peerSocket;
                    }
                    peerSocket->setBlocking(false);
                    std::cout << "\n>>> Connected to " << peerName << " (P2P)\n";
                }
            }
        }
    }
    else
    {
        std::cout << "Error: Direct connection to " << peerName << " failed. Try using 'relay' command.\n";
    }
}

void P2PHandler::receiveMessages()
{
    while (isRunning)
    {
        // 1. Проверка прямых P2P сообщений
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            for (auto it = peers.begin(); it != peers.end();)
            {
                sf::Packet packet;
                sf::Socket::Status status = it->second->receive(packet);

                if (status == sf::Socket::Status::Done)
                {
                    std::string senderName, message;
                    if (packet >> senderName >> message)
                    {
                        std::cout << "\n"
                                  << senderName << " (P2P): " << message << "\n> ";
                        std::cout.flush();
                    }
                    ++it;
                }
                else if (status == sf::Socket::Status::Disconnected)
                {
                    std::cout << "\n>>> Peer disconnected: " << it->first << "\n> ";
                    it = peers.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // 2. Проверка сообщений от Ретранслятора (Relay)
        if (connectedToRelay)
        {
            char buffer[4096];
            std::size_t received;
            sf::Socket::Status status = relaySocket.receive(buffer, sizeof(buffer) - 1, received);

            if (status == sf::Socket::Status::Done)
            {
                buffer[received] = '\0';
                // Выводим как есть (сервер сам добавляет "Имя: ")
                std::cout << "\n[Relay] " << buffer << "\n> ";
                std::cout.flush();
            }
            else if (status == sf::Socket::Status::Disconnected)
            {
                std::cout << "\n[!] Connection to Relay lost.\n> ";
                connectedToRelay = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void P2PHandler::sendMessage(const std::string &message)
{
    if (message.empty())
        return;

    // Отправка прямым пирам
    sf::Packet packet;
    packet << username << message;
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (auto &peer : peers)
        {
            peer.second->send(packet);
        }
    }

    // Отправка через ретранслятор
    if (connectedToRelay)
    {
        relaySocket.send(message.c_str(), message.size());
    }
}

void P2PHandler::chatLoop()
{
    std::cout << "Commands:\n";
    std::cout << "  connect <ip> <port> <name> - Direct P2P connection\n";
    std::cout << "  relay <ip> <port>          - Connect via Relay server\n";
    std::cout << "  list                       - Show connected peers\n";
    std::cout << "  exit                       - Quit chat\n\n";

    std::string input;
    while (isRunning)
    {
        if (_kbhit())
        {
            std::getline(std::cin, input);
            if (input.empty())
                continue;

            if (input == "exit")
            {
                isRunning = false;
            }
            else if (input == "list")
            {
                displayPeerList();
                if (connectedToRelay)
                    std::cout << "  - Connected to Relay server\n";
            }
            else if (input.substr(0, 7) == "connect")
            {
                std::istringstream iss(input);
                std::string cmd, ip, name;
                unsigned short port;
                if (iss >> cmd >> ip >> port >> name)
                    connectToPeer(ip, port, name);
            }
            else if (input.substr(0, 5) == "relay")
            {
                std::istringstream iss(input);
                std::string cmd, ip;
                unsigned short rPort;
                if (iss >> cmd >> ip >> rPort)
                {
                    auto ipAddressOpt = sf::IpAddress::resolve(ip);
                    if (!ipAddressOpt.has_value())
                    {
                        std::cout << "Error: Cannot resolve IP address: " << ip << "\n";
                        return;
                    }

                    if (relaySocket.connect(ipAddressOpt.value(), rPort) == sf::Socket::Status::Done)
                    {
                        connectedToRelay = true;
                        relaySocket.setBlocking(false);
                        relaySocket.send(username.c_str(), username.size()); // Регистрация имени
                        std::cout << "Connected to Relay server!\n";
                    }
                    else
                    {
                        std::cout << "Could not reach Relay server.\n";
                    }
                }
            }
            else
            {
                std::cout << "You: " << input << "\n> ";
                sendMessage(input);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void P2PHandler::displayPeerList() const
{
    std::lock_guard<std::mutex> lock(peersMutex);
    if (peers.empty())
        std::cout << "No direct peers connected.\n";
    else
    {
        std::cout << "\n=== Direct Peers ===\n";
        for (const auto &peer : peers)
            std::cout << "  - " << peer.first << "\n";
    }
}

std::string P2PHandler::getPeerId(const std::string &username)
{
    std::lock_guard<std::mutex> lock(peersMutex);
    for (const auto &peer : peers)
    {
        if (peer.first.find(username) != std::string::npos)
            return peer.first;
    }
    return "";
}
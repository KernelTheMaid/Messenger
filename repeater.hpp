#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class ChatRelay {
private:
    SOCKET listener;
    std::map<SOCKET, std::string> clients;
    std::mutex clientsMutex;
    bool isRunning = true;

public:
    bool start(unsigned short port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener == INVALID_SOCKET) return false;

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return false;

        listen(listener, SOMAXCONN);
        std::cout << "[Relay] Server active on port " << port << std::endl;

        std::thread acceptThread(&ChatRelay::acceptClients, this);
        receiveLoop();

        if (acceptThread.joinable()) acceptThread.join();
        closesocket(listener);
        WSACleanup();
        return true;
    }

private:
    void acceptClients() {
        while (isRunning) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET client = accept(listener, (sockaddr*)&clientAddr, &clientAddrLen);

            if (client != INVALID_SOCKET) {
                char buffer[256];
                int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    std::string username(buffer);
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        clients[client] = username;
                    }
                    broadcast("System", username + " joined the relay");
                    u_long mode = 1;
                    ioctlsocket(client, FIONBIO, &mode);
                } else {
                    closesocket(client);
                }
            }
        }
    }

    void receiveLoop() {
        while (isRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto it = clients.begin(); it != clients.end(); ) {
                char buffer[4096];
                int bytes = recv(it->first, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    broadcast(it->second, std::string(buffer));
                    ++it;
                } else if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                    closesocket(it->first);
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void broadcast(const std::string& sender, const std::string& message) {
        std::string fullMessage = sender + ": " + message;
        for (auto& client : clients) {
            send(client.first, fullMessage.c_str(), (int)fullMessage.size(), 0);
        }
    }
};
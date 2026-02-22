#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class ChatRelay {
private:
    SOCKET listener;
    std::map<SOCKET, std::string> clients;  // сокет -> имя пользователя
    std::mutex clientsMutex;
    bool isRunning = true;

public:
    bool start(unsigned short port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return false;
        }

        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener == INVALID_SOCKET) {
            std::cerr << "Socket creation failed\n";
            return false;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed\n";
            return false;
        }

        listen(listener, SOMAXCONN);
        std::cout << "Relay listening on port " << port << "\n";

        // Поток для принятия новых клиентов
        std::thread acceptThread(&ChatRelay::acceptClients, this);

        // Основной цикл обработки сообщений
        receiveLoop();

        acceptThread.join();
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
                std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr) << "\n";

                // Получаем имя клиента (первое сообщение)
                char buffer[256];
                int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    std::string username(buffer);

                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        clients[client] = username;
                    }

                    std::cout << "User " << username << " joined\n";

                    // Уведомляем всех о новом пользователе
                    broadcast("System", username + " joined the chat");

                    // Переводим сокет в неблокирующий режим для приёма
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
                SOCKET sock = it->first;
                char buffer[4096];
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);

                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    std::string message(buffer);
                    broadcast(it->second, message);
                    ++it;
                } else if (bytes == 0 || (bytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                    // Клиент отключился
                    std::cout << "User " << it->second << " disconnected\n";
                    broadcast("System", it->second + " left the chat");
                    closesocket(sock);
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void broadcast(const std::string& sender, const std::string& message) {
        std::string fullMessage = sender + ": " + message;
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& client : clients) {
            send(client.first, fullMessage.c_str(), fullMessage.size(), 0);
        }
    }
};

int main(int argc, char* argv[]) {
    unsigned short port = 8888; // порт по умолчанию
    if (argc >= 2) {
        port = atoi(argv[1]);
    }

    ChatRelay relay;
    relay.start(port);

    return 0;
}
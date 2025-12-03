#include <cerrno>
#include <iostream>
#include <sys/select.h>
#include <vector>
#include <csignal>
#include <cstring>
#include <cstdlib>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/sigset_t.h>


using namespace std;


class NetworkServer {
private:
    int serverSocket; // число для идентификации сокета
    int port; 
    vector<int> clientSockets; // список активных подключений (хранит клиентские сокеты)

    static volatile sig_atomic_t wasSigHup; // Флаг, что пришел SIGHUP
    sigset_t originalMask;
    sigset_t blockedMask;

    bool stop;
public:
    NetworkServer(int p = 8080) {
        serverSocket = -1;
        port = p;
        clientSockets.reserve(5);
        stop = false;
    }

    ~NetworkServer() { clean(); }

    // Обработчик для сигнала
    // Вызывается операционной системой, когда процесс получает SIGHUP
    static void sigHupHandler(int r) {
        // Выполняется при получении сигнала
        // Обработчик устанавливает флаг равный 1
        wasSigHup = 1;
    }

    void registerSignalHandler() {
        // Структура для настроек. Описывает как обрабатывать сигнал
        struct sigaction sa;
        
        sigaction(SIGHUP, NULL, &sa); // Текущие настройки
        
        // Установка нашего обработчика
        sa.sa_handler = sigHupHandler; // Будет вызываться при сигнале SIGHUP
        sa.sa_flags |= SA_RESTART; // Флаг автоматического перезапуска после обработки сигнала

        // Регистрируем обработчик
        sigaction(SIGHUP, &sa, NULL);
    }

    // Функция ставит сигналы в очередь
    void blockSignal() {
        sigemptyset(&blockedMask); // Очищаем маску
        
        // Добавляем SIGHUP в маску
        sigaddset(&blockedMask,
                  SIGHUP);
        
        // Блокируем
        sigprocmask(SIG_BLOCK,
                    &blockedMask,
                    &originalMask); 
    }

    void addClient(int clientSocket) {
        clientSockets.push_back(clientSocket);
    }

    void deleteClient(int clientSocket) {
        for (auto it = clientSockets.begin(); it != clientSockets.end(); it++) {
            if (*it == clientSocket) {
                clientSockets.erase(it);
                close(clientSocket);
                return;
            }
        }

        cout << "Client not found!\n";
    }


    void createSocket() {
        int err = 0;
        int optionVal = 1;

        // Возвращает файловый дискриптор
        serverSocket = socket(AF_INET,
                              SOCK_STREAM,
                              0);

        if (serverSocket < 0) {
            cout << "Error\n";
            exit(-1);
        }

        // Настройка сокета с переиспользованием адреса
        err = setsockopt(serverSocket,
                         SOL_SOCKET,
                         SO_REUSEADDR,
                         &optionVal,
                         sizeof(optionVal));

        if (err < 0) {
            close(serverSocket);
            cout << "Error\n";
            exit(-2);
        }


        // Настройка адреса сервера
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(port);

        // Привязка сокета к адресу
        err = bind(serverSocket,
                   (sockaddr*)&serverAddress,
                   sizeof(serverAddress));
        
        if (err < 0) {
            close(serverSocket);
            cout << "Error\n";
            exit(-3);
        }

        // Режим прослушивания. 5 - размер очереди ожидающих подключений
        err = listen(serverSocket, 5);

        if (err < 0) {
            close(serverSocket);
            cout << "Error\n";
            exit(-4);
        }

        cout << "Socket was created\n\n";
    }

    
    // Основной цикл сервера
    void start() {
        cout << "Start\n";


        while (!stop) {
            // Список сокетов
            fd_set fds;
            int maxFd = serverSocket;
            int err;
            
            // Очищаем список
            FD_ZERO(&fds);
            // Добавляем сокет в список
            FD_SET(serverSocket, &fds);

            // Добавляем всех клиентов в список
            for (auto it = clientSockets.begin(); it != clientSockets.end(); it++) {
                FD_SET(*it, &fds);

                if (*it > maxFd) maxFd = *it;
            }

            // Ожидаем событие
            err = pselect(maxFd + 1, // Сколько сокетов
                         &fds, // Список сокетов
                         NULL,
                         NULL,
                         NULL,
                         &originalMask); // Сигналы SIGHUP
            
            if (err == -1) {
                if (errno == EINTR) { // Если прерван сигналом
                    if (wasSigHup)
                    {
                        cout << "\n\nSIGHUP received!" << endl;
                        wasSigHup = 0; // сбрасываем флаг
                        continue;
                    } // начинаем заново
                } else break;
            }

            // Если пришел новый клиент
            if (FD_ISSET(serverSocket, &fds)) {
                acceptConnection();
            }
            
            // Обработка клиентов
            handleClients(fds);

        }

        cout << "Server stopped" << endl;
    }

private:
    void clean() {
        cout << "Clean... \n";

        for (auto& client : clientSockets) { 
            close(client);
            // client = -1;
        }

        clientSockets.clear();

        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }

        cout << "Complete!\n";
    }


    void acceptConnection() {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        

        int clientSocket = accept(serverSocket, 
                                (sockaddr*)&clientAddress, 
                                &clientLength);
        
        if (clientSocket < 0) {

            if (errno == EINTR) { // Прерван сигналом
                cout << "Accept interrupted by signal...\n";
                return;
            }
            perror("accept failed");
            return;
        }
        
        // Получаем IP-адрес клиента
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp));
        
        cout << "\n\nNew client connected: \n";
        
        cout << "IP: " << clientIp << ":" << htons(clientAddress.sin_port) << endl;
        cout << "FD: " << clientSocket << endl; 

        addClient(clientSocket);
        
        const char* welcomeMessage = "Welcome!\n";
        send(clientSocket, welcomeMessage, strlen(welcomeMessage), 0);
    }

    void handleClients(fd_set& fds) {
        vector<int> clientsToRemove;
        
        for (auto it = clientSockets.begin(); it != clientSockets.end(); it++) {
            int clientFd = *it;
            
            // Есть ли данные от клиента
            if (FD_ISSET(clientFd, &fds)) {
                char buffer[1024];
                
                ssize_t bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
                
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    
                    string data(buffer, bytes);

                    cout << "\n\nReceived from client FD: " << clientFd << endl;
                    cout << "Bytes: " << bytes << endl;
                    cout << "Data: " << data << endl;
                    
                    // Эхо-ответ клиенту
                    send(clientFd, buffer, bytes, 0);
                    
                } else if (bytes == 0) {
                    cout << "Client disconnected!\n";
                    cout << "FD: " << clientFd << endl;

                    clientsToRemove.push_back(clientFd);
                    
                } else {
                    if (errno != EINTR) { 
                        cout << "Error with client\n";
                        clientsToRemove.push_back(clientFd);
                    }
                }
            }
        }
        
        for (int clientFd : clientsToRemove)
            deleteClient(clientFd);
    }

};


volatile sig_atomic_t NetworkServer::wasSigHup = 0;


int main(int argc, char** argv) {
    int port = 8080;

    if (argc > 1) port = atoi(argv[1]);


    cout << "Network Server" << endl;
    cout << "Starting server on port " << port << endl;
    cout << "Server PID: " << getpid() << "\n\n";

    NetworkServer server(port);

    server.createSocket();
    
    server.registerSignalHandler();

    server.blockSignal();

    server.start(); 


    cout << "Server stopped!" << endl;

    return 0;
}

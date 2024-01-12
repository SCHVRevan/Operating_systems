#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#define PORT "31337"
#define MAX_CLIENTS 256

volatile sig_atomic_t wasSigHup = 0;

// Обработчик сигналов для остановки сервера
void sigHupHandler(int r) {
    wasSigHup = 1;
}

// Получение адреса
void *get_socket_address(struct sockaddr *sockaddr) {
    if (sockaddr->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sockaddr)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sockaddr)->sin6_addr);
}

// Запуск сервера и привязка к порту
int setup_server_socket() {
    struct addrinfo hints, *addr_info, *iter;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &addr_info) != 0) {
        perror("getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    int server_socket, yes = 1;
    for (iter = addr_info; iter != NULL; iter = iter->ai_next) {
        server_socket = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
        if (server_socket < 0) continue;

        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(server_socket, iter->ai_addr, iter->ai_addrlen) == 0) break; // Успешно привязали сокет

        close(server_socket);
    }

    if (iter == NULL) {
        fprintf(stderr, "Failed to bind server socket\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(addr_info);

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    return server_socket;
}

// Регистрация обработчика сигналов
void register_signal_handler() {
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGKILL, &sa, NULL);
}

// Поиск id клиента в массиве активных клиентов
int find_client_index(int clients[], int active_clients, int fd) {
    for (int i = 0; i < active_clients; i++) {
        if (clients[i] == fd) return i;
    }
    return -1;
}

int main() {
    int server_socket = setup_server_socket();
    int client_sockets[MAX_CLIENTS];
    int active_clients = 0;

    register_signal_handler();

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigaddset(&blockedMask, SIGINT);
    sigaddset(&blockedMask, SIGTERM);
    sigaddset(&blockedMask, SIGQUIT);
    sigaddset(&blockedMask, SIGKILL);
  
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask error");
        return 1;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(server_socket, &fds);
    int max_fd = server_socket;
    
    struct timespec timeout;
    timeout.tv_sec = 1;

    while (!wasSigHup) {
        // Копируем набор дескрипторов, так как pselect изменяет его
        fd_set temp_fds = fds;
      
        for (int i = 0; i < active_clients; i++) {
            FD_SET(client_sockets[i], &fds);
            if (client_sockets[i] > max_fd) max_fd = client_sockets[i];
        }

        if (pselect(max_fd + 1, &temp_fds, NULL, NULL, &timeout, &origMask) == -1) {
            if (errno != EINTR) {
                perror("pselect error");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        for (int i = server_socket; i <= max_fd; i++) {
            if (!FD_ISSET(i, &temp_fds)) continue;

            if (i == server_socket) {
                // Обработка нового подключения
                struct sockaddr_storage client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

                if (new_socket == -1) {
                    perror("accept error");
                    continue;
                }

                FD_SET(new_socket, &fds);
                client_sockets[active_clients++] = new_socket;

                char client_ip[INET6_ADDRSTRLEN];
                inet_ntop(client_addr.ss_family, get_socket_address((struct sockaddr *)&client_addr), client_ip, INET6_ADDRSTRLEN);
                printf("New connection from %s on socket %d\n", client_ip, new_socket);
            } else {
                // Обработка данных от клиента
                char buffer[1024];
                int bytes_received = recv(i, buffer, sizeof(buffer), 0);

                if (bytes_received <= 0) {
                    if (bytes_received == 0) {
                        printf("Socket %d disconnected\n", i);
                    } else {
                        perror("recv error");
                    }
                    close(i);
                    FD_CLR(i, &fds);

                    int client_index = find_client_index(client_sockets, active_clients, i);
                    client_sockets[client_index] = client_sockets[--active_clients];
                } else {
                    printf("Received from socket %d: %d bytes\n", i, bytes_received);
                }
            }
        }
    }

    printf("Server has been stopped.\n");
    close(server_socket);
    return 0;
}

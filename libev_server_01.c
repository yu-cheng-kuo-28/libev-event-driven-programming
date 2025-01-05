#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ev.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void read_cb(EV_P_ ev_io *w, int revents);
void accept_cb(EV_P_ ev_io *w, int revents);

typedef struct {
    ev_io io_watcher;
    int fd;
} server_t;

void accept_cb(EV_P_ ev_io *w, int revents) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int server_fd = w->fd;
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Accept failed");
        return;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    ev_io* client_watcher = (ev_io*)malloc(sizeof(ev_io));
    if (!client_watcher) {
        perror("Memory allocation failed");
        close(client_fd);
        return;
    }

    ev_io_init(client_watcher, read_cb, client_fd, EV_READ);
    client_watcher->data = (void*)(intptr_t)client_fd;
    ev_io_start(EV_A_ client_watcher);
}

void read_cb(EV_P_ ev_io *w, int revents) {
    char buffer[BUFFER_SIZE];
    int client_fd = (int)(intptr_t)(w->data);

    ssize_t len = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (len > 0) {
        buffer[len] = '\0';
        printf("Received from client: %s\n", buffer);
        
        write(client_fd, buffer, len);
    } else if (len == 0 || (len < 0 && errno != EAGAIN)) {
        printf("Client disconnected or error\n");
        ev_io_stop(EV_A_ w);
        close(client_fd);
        free(w);
    }
}

int main() {
    struct sockaddr_in server_addr;
    int server_fd;
    struct ev_loop *loop = EV_DEFAULT;
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }

    printf("Server listening on port %d\n", PORT);

    server_t server;
    server.fd = server_fd;
    ev_io_init(&server.io_watcher, accept_cb, server_fd, EV_READ);
    server.io_watcher.data = &server;
    ev_io_start(loop, &server.io_watcher);

    ev_run(loop, 0);

    close(server_fd);
    return 0;
}

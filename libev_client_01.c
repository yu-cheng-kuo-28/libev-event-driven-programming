#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ev.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

#define SERVER_IP "XX.XX.XX.XX"
#define SERVER_PORT 8080
#define MESSAGE "Hello from the client!"

typedef struct {
    ev_io io_watcher;
    int fd;
    struct ev_loop *loop;
} client_t;

void read_cb(EV_P_ ev_io *w, int revents) {
    char buffer[1024];
    client_t *client = (client_t*)w->data;
    
    ssize_t len = read(client->fd, buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        printf("Received from server: %s\n", buffer);
        
        ev_io_stop(client->loop, w);
        close(client->fd);
        ev_break(client->loop, EVBREAK_ALL);
    } else if (len == 0 || (len < 0 && errno != EAGAIN)) {
        printf("Server disconnected or error\n");
        ev_io_stop(client->loop, w);
        close(client->fd);
        ev_break(client->loop, EVBREAK_ALL);
    }
}

void write_cb(EV_P_ ev_io *w, int revents) {
    client_t *client = (client_t*)w->data;
    ssize_t len = write(client->fd, MESSAGE, strlen(MESSAGE));
    
    if (len < 0) {
        if (errno != EAGAIN) {
            perror("Write failed");
            ev_io_stop(client->loop, w);
            close(client->fd);
            ev_break(client->loop, EVBREAK_ALL);
        }
        return;
    }

    printf("Sent message to server: %s\n", MESSAGE);

    ev_io_stop(client->loop, w);
    ev_io_init(&client->io_watcher, read_cb, client->fd, EV_READ);
    client->io_watcher.data = client;
    ev_io_start(client->loop, &client->io_watcher);
}

int main() {
    struct sockaddr_in server_addr;
    client_t client;
    struct ev_loop *loop = EV_DEFAULT;

    client.loop = loop;
    
    client.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client.fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    int flags = fcntl(client.fd, F_GETFL, 0);
    fcntl(client.fd, F_SETFL, flags | O_NONBLOCK);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client.fd);
        return -1;
    }

    if (connect(client.fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("Connection failed");
            close(client.fd);
            return -1;
        }
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);

    ev_io_init(&client.io_watcher, write_cb, client.fd, EV_WRITE);
    client.io_watcher.data = &client;
    ev_io_start(loop, &client.io_watcher);

    ev_run(loop, 0);

    return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>

typedef int bool;
#define true 1
#define false 0

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

typedef struct {
    bool ok;
    int sockets[2];
} WaitResult;

WaitResult wait_for_players() {
    WaitResult result = (WaitResult){ false };

    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        printf("Failed to create socket\n");
        return result;
    }

    int yes = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(1982),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(listen_socket, (sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listen_socket);
        printf("Failed to bind socket\n");
        return result;
    }

    if (listen(listen_socket, 1) < 0) {
        close(listen_socket);
        printf("Failed to listen on socket\n");
        return result;
    }  

    for (int i = 0; i < 2; i++) {
        result.sockets[i] = accept(listen_socket, NULL, NULL);
        if (result.sockets[i] < 0) {
            close(listen_socket);
            printf("Failed to accept connection\n");
            return result;
        }
    }

    result.ok = true;
    close(listen_socket);
    return result;
}

void play_game(int *sockets) {
    
}

void run_server() {
    WaitResult result = wait_for_players();
    if (!result.ok) {
        fprintf(stderr, "Womp, womp...\n");
        return;
    }

    play_game(result.sockets);
}


/*
    // loop polling for incoming connections
    struct pollfd sockets[3];
    sockets[0].fd = listen_socket;
    sockets[0].events = POLLIN;

    int new_conn = 0;
    int poll_result = 0;
    int num_connections = 0;

    while (1) {
        poll_result = poll(sockets, 1 + num_connections, -1);
        if (poll_result < 0) {
            perror("Poll failed");
            return;
        }

        if (sockets[0].revents & POLLIN) {
            // get the connection and update the meta
            new_conn = accept(listen_fd, NULL, NULL);
            num_connections++;

            // add the new connection to the poll list
            sockets[num_connections].fd = new_conn;
            sockets[num_connections].events = POLLIN;
        }

        // walk through list of connections and check if any of them has data to read
        for (int i = 1; i <= num_connections; i++) {
            if (sockets[i].revents & POLLIN) {
                char buf[1024];
                int n = read(sockets[i].fd, buf, sizeof(buf));

                if (n > 0) {
                    // printf("Received from client: %.*s\n", n, buf);
                    for (int send_conn = 1; send_conn <= num_connections; send_conn++) {
                        write(sockets[send_conn].fd, buf, n);
                    }
                }
            }
        }
    }*/

void run_client() {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(1982),
        .sin_addr.s_addr = htonl(0x7F000001)
    };

    int result = connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (result < 0) {
        perror("Connect failed");
        return;
    }

    const char *message = "Hello, Echo Server!";
    printf("Sending to server: %s\n", message);
    write(sock_fd, message, strlen(message));

    char buf[1024];
    int n = read(sock_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received from server: %s\n", buf);
    }

    close(sock_fd);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "server") == 0) {
            run_server();
            return 0;
        } else if (strcmp(argv[1], "client") == 0) {
            run_client();
            return 0;
        }
    }

    printf("Usage: %s server|client\n", argv[0]);
    return 1;
}

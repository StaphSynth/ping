#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

void run_server() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(1982),
        .sin_addr.s_addr = INADDR_ANY
    };
    
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 1);
    
    printf("Listening on port 1982\n");
    int conn = accept(listen_fd, NULL, NULL);
    close(listen_fd);
    
    char buf[1024];
    int n;
    while ((n = read(conn, buf, sizeof(buf))) > 0) {
        printf("Received from client: %.*s\n", n, buf);
        write(conn, buf, n);
    }

    printf("I am going to sleep now. Forever.\n");
    
    close(conn);
}

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

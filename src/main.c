#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>
#include <SDL2/SDL.h>

typedef int bool;
#define true 1
#define false 0

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

static const int BALL_SPEED_X = 5;
static const int BALL_SPEED_Y = 5;
static const int BALL_SIZE = 10;
static const int WINDOW_WIDTH = 512;
static const int WINDOW_HEIGHT = 384;

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

typedef struct {
  int32_t ball_x;
  int32_t ball_y;
  int32_t ball_velocity_x;
  int32_t ball_velocity_y;
  int32_t paddle1_y;
  int32_t paddle2_y;
} GameState;

typedef struct {
    int32_t ball_x;
    int32_t ball_y;
} GameStateMessage;

GameState init_game_state() {
    GameState state;
    state.ball_x = 40;
    state.ball_y = 12;
    state.ball_velocity_x = BALL_SPEED_X;
    state.ball_velocity_y = BALL_SPEED_Y;
    state.paddle1_y = 10;
    state.paddle2_y = 10;

    return state;
}



void play_game(int *sockets) {
    GameState state = init_game_state();
    GameStateMessage message;

    // loop to send game state to clients and read updates from them
    while (1) {
        //// Game logic update
        state.ball_x += state.ball_velocity_x;
        state.ball_y += state.ball_velocity_y;

        if (state.ball_x <= 0) {
            state.ball_x = 0;
            state.ball_velocity_x = -state.ball_velocity_x;
        } else if (state.ball_x >= WINDOW_WIDTH - BALL_SIZE) {
            state.ball_x = WINDOW_WIDTH - BALL_SIZE;
            state.ball_velocity_x = -state.ball_velocity_x;
        }

        if (state.ball_y <= 0) {
            state.ball_y = 0;
            state.ball_velocity_y = -state.ball_velocity_y;
        } else if (state.ball_y >= WINDOW_HEIGHT - BALL_SIZE) {
            state.ball_y = WINDOW_HEIGHT - BALL_SIZE;
            state.ball_velocity_y = -state.ball_velocity_y;
        }

        //// send game state to clients

        // update message
        message.ball_x = htonl(state.ball_x);
        message.ball_y = htonl(state.ball_y);

        // send message to clients
        for (int i = 0; i < 2; i++) {
            int bytes_written = write(sockets[i], &message, sizeof(message));

            if (bytes_written < 0) {
                printf("Write failed, exiting.\n");
                return;
            }
        }

        //// sleep
        usleep(50000); // sleep for 50ms
    }
}

void run_server() {
    signal( SIGPIPE, SIG_IGN ); // ignore SIGPIPE to prevent crashing when writing to a closed socket

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

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "SDL Rectangle",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    while (1) {
        GameStateMessage message;
        int bytes_read = read(sock_fd, &message, sizeof(message));

        if (bytes_read < 0) {
            perror("Failed to read game state");
            return;
        }

        uint32_t ball_x = ntohl(message.ball_x);
        uint32_t ball_y = ntohl(message.ball_y);

        // Clear to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw a filled red rectangle
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect = { ball_x, ball_y, BALL_SIZE, BALL_SIZE };  // x, y, w, h
        SDL_RenderFillRect(renderer, &rect);

        SDL_RenderPresent(renderer);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
        }
    }
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

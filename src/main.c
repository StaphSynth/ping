#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef int bool;
#define true 1
#define false 0

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

static const int BALL_SPEED_X = 10;
static const int BALL_SPEED_Y = 10;
static const int BALL_SIZE = 10;
static const int WINDOW_WIDTH = 512;
static const int WINDOW_HEIGHT = 384;
static const int GUTTER_WIDTH = 10;
static const int PADDLE_WIDTH = 10;
static const int PADDLE_HEIGHT = 60;

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
    int32_t paddle_y[2];
    int32_t scores[2];
} GameState;

typedef struct {
    int32_t ball_x;
    int32_t ball_y;
    int32_t paddle_y[2];
    int32_t paddle_id;
    int32_t scores[2];
} GameStateMessage;

GameState init_game_state() {
    GameState state;
    state.ball_x = 40;
    state.ball_y = 12;
    state.ball_velocity_x = BALL_SPEED_X;
    state.ball_velocity_y = BALL_SPEED_Y;
    state.paddle_y[0] = 10;
    state.paddle_y[1] = 10;
    state.scores[0] = 0;
    state.scores[1] = 0;

    return state;
}

bool collision_with_left_wall(GameState *state) {
    return state->ball_x <= 0;
}

bool collision_with_right_wall(GameState *state) {
    return state->ball_x >= WINDOW_WIDTH - BALL_SIZE;
}

bool collision_with_y_wall(GameState *state) {
    return state->ball_y <= 0 || state->ball_y >= WINDOW_HEIGHT - BALL_SIZE;
}

int32_t clamp(int32_t value, int32_t min, int32_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void reset_ball(GameState *state) {
    state->ball_x = WINDOW_WIDTH / 2 - BALL_SIZE / 2;
    state->ball_y = WINDOW_HEIGHT / 2 - BALL_SIZE / 2;
    state->ball_velocity_x = -state->ball_velocity_x; // send ball towards the player who just scored
    state->ball_velocity_y = BALL_SPEED_Y; // reset vertical speed
}

bool collision_with_paddle(GameState *state, int paddle_id) {
    int paddle_x = (paddle_id == 0) ? GUTTER_WIDTH : (WINDOW_WIDTH - GUTTER_WIDTH - PADDLE_WIDTH);
    int paddle_y = state->paddle_y[paddle_id];

    if (paddle_id == 0 && state->ball_velocity_x > 0) return false;
    if (paddle_id == 1 && state->ball_velocity_x < 0) return false;

    return state->ball_x < paddle_x + PADDLE_WIDTH &&
           state->ball_x + BALL_SIZE > paddle_x &&
           state->ball_y < paddle_y + PADDLE_HEIGHT &&
           state->ball_y + BALL_SIZE > paddle_y;
}

void handle_collisions(GameState *state) {
    if (collision_with_left_wall(state)){
        // reset ball to center
        reset_ball(state);
        state->scores[1]++; // update score
    }
    else if (collision_with_right_wall(state)){
        // reset ball to center
        reset_ball(state);
        state->scores[0]++;
    }

    if (collision_with_y_wall(state)) {
        state->ball_y = clamp(state->ball_y, 0, WINDOW_HEIGHT - BALL_SIZE);
        state->ball_velocity_y = -state->ball_velocity_y;
    }

    for (int i = 0; i < 2; i++) {
        if (collision_with_paddle(state, i)) {
            state->ball_velocity_x = -state->ball_velocity_x;

            // add some vertical velocity based on where the ball hit the paddle
            int paddle_center = state->paddle_y[i] + PADDLE_HEIGHT / 2;
            int ball_center = state->ball_y + BALL_SIZE / 2;
            int offset = ball_center - paddle_center;
            state->ball_velocity_y = offset / (PADDLE_HEIGHT / 2) * BALL_SPEED_Y;
        }
    }
}

void play_game(int *sockets) {
    GameState state = init_game_state();
    GameStateMessage message;

    // loop to send game state to clients and read updates from them
    while (1) {
        //// Game logic update
        state.ball_x += state.ball_velocity_x;
        state.ball_y += state.ball_velocity_y;

        handle_collisions(&state);

        //// send game state to clients

        // update message
        message.ball_x = htonl(state.ball_x);
        message.ball_y = htonl(state.ball_y);
        message.paddle_y[0] = htonl(state.paddle_y[0]);
        message.paddle_y[1] = htonl(state.paddle_y[1]);
        message.scores[0] = htonl(state.scores[0]);
        message.scores[1] = htonl(state.scores[1]);

        // send message to clients
        for (int i = 0; i < 2; i++) {
            message.paddle_id = htonl(i);
            int bytes_written = write(sockets[i], &message, sizeof(message));

            if (bytes_written < 0) {
                printf("Write failed, exiting.\n");
                return;
            }
        }

        //// sleep
        usleep(50000); // sleep for 50ms

        //// Read client updates
        for (int i = 0; i < 2; i++)
        {
            uint32_t paddle_y_update;
            size_t bytes_read = read(sockets[i], &paddle_y_update, sizeof(paddle_y_update));

            if (bytes_read < sizeof(paddle_y_update))
            {
                printf("Read failed, exiting.\n");
                return;
            }

            state.paddle_y[i] = ntohl(paddle_y_update);
        }
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

void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, white);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest_rect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void run_client(uint32_t ip_address) {

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(1982),
        .sin_addr.s_addr = ip_address
    };

    int result = connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (result < 0) {
        perror("Connect failed");
        return;
    }

    SDL_Init(SDL_INIT_VIDEO);

    TTF_Init();
    TTF_Font *font = TTF_OpenFont("./font.ttf", 24);

    SDL_Window *window = SDL_CreateWindow(
        "SDL Rectangle",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    int paddle_y_velocity = 0;

    while (1) {
        GameStateMessage message;
        int bytes_read = read(sock_fd, &message, sizeof(message));

        if (bytes_read < 0) {
            perror("Failed to read game state");
            return;
        }

        uint32_t ball_x = ntohl(message.ball_x);
        uint32_t ball_y = ntohl(message.ball_y);
        uint32_t paddle_y[2];
        uint32_t paddle_id = ntohl(message.paddle_id);
        paddle_y[0] = ntohl(message.paddle_y[0]);
        paddle_y[1] = ntohl(message.paddle_y[1]);
        uint32_t scores[2];
        scores[0] = ntohl(message.scores[0]);
        scores[1] = ntohl(message.scores[1]);

        // Clear to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw a filled red rectangle for the ball.
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect = { ball_x, ball_y, BALL_SIZE, BALL_SIZE };  // x, y, w, h
        SDL_RenderFillRect(renderer, &rect);

        // Draw each paddle.
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect paddle_0 = { GUTTER_WIDTH, paddle_y[0], PADDLE_WIDTH, PADDLE_HEIGHT };  // x, y, w, h
        SDL_Rect paddle_1 = { WINDOW_WIDTH - GUTTER_WIDTH - PADDLE_WIDTH, paddle_y[1], PADDLE_WIDTH, PADDLE_HEIGHT };  // x, y, w, h
        SDL_RenderFillRect(renderer, &paddle_0);
        SDL_RenderFillRect(renderer, &paddle_1);

        // Draw scores
        for (int i = 0; i < 2; i++) {
            char score_text[16];
            snprintf(score_text, sizeof(score_text), " %d", scores[i]);
            draw_text(renderer, font, score_text, i == 0 ? 20 : WINDOW_WIDTH - 50, 20);
        }

        SDL_RenderPresent(renderer);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_UP) {
                    paddle_y_velocity = -10;
                } else if (e.key.keysym.sym == SDLK_DOWN) {
                    paddle_y_velocity = 10;
                }
            }

            if (e.type == SDL_KEYUP) {
                if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_DOWN) {
                    paddle_y_velocity = 0;
                }
            }

        }

        // send paddle update to server
        int32_t paddle_y_update = paddle_y[paddle_id] + paddle_y_velocity;
        paddle_y_update = htonl(paddle_y_update);
        int bytes_written = write(sock_fd, &paddle_y_update, sizeof(paddle_y_update));
        if (bytes_written < 0) {
            perror("Failed to send paddle update");
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "server") == 0) {
            run_server();
            return 0;
        } else if (strcmp(argv[1], "client") == 0) {
            const char* ip_address = "127.0.0.1";
            if (argc > 2) {
                ip_address = argv[2];
            }

            // Try to parse IP address into uint32_t.
            uint32_t ip_addr_int = inet_addr(ip_address);
            if (ip_addr_int == INADDR_NONE) {
                fprintf(stderr, "Invalid IP address: %s\n", ip_address);
                return 1;
            }

            run_client(ip_addr_int);
            return 0;
        }
    }

    printf("Usage: %s server|client\n", argv[0]);
    return 1;
}

#include "SDL.h"
#include "SDL_image.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include "common.h"

#define WIN_WIDTH       800
#define WIN_HEIGHT      600
#define MOONLANDER_SIZE 64

#define ENGINE_OFF    0
#define ENGINE_BOTTOM 0b0001
#define ENGINE_LEFT   0b0010
#define ENGINE_RIGHT  0b0100
#define ENGINE_TOP    0b1000

#define ZERO_TRESHOLD   0.1
#define ACCEL           0.2
#define GRAVITY         0.05
#define ROTATE_SPEED    4.0

#define SOCKET_CODE_ENGINE_BOTTOM_ON  0b00000001
#define SOCKET_CODE_ENGINE_LEFT_ON    0b00000010
#define SOCKET_CODE_ENGINE_RIGHT_ON   0b00000100
#define SOCKET_CODE_ENGINE_TOP_ON     0b00001000
#define SOCKET_CODE_ENGINE_BOTTOM_OFF 0b00010000
#define SOCKET_CODE_ENGINE_LEFT_OFF   0b00100000
#define SOCKET_CODE_ENGINE_RIGHT_OFF  0b01000000
#define SOCKET_CODE_ENGINE_TOP_OFF    0b10000000

static uint8_t socket_engine_status = 0;

struct obj2d_t {
    double x;
    double y;
    double vx;
    double vy;
    double dir;
};

struct static_asset_t {
    SDL_Texture * tex;
    SDL_Rect frame;
};

struct game_t {
    struct obj2d_t * const moonlander;
    uint8_t engine;
    struct static_asset_t * const rocket;
    struct static_asset_t * const engine_top;
    struct static_asset_t * const engine_bottom;
    struct static_asset_t * const engine_left;
    struct static_asset_t * const engine_right;
};

void free_game(struct game_t * game) {
    // noop
}

void move(struct obj2d_t * const obj2d) {
    obj2d->y += obj2d->vy;
    obj2d->x += obj2d->vx;

    if (fabs(obj2d->y) < ZERO_TRESHOLD) obj2d->y = 0.0;
    if (fabs(obj2d->x) < ZERO_TRESHOLD) obj2d->x = 0.0;

    // Re-flow into window.
    if (obj2d->x < 0) obj2d->x += WIN_WIDTH;
    if (obj2d->y < 0) obj2d->y += WIN_HEIGHT;
    if (obj2d->x >= WIN_WIDTH) obj2d->x -= WIN_WIDTH;
    if (obj2d->y >= WIN_HEIGHT) obj2d->y -= WIN_HEIGHT;
}

double deg2rad(double dir) {
    return (dir / 180) * M_PI;
}

void update_game_status(struct game_t * const game) {
    double vx = 0.0;
    double vy = 0.0;
    if (game->engine & ENGINE_BOTTOM) {
        vy = -cos(deg2rad(game->moonlander->dir)) * ACCEL;
        vx = sin(deg2rad(game->moonlander->dir)) * ACCEL;
    }
    if (game->engine & ENGINE_TOP) {
        vy = cos(deg2rad(game->moonlander->dir)) * ACCEL;
        vx = -sin(deg2rad(game->moonlander->dir)) * ACCEL;
    }

    game->moonlander->vy += vy;
    game->moonlander->vx += vx;

    if (game->engine & ENGINE_LEFT) game->moonlander->dir += ROTATE_SPEED;
    if (game->engine & ENGINE_RIGHT) game->moonlander->dir -= ROTATE_SPEED;
    if (game->moonlander->dir >= 360) {
        game->moonlander->dir -= 360;
    } else if (game->moonlander->dir < 0) {
        game->moonlander->dir += 360;
    }

    move(game->moonlander);
}

void interact_game_engine(struct game_t * const game, const uint8_t engine_mask) {
    game->engine |= engine_mask;
}

void draw(SDL_Renderer * const renderer, SDL_Window * const win, struct game_t * const game) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    game->rocket->frame.x = game->moonlander->x;
    game->rocket->frame.y = game->moonlander->y;
    SDL_Point center = { game->rocket->frame.w >> 1, game->rocket->frame.h >> 1 };

    SDL_RenderCopyEx(renderer, game->rocket->tex, NULL, &game->rocket->frame, game->moonlander->dir, &center, SDL_FLIP_NONE);

    if (game->engine & ENGINE_BOTTOM) {
        game->engine_bottom->frame.x = game->moonlander->x;
        game->engine_bottom->frame.y = game->moonlander->y;
        SDL_RenderCopyEx(renderer, game->engine_bottom->tex, NULL, &game->engine_bottom->frame, game->moonlander->dir, &center, SDL_FLIP_NONE);
    }

    if (game->engine & ENGINE_TOP) {
        game->engine_top->frame.x = game->moonlander->x;
        game->engine_top->frame.y = game->moonlander->y;
        SDL_RenderCopyEx(renderer, game->engine_top->tex, NULL, &game->engine_top->frame, game->moonlander->dir, &center, SDL_FLIP_NONE);
    }

    if (game->engine & ENGINE_LEFT) {
        game->engine_left->frame.x = game->moonlander->x;
        game->engine_left->frame.y = game->moonlander->y;
        SDL_RenderCopyEx(renderer, game->engine_left->tex, NULL, &game->engine_left->frame, game->moonlander->dir, &center, SDL_FLIP_NONE);
    }

    if (game->engine & ENGINE_RIGHT) {
        game->engine_right->frame.x = game->moonlander->x;
        game->engine_right->frame.y = game->moonlander->y;
        SDL_RenderCopyEx(renderer, game->engine_right->tex, NULL, &game->engine_right->frame, game->moonlander->dir, &center, SDL_FLIP_NONE);
    }
}

SDL_Texture * load_png(SDL_Renderer *renderer, char *path) {
    SDL_Surface *surface = IMG_Load(path);
    if (surface == NULL) {
        perror("Cannot load png");
        exit(EXIT_FAILURE);
    }

    SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return texture;
}

static void *socket_comms() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Cannot establish socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("Cannot bind");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 32);

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int cli_sock_fd;

    char buf[BUF_LEN];

    for (;;) {
        cli_sock_fd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len);
        if (cli_sock_fd == -1) {
            perror("Cannot accept");
            exit(EXIT_FAILURE);
        }

        bzero(buf, BUF_LEN);
        read(cli_sock_fd, buf, BUF_LEN);

        uint8_t cmd_bit = (uint8_t)buf[0];
        printf("IN: <%d>\n", cmd_bit);

        switch (cmd_bit) {
        case SOCKET_CODE_ENGINE_BOTTOM_ON:
            socket_engine_status |= ENGINE_BOTTOM;
            break;
        case SOCKET_CODE_ENGINE_BOTTOM_OFF:
            socket_engine_status &= ~ENGINE_BOTTOM;
            break;
        default:
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL Init Error\n");
        exit(EXIT_FAILURE);
    }

    SDL_Window *win = SDL_CreateWindow("Moon Lander", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
    if (win == NULL) {
        printf("Window creation error\n");
        exit(EXIT_FAILURE);
    }

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Renderer creation error\n");
        exit(EXIT_FAILURE);
    }

    SDL_Rect rocket_frame = { 100, 100, 80, 100 };
    struct static_asset_t rocket = { .tex = load_png(renderer, "./rocket.png"), .frame = rocket_frame };
    struct static_asset_t engine_top = { .tex = load_png(renderer, "./engine_top.png"), .frame = rocket_frame };
    struct static_asset_t engine_bottom = { .tex = load_png(renderer, "./engine_bottom.png"), .frame = rocket_frame };
    struct static_asset_t engine_left = { .tex = load_png(renderer, "./engine_left.png"), .frame = rocket_frame };
    struct static_asset_t engine_right = { .tex = load_png(renderer, "./engine_right.png"), .frame = rocket_frame };

    struct obj2d_t moonlander = {
        .x = (WIN_WIDTH - MOONLANDER_SIZE) >> 1,
        .y = (WIN_HEIGHT - MOONLANDER_SIZE) >> 1,
        .vx = 0.0,
        .vy = 0.0,
        .dir = 0.0 };

    struct game_t game = {
        .moonlander     = &moonlander,
        .engine         = ENGINE_OFF,
        .rocket         = &rocket,
        .engine_top     = &engine_top,
        .engine_bottom  = &engine_bottom,
        .engine_left    = &engine_left,
        .engine_right   = &engine_right };

    pthread_t socket_thread;
    if (pthread_create(&socket_thread, NULL, socket_comms, NULL) != 0) {
        perror("Cannot create thread");
        exit(EXIT_FAILURE);
    };

    bool quit = false;
    while (!quit) {
        update_game_status(&game);
        draw(renderer, win, &game);

        game.engine = ENGINE_OFF;

        SDL_RenderPresent(renderer);
        SDL_Delay(10);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
        }

        const uint8_t *key_state = SDL_GetKeyboardState(NULL);
        if (key_state[SDL_SCANCODE_ESCAPE]) {
            quit = true;
        }

        if (key_state[SDL_SCANCODE_DOWN] || socket_engine_status & ENGINE_BOTTOM) {
            interact_game_engine(&game, ENGINE_BOTTOM);
        }

        if (key_state[SDL_SCANCODE_UP]) {
            interact_game_engine(&game, ENGINE_TOP);
        }

        if (key_state[SDL_SCANCODE_LEFT]) {
            interact_game_engine(&game, ENGINE_LEFT);
        }

        if (key_state[SDL_SCANCODE_RIGHT]) {
            interact_game_engine(&game, ENGINE_RIGHT);
        }
    }

    free_game(&game);
    pthread_cancel(socket_thread);

    SDL_Quit();
}

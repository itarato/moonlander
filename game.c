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
#define DOCK_WIDTH      200
#define DOCK_HEIGHT     32
#define SCREEN_RATE_DELAY 10

#define ENGINE_OFF    0
#define ENGINE_BOTTOM 0b0001
#define ENGINE_LEFT   0b0010
#define ENGINE_RIGHT  0b0100
#define ENGINE_TOP    0b1000

#define ZERO_TRESHOLD   0.1
#define ACCEL           0.2
#define GRAVITY         0.02
#define ROTATE_SPEED    2.0

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
    SDL_Rect dock;
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

static inline double deg2rad(double dir) { return (dir / 180) * M_PI; }

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

    // Gravity.
    game->moonlander->vy += GRAVITY;

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

    uint8_t engine_dirs[4] = { ENGINE_BOTTOM, ENGINE_TOP, ENGINE_LEFT, ENGINE_RIGHT};
    struct static_asset_t * const engines[4] = { game->engine_bottom, game->engine_top, game->engine_left, game->engine_right };
    for (int i = 0; i < 4; i++) {
        if (game->engine & engine_dirs[i]) {
            engines[i]->frame.x = game->moonlander->x;
            engines[i]->frame.y = game->moonlander->y;
            SDL_RenderCopyEx(renderer, engines[i]->tex, NULL, &engines[i]->frame, game->moonlander->dir, &center, SDL_FLIP_NONE);
        }
    }

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &game->dock);
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

static void *server_thread_entry() {
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

        // @TODO It could send 2 bytes: [on/off, engine].
        switch (cmd_bit) {
        case SOCKET_CODE_ENGINE_BOTTOM_ON:
            socket_engine_status |= ENGINE_BOTTOM;
            break;
        case SOCKET_CODE_ENGINE_BOTTOM_OFF:
            socket_engine_status &= ~ENGINE_BOTTOM;
            break;
        case SOCKET_CODE_ENGINE_LEFT_ON:
            socket_engine_status |= ENGINE_LEFT;
            break;
        case SOCKET_CODE_ENGINE_LEFT_OFF:
            socket_engine_status &= ~ENGINE_LEFT;
            break;
        case SOCKET_CODE_ENGINE_RIGHT_ON:
            socket_engine_status |= ENGINE_RIGHT;
            break;
        case SOCKET_CODE_ENGINE_RIGHT_OFF:
            socket_engine_status &= ~ENGINE_RIGHT;
            break;
        case SOCKET_CODE_ENGINE_TOP_ON:
            socket_engine_status |= ENGINE_TOP;
            break;
        case SOCKET_CODE_ENGINE_TOP_OFF:
            socket_engine_status &= ~ENGINE_TOP;
            break;
        default:
            break;
        }
    }
}

void restart_game(struct game_t * const game) {
    game->moonlander->x = (WIN_WIDTH - MOONLANDER_SIZE) >> 1;
    game->moonlander->y = (WIN_HEIGHT - MOONLANDER_SIZE) >> 1;
    game->moonlander->vx = 0.0;
    game->moonlander->vy = 0.0;
    game->moonlander->dir = 0.0;

    game->engine = ENGINE_OFF;
}

static inline void panic_with_sdl_image(char *msg) {
    printf("%s: %s\n", msg, IMG_GetError());
    exit(EXIT_FAILURE);
}

static inline void panic_with_sdl(char *msg) {
    printf("%s: %s\n", msg, SDL_GetError());
    exit(EXIT_FAILURE);
}

static inline void panic_with_errno(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) panic_with_sdl("SDL Init Error\n");

    SDL_Window *win = SDL_CreateWindow("Moon Lander", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
    if (win == NULL) panic_with_sdl("Window creation error\n");

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) panic_with_sdl_image("SDL_image could not initialize");

    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) panic_with_sdl("Renderer creation error\n");

    SDL_Rect rocket_frame = { 100, 100, 80, 100 };
    struct static_asset_t rocket        = { .tex = load_png(renderer, "./rocket.png"), .frame = rocket_frame };
    struct static_asset_t engine_top    = { .tex = load_png(renderer, "./engine_top.png"), .frame = rocket_frame };
    struct static_asset_t engine_bottom = { .tex = load_png(renderer, "./engine_bottom.png"), .frame = rocket_frame };
    struct static_asset_t engine_left   = { .tex = load_png(renderer, "./engine_left.png"), .frame = rocket_frame };
    struct static_asset_t engine_right  = { .tex = load_png(renderer, "./engine_right.png"), .frame = rocket_frame };

    struct obj2d_t moonlander;
    struct game_t game = {
        .moonlander     = &moonlander,
        .rocket         = &rocket,
        .engine_top     = &engine_top,
        .engine_bottom  = &engine_bottom,
        .engine_left    = &engine_left,
        .engine_right   = &engine_right,
        .dock           = { (WIN_WIDTH - DOCK_WIDTH) >> 1, WIN_HEIGHT - (DOCK_HEIGHT << 1), DOCK_WIDTH, DOCK_HEIGHT } };
    restart_game(&game);

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, server_thread_entry, NULL) != 0) panic_with_errno("Cannot create thread");

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        update_game_status(&game);
        draw(renderer, win, &game);

        game.engine = ENGINE_OFF;

        SDL_RenderPresent(renderer);
        SDL_Delay(SCREEN_RATE_DELAY);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
        }

        const uint8_t *key_state = SDL_GetKeyboardState(NULL);
        if (key_state[SDL_SCANCODE_DOWN]  || socket_engine_status & ENGINE_BOTTOM)  interact_game_engine(&game, ENGINE_BOTTOM);
        if (key_state[SDL_SCANCODE_UP]    || socket_engine_status & ENGINE_TOP)     interact_game_engine(&game, ENGINE_TOP);
        if (key_state[SDL_SCANCODE_LEFT]  || socket_engine_status & ENGINE_LEFT)    interact_game_engine(&game, ENGINE_LEFT);
        if (key_state[SDL_SCANCODE_RIGHT] || socket_engine_status & ENGINE_RIGHT)   interact_game_engine(&game, ENGINE_RIGHT);
        if (key_state[SDL_SCANCODE_ESCAPE]) quit = true;
        if (key_state[SDL_SCANCODE_N]) restart_game(&game);
    }

    free_game(&game);
    pthread_cancel(server_thread);

    SDL_Quit();
}

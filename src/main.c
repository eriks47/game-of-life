#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <time.h>
#include <stdio.h>
#include <string.h>

int dirs[8][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}, {-1, 1}, {1, -1}};

struct CellChange
{
    int x;
    int y;
    int newval;
    struct CellChange *next;
};

struct GameState
{
    char cells[1000][1000];
    struct CellChange *head;
};

struct Playback
{
    int zoom;
    int speed; // updates per second
    int camera_x;
    int camera_y;
    int predrag_x;
    int predrag_y;
    int predragc_x;
    int predragc_y;
    int dragging;
    int cellsize;
    int paused;
    int initial_render;
};

struct Application
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width;
    int height;
    int is_pressed[SDL_NUM_SCANCODES];
    int done;
    clock_t last_tick;
    clock_t last_second;
    clock_t last_click;
    clock_t click_delay;
    int ticks;
    int frames;
    int click_event;
    TTF_Font *font;
};

struct GameState game_state;
struct Playback playback;
struct Application app;

void init()
{
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // app
    app.window = SDL_CreateWindow("game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);
    memset(app.is_pressed, 0, sizeof(*app.is_pressed) * SDL_NUM_SCANCODES);
    app.font = TTF_OpenFont("/usr/share/fonts/TTF/JetBrainsMono-Bold.ttf", 36);
    app.width = 800;
    app.height = 600;
    app.done = 0;
    app.last_tick = 0;
    app.last_second = 0;
    app.last_click = 0;
    app.click_delay = 10;
    app.ticks = 0;
    app.frames = 0;
    app.click_event = 0;

    // playback
    playback.zoom = 20;
    playback.speed = 1;
    playback.camera_x = 10;
    playback.camera_y = 10;
    playback.predrag_x = 10;
    playback.predrag_y = 10;
    playback.predragc_x = 10;
    playback.predragc_y = 10;
    playback.dragging = 0;
    playback.cellsize = 4;
    playback.paused = 1;
    playback.initial_render = 0;

    // game state
    memset(game_state.cells, 0, sizeof(char) * 1000 * 1000);
}

void add_cell_change(int x, int y, int newval)
{
    struct CellChange *change = malloc(sizeof(*change));
    change->newval = newval;
    change->x = x;
    change->y = y;
    change->next = game_state.head;
    game_state.head = change;
}


void handle_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            app.done = 1;
        } else if (event.type == SDL_KEYUP && event.key.keysym.scancode == SDL_SCANCODE_MINUS) {
            playback.zoom -= 1;
            playback.zoom = SDL_max(playback.zoom, 1);
        } else if (event.type == SDL_KEYUP && event.key.keysym.scancode == SDL_SCANCODE_EQUALS) {
            playback.zoom += 1;
        } else if (event.type == SDL_KEYUP && event.key.keysym.scancode == SDL_SCANCODE_9) {
            playback.speed -= 1;
            playback.speed = SDL_max(playback.speed, 1);
        } else if (event.type == SDL_KEYUP && event.key.keysym.scancode == SDL_SCANCODE_0) {
            playback.speed += 1;
        } else if (event.type == SDL_KEYDOWN) {
            app.is_pressed[event.key.keysym.scancode] = 1; 
        } else if (event.type == SDL_KEYUP) {
            app.is_pressed[event.key.keysym.scancode] = 0;
        } else if (event.type == SDL_MOUSEMOTION && app.is_pressed[SDL_SCANCODE_SPACE]) {
            int mouse_x;
            int mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            if (!playback.dragging) {
                playback.predrag_x = mouse_x;
                playback.predrag_y = mouse_y;
                playback.predragc_x = playback.camera_x;
                playback.predragc_y = playback.camera_y;
            }
            playback.dragging = 1;
            playback.camera_x = playback.predragc_x + (playback.predrag_x - mouse_x);
            playback.camera_y =  playback.predragc_y + (playback.predrag_y - mouse_y);
        } else if (event.type == SDL_MOUSEBUTTONDOWN && clock() - app.last_click > CLOCKS_PER_SEC / app.click_delay) {
            app.last_click = clock();
            app.click_event = 1;
        }
    }

    for (struct CellChange *change = game_state.head; change;) {
        game_state.cells[change->y][change->x] = change->newval;
        if (change == game_state.head) {
            game_state.head = NULL;
        }
        struct CellChange *next = change->next;
        free(change);
        change = next;
    }

    if (!app.is_pressed[SDL_SCANCODE_SPACE]) {
        playback.dragging = 0;
    }

    if (app.is_pressed[SDL_SCANCODE_P]) {
        playback.paused = 1;
    }
    if (app.is_pressed[SDL_SCANCODE_O]) {
        playback.paused = 0;
    }

    if (app.click_event && !playback.dragging) {
        int mouse_x;
        int mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        int world_x = playback.camera_x + mouse_x - app.width / 2;
        int world_y = playback.camera_y + mouse_y - app.height / 2;
        int x = world_x / (playback.cellsize * playback.zoom);
        int y = world_y / (playback.cellsize * playback.zoom);
        if (x >= 0 && x <= 1000 && y >= 0 && y <= 1000) {
            add_cell_change(x, y, game_state.cells[y][x] ^ 1);
        }
        app.click_event = 0;
    }

}

void tick()
{
    // if playback is paused do not step through state
    if (!playback.paused) {
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 100; ++x) {
                int neighbours = 0;
                for (int i = 0; i < 8; ++i) {
                    int new_y = y + dirs[i][1];
                    int new_x = x + dirs[i][0];
                    if (new_x >= 0 && new_x < 100 && new_y >= 0 && new_y < 100 && game_state.cells[new_y][new_x]) {
                        ++neighbours;
                    }
                }
                if (game_state.cells[y][x] && (neighbours < 2 || neighbours > 3)) {
                    add_cell_change(x, y, 0);
                } else if (!game_state.cells[y][x] && neighbours == 3) {
                    add_cell_change(x, y, 1);
                }
            }
        }
    }
}

SDL_Rect cell_rect(int x, int y)
{
    int real_cellsize = playback.cellsize * playback.zoom;
    return (SDL_Rect) {
        x * real_cellsize - playback.camera_x + app.width / 2,
        y * real_cellsize - playback.camera_y + app.height / 2, 
        real_cellsize, 
        real_cellsize
    };
}

void draw_boxed_text(int x, int y, const char *text)
{
    SDL_Surface *surface = TTF_RenderText_Solid(app.font, text, (SDL_Color){255, 255, 255, 255});
    SDL_Texture *texture = SDL_CreateTextureFromSurface(app.renderer, surface);

    int len = strlen(text);
    SDL_Rect dest =  {x, y, 30 * len + 10, 60};
    SDL_SetRenderDrawColor(app.renderer, 1, 0, 1, 255);
    SDL_RenderFillRect(app.renderer, &dest);
    SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(app.renderer, &dest);
    dest.x += 5;
    dest.y += 5;
    dest.w -= 10;
    dest.h -= 10;
    SDL_RenderCopy(app.renderer, texture, NULL, &dest);
}

void render()
{
    SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255);
    SDL_RenderClear(app.renderer);
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            SDL_Rect cell = cell_rect(x, y);
            if (game_state.cells[y][x]) {
                SDL_SetRenderDrawColor(app.renderer, 1, 0, 1, 255);
                SDL_RenderFillRect(app.renderer, &cell);
            }
            SDL_SetRenderDrawColor(app.renderer, 193, 193, 193, 255);
            SDL_RenderDrawRect(app.renderer, &cell);
        }
    }

    // render ui
    if (playback.paused) {
        draw_boxed_text(35, 35, "Paused");
    } else {
        draw_boxed_text(35, 35, "Playing");
    }

    char buff[10];
    sprintf(buff, "Zoom:%d", playback.zoom);
    draw_boxed_text(app.width - 250, 35, buff);

    sprintf(buff, "Speed:%d", playback.speed);
    draw_boxed_text(app.width - 520, 35, buff);

    SDL_RenderPresent(app.renderer);
    playback.initial_render = 1;
}

void destroy()
{
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    TTF_CloseFont(app.font);
    TTF_Quit();
}

int main()
{
    init();
    while (!app.done) {
        clock_t tick_ellapsed = clock() - app.last_tick;
        clock_t second_ellapsed = clock() - app.last_second;

        if (second_ellapsed >= CLOCKS_PER_SEC) {
            app.last_second = clock();
            app.frames = 0;
            app.ticks = 0;
        }

        handle_events();

        if (tick_ellapsed >= CLOCKS_PER_SEC / playback.speed) {
            tick();
            app.last_tick = clock();
            ++app.ticks;
            ++app.frames;
        }
        render();
        SDL_Delay(10);
    }
    destroy();
}

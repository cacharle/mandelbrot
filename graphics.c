#include <stdbool.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include "header.h"

#define WINDOW_TITLE "Mandelbrot"
#define WINDOW_X 20
#define WINDOW_Y 20
#define REFRESH_RATE 2
#define MOVE_RATIO 10
#define ZOOM_RATIO 1.1
#define REAL_LO(state) (state->center.x - state->real_range / 2)
#define REAL_HI(state) (state->center.x + state->real_range / 2)
#define IMAG_LO(state) (state->center.y - state->imag_range / 2)
#define IMAG_HI(state) (state->center.y + state->imag_range / 2)

#define IN_SET_COLOR 0x050505
#define PALETTE_START 0x000022
#define PALETTE_END 0xd62f2f
#define SET_DRAW_COLOR(renderer, c) ( \
        SDL_SetRenderDrawColor(renderer, c.rgb.r, c.rgb.g, c.rgb.b, SDL_ALPHA_OPAQUE));

#define RED_MASK   0x0000ff
#define GREEN_MASK 0x00ff00
#define BLUE_MASK  0xff0000
#define PITCH(ch, width) (ch * width)

static void update(GState *state);
static void event_handler(GState *state);
static Color *create_palette(Color start, Color end);
/* static void move_center(GState *state, int motion_x, int motion_y); */
static void recenter(GState *state, int x, int y);
static void recenter_x(GState *state, int x);
static void recenter_y(GState *state, int y);
static void zoom_in(GState *state, double ratio);
static void zoom_out(GState *state, double ratio);
static void destroy_state(GState *state);
static void error_exit_state(GState *state, const char *msg);
static void error_exit(const char *msg);

GState *graphics_init(Config *config)
{
    Color start, end;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        error_exit("unable to init SDL");
    GState *state = (GState*)malloc(sizeof(GState));
    if (state == NULL)
        error_exit("state allocation failed");
    state->window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y,
                                     config->window_w, config->window_h, 0);
    if (state->window == NULL)
        error_exit_state(state, "unable to create window");
    state->renderer = SDL_CreateRenderer(state->window, -1, 0);
    if (state->renderer == NULL)
        error_exit_state(state, "unable to create renderer");
    start.hexcode = PALETTE_START;
    end.hexcode = PALETTE_END;
    state->palette = create_palette(start, end);
    if (state->palette == NULL)
        error_exit_state(state, "unable to create color palette");
    state->canvas = SDL_CreateTexture(
        state->renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, config->window_w, config->window_h
    );
    if (state->canvas == NULL)
        error_exit_state(state, "unable to create canvas texture");
    state->running = true;
    state->window_w = config->window_w;
    state->window_h = config->window_h;
    state->real_range = config->real_range;
    state->imag_range = config->imag_range;
    state->center.x = config->center_x;
    state->center.y = config->center_y;
    state->in_set_color.hexcode = IN_SET_COLOR;
    state->moving = false;
    state->changed = true;
    return state;
}

void graphics_quit(GState *state)
{
    destroy_state(state);
    SDL_Quit();
}

void graphics_run(GState *state)
{
    while (state->running)
    {
        event_handler(state);
        if (state->changed)
        {
            update(state);
            state->changed = false;
        }
        SDL_Delay(REFRESH_RATE);
    }
}

static void update(GState *state)
{
    void *pixels = mandelbrot_pixels(REAL_LO(state), REAL_HI(state),
                                     IMAG_LO(state), IMAG_HI(state),
                                     state->window_w, state->window_h, state->palette);
    if (pixels == NULL)
        error_exit_state(state, "unable to create pixels");
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        pixels, state->window_w, state->window_h,
        PIXELS_DEPTH, PITCH(PIXELS_CHANELS, state->window_w),
        RED_MASK, BLUE_MASK, GREEN_MASK, 0
    );
    if (surface == NULL)
    {
        free(pixels);
        error_exit_state(state, "unable to create pixels surface");
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(state->renderer, surface);
    free(pixels);
    SDL_FreeSurface(surface);
    if (texture == NULL)
        error_exit_state(state, "unable to create texture");
    SDL_Rect frame;
    if (SDL_QueryTexture(texture, NULL, NULL, &frame.w, &frame.h) != 0)
    {
        SDL_DestroyTexture(texture);
        error_exit_state(state, "unable to load texture");
    }
    frame.x = 0;
    frame.y = 0;
    if (SDL_RenderCopy(state->renderer, texture, NULL, &frame) != 0)
    {
        SDL_DestroyTexture(texture);
        error_exit_state(state, "unable to render texture");
    }
    SDL_RenderPresent(state->renderer);
    SDL_DestroyTexture(texture);
}

static void event_handler(GState *state)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_QUIT:
                state->running = false;
                break;
            case SDL_KEYDOWN:
                switch (e.key.keysym.sym)
                {
                    case SDLK_UP:
                    case SDLK_k:
                        state->center.y -= state->imag_range / MOVE_RATIO;
                        break;
                    case SDLK_DOWN:
                    case SDLK_j:
                        state->center.y += state->imag_range / MOVE_RATIO;
                        break;
                    case SDLK_LEFT:
                    case SDLK_h:
                        state->center.x -= state->real_range / MOVE_RATIO;
                        break;
                    case SDLK_RIGHT:
                    case SDLK_l:
                        state->center.x += state->real_range / MOVE_RATIO;
                        break;
                    case SDLK_PLUS:
                    case SDLK_p:
                        zoom_in(state, ZOOM_RATIO);
                        break;
                    case SDLK_MINUS:
                    case SDLK_m:
                        zoom_out(state, ZOOM_RATIO);
                        break;
                    case SDLK_q:
                        state->running = false;
                }
                break;
            case SDL_MOUSEWHEEL:
                if (e.wheel.y == -1)
                    zoom_in(state, ZOOM_RATIO);
                else if (e.wheel.y == 1)
                    zoom_out(state, ZOOM_RATIO);
                break;
            case SDL_MOUSEBUTTONDOWN:
                    printf("> %d, %d\n", e.button.x, e.button.y);
                if (e.button.button == SDL_BUTTON_RIGHT)
                    recenter(state, e.button.x, e.button.y);
            // TODO
            /* case SDL_MOUSEBUTTONUP: */
            /*     if (e.button.button == SDL_BUTTON_LEFT) */
            /*     { */
            /*         if (!state->moving && e.button.state == SDL_PRESSED) */
            /*             state->moving = true; */
            /*         else if (state->moving && e.button.state == SDL_RELEASED) */
            /*             state->moving = false; */
            /* printf("%d %d\n", state->moving, e.button.state); */
            /*     } */
            /*     break; */
            /* case SDL_MOUSEMOTION: */
            /*     if (state->moving) */
            /*     { */
            /*         printf("%d, %d\n", e.motion.x, e.motion.y); */
            /*         move_center(state, e.motion.xrel, e.motion.yrel); */
            /*         printf("%f, %f\n", state->center.x, state->center.y); */
            /*     } */
        }
        state->changed = true;
    }
}

 static Color *create_palette(Color start, Color end)
{
    int red_step = abs(end.rgb.r - start.rgb.r) / MAX_ITERATION;
    int green_step = abs(end.rgb.g - start.rgb.g) / MAX_ITERATION;
    int blue_step = abs(end.rgb.b - start.rgb.b) / MAX_ITERATION;

    Color *palette = (Color*)malloc(sizeof(Color) * (MAX_ITERATION + 1));
    if (palette == NULL)
        return NULL;
    for (int i = 0; i < MAX_ITERATION; i++)
    {
        palette[i].rgb.r = i * red_step + start.rgb.r;
        palette[i].rgb.g = i * green_step + start.rgb.g;
        palette[i].rgb.b = i * blue_step + start.rgb.b;
    }
    palette[MAX_ITERATION].hexcode = 0x0;
    return palette;
}

// TODO
/* static void move_center(GState *state, int motion_x, int motion_y) */
/* { */
/*     printf("%d, %d\n", motion_x, motion_y); */
/*     if (motion_x != 0) */
/*     state->center.x -= map_range(150 - motion_x, 0, state->window_w, */
/*                                  REAL_LO(state), REAL_HI(state)); */
/*     if (motion_y != 0) */
/*     state->center.y -=  map_range(150 - motion_y, 0, state->window_h, */
/*                                   IMAG_LO(state), IMAG_HI(state)); */
/*     printf(">> %f, %f\n", state->center.x, state->center.y); */
/* } */

static void recenter(GState *state, int x, int y)
{
    recenter_x(state, x);
    recenter_y(state, y);
}

static void recenter_x(GState *state, int x)
{
    state->center.x = map_range(x, 0, state->window_w,
                                REAL_LO(state), REAL_HI(state));
}

static void recenter_y(GState *state, int y)
{
    state->center.y = map_range(y, 0, state->window_w,
                                IMAG_LO(state), IMAG_HI(state));
}

static void zoom_in(GState *state, double ratio)
{
    state->real_range /= ratio;
    state->imag_range /= ratio;
}

static void zoom_out(GState *state, double ratio)
{
    state->real_range *= ratio;
    state->imag_range *= ratio;
}

static void destroy_state(GState *state)
{
    if (state == NULL)
        return;
    free(state->palette);
    SDL_DestroyTexture(state->canvas);
    SDL_DestroyRenderer(state->renderer);
    SDL_DestroyWindow(state->window);
    free(state);
}

static void error_exit_state(GState *state, const char *msg)
{
    destroy_state(state);
    error_exit(msg);
}

static void error_exit(const char *msg)
{
    SDL_Log("ERROR: %s: %s", SDL_GetError(), msg);
    SDL_Quit();
    exit(EXIT_FAILURE);
}

#include <stdio.h>
#include <flux-mods/sx/sx.h>
#include <flux-mods/sdl2/SDL/include/SDL.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

int sx_main(void) {
    SDL_Window *window = NULL;
    SDL_Surface *surface = NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "hello sdl2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (window == NULL) {
        fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        return 1;
    }

    surface = SDL_GetWindowSurface(window);
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0x1E, 0x1E, 0x32));
    SDL_UpdateWindowSurface(window);

    SDL_bool running = SDL_TRUE;
    while(running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_WINDOWEVENT:
                switch(event.window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                    running = SDL_FALSE;
                    break;
                }
                break;
            }
        }

        //--* code here *--//
    }
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

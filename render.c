#include "render.h"

#include "scenes.h"
#include "simulation.h"

#include <math.h>
#include <stdio.h>

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int y = -radius; y <= radius; y++) {
        int x_span = (int)sqrt((double)((radius * radius)-(y * y)));
        SDL_RenderDrawLine(renderer, cx - x_span, cy + y, cx + x_span, cy + y);
    }
}

static void draw_trail(SDL_Renderer *renderer, const Body *body) {
    if (body->trail_count < 2) {
        return;
    }
    int oldest = (body->trail_next - body->trail_count + TRAIL_LENGTH) % TRAIL_LENGTH;

    for (int i = 1; i < body->trail_count; i++) {
        int index_a = (oldest + i - 1) % TRAIL_LENGTH;
        int index_b = (oldest + i) % TRAIL_LENGTH;

        double t = (double)i / (double)(body->trail_count - 1);
        Uint8 alpha = (Uint8)(20.0 + (160.0 * t));

        SDL_SetRenderDrawColor(renderer, body->color.r, body->color.g, body->color.b, alpha);
        SDL_RenderDrawLine(
            renderer,
            (int)lround(body->trail[index_a].x),
            (int)lround(body->trail[index_a].y),
            (int)lround(body->trail[index_b].x),
            (int)lround(body->trail[index_b].y)
        );
    }
}

static void draw_spawn_preview(SDL_Renderer *renderer, const SpawnState *spawn) {
    if(!spawn->active) {
        return;
    }

    SDL_Color color = current_spawn_color(spawn);
    SDL_Color ghost_color = color;
    SDL_Color cursor_color = {255, 255, 255, 180};

    ghost_color.a = 120;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 220);
    SDL_RenderDrawLine(
        renderer,
        (int)lround(spawn->start.x),
        (int)lround(spawn->start.y),
        (int)lround(spawn->current.x),
        (int)lround(spawn->current.y)
    );

    draw_filled_circle(
        renderer,
        (int)lround(spawn->start.x),
        (int)lround(spawn->start.y),
        (int)lround(radius_from_mass(spawn->mass)),
        ghost_color
    );

    draw_filled_circle(
        renderer,
        (int)lround(spawn->current.x),
        (int)lround(spawn->current.y),
        3,
        cursor_color
    );
}

void render_simulation(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn) {
    SDL_SetRenderDrawColor(renderer, 8, 12, 20, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < sim->body_count; i++) {
        draw_trail(renderer, &sim->bodies[i]);
    }

    for (int i=0; i < sim->body_count; i++) {
        const Body *body = &sim->bodies[i];
        draw_filled_circle(
            renderer,
            (int)lround(body->position.x),
            (int)lround(body->position.y),
            (int)lround(body->radius),
            body->color
        );
    }

    draw_spawn_preview(renderer, spawn);
    SDL_RenderPresent(renderer);
}

void update_window_title(SDL_Window *window, const Simulation *sim, const SpawnState *spawn, ScenePreset scene,  bool paused) {
    char title[256];

    snprintf(
        title,
        sizeof(title),
        "Gravity Sim | %s | scene %s| bodies %d/%d | spawn mass %.0f | 0-3 switch scenes",
        paused ? "paused" : "running",
        scene_name(scene),
        sim->body_count,
        MAX_BODIES,
        spawn->mass
    );
    SDL_SetWindowTitle(window, title);
}

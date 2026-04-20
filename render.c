#include "render.h"

#include "scenes.h"
#include "simulation.h"

#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>

static const char *HUD_FONT_CANDIDATES[] = {
    "assets/fonts/NotoSans-Regular.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/Library/Fonts/Arial.ttf",
    "C:/Windows/Fonts/arial.ttf"
};

static TTF_Font *g_hud_title_font = NULL;
static TTF_Font *g_hud_body_font = NULL;
static bool g_started_ttf = false;

static TTF_Font *open_font_from_candidates(int point_size) {
    int candidate_count = (int)(sizeof(HUD_FONT_CANDIDATES) / sizeof(HUD_FONT_CANDIDATES[0]));

    for (int i = 0; i < candidate_count; i++) {
        TTF_Font *font = TTF_OpenFont(HUD_FONT_CANDIDATES[i], point_size);

        if (font != NULL) {
            return font;
        }
    }

    return NULL;
}

bool init_render_resources(void) {
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
            return false;
        }

        g_started_ttf = true;
    }

    g_hud_title_font = open_font_from_candidates(22);
    g_hud_body_font = open_font_from_candidates(14);

    if (g_hud_title_font == NULL || g_hud_body_font == NULL) {
        fprintf(stderr, "Failed to load HUD font: %s\n", TTF_GetError());
        shutdown_render_resources();
        return false;
    }

    TTF_SetFontHinting(g_hud_title_font, TTF_HINTING_LIGHT);
    TTF_SetFontHinting(g_hud_body_font, TTF_HINTING_LIGHT);
    return true;
}

void shutdown_render_resources(void) {
    if (g_hud_title_font != NULL) {
        TTF_CloseFont(g_hud_title_font);
        g_hud_title_font = NULL;
    }

    if (g_hud_body_font != NULL) {
        TTF_CloseFont(g_hud_body_font);
        g_hud_body_font = NULL;
    }

    if (g_started_ttf) {
        TTF_Quit();
        g_started_ttf = false;
    }
}

static SDL_Point world_to_screen(Vec2 world, const Camera *camera) {
    SDL_Point point;
    point.x = (int)lround((WINDOW_WIDTH * 0.5) +
                          ((world.x - camera->center.x) / camera->meters_per_pixel));
    point.y = (int)lround((WINDOW_HEIGHT * 0.5) -
                          ((world.y - camera->center.y) / camera->meters_per_pixel));
    return point;
}

Vec2 screen_to_world(int screen_x, int screen_y, const Camera *camera) {
    return vec2(
        camera->center.x + (((double)screen_x - (WINDOW_WIDTH * 0.5)) * camera->meters_per_pixel),
        camera->center.y + (((WINDOW_HEIGHT * 0.5) - (double)screen_y) * camera->meters_per_pixel)
    );
}

static int draw_radius_pixels(double physical_radius, const Camera *camera) {
    double real_radius_pixels = physical_radius / camera->meters_per_pixel;

    // Real radii are too small to read at orbital scale, so the renderer
    // compresses visible size while leaving the physical radius untouched.
    double compressed_radius_pixels = 2.0 + (1.8 * cbrt(physical_radius / 1.0e6));
    double draw_radius = fmax(real_radius_pixels, compressed_radius_pixels);

    if (draw_radius < 4.0) {
        draw_radius = 4.0;
    }

    return (int)lround(draw_radius);
}

static void draw_text_line(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                           SDL_Color color, const char *text) {
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect destination;

    if (font == NULL || text == NULL || text[0] == '\0') {
        return;
    }

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (surface == NULL) {
        return;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    destination.x = x;
    destination.y = y;
    destination.w = surface->w;
    destination.h = surface->h;

    SDL_RenderCopy(renderer, texture, NULL, &destination);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void draw_panel(SDL_Renderer *renderer, int x, int y, int width, int height) {
    SDL_Rect panel = {x, y, width, height};
    SDL_Rect border = {x, y, width, height};

    SDL_SetRenderDrawColor(renderer, 10, 14, 24, 222);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 76, 94, 132, 255);
    SDL_RenderDrawRect(renderer, &border);
}

static void draw_time_scale_bar(SDL_Renderer *renderer, int x, int y, int width, int height,
                                int active_index, int count) {
    int gap = 4;
    int step_width = (width - (gap * (count - 1))) / count;

    for (int i = 0; i < count; i++) {
        SDL_Rect block = {
            x + i * (step_width + gap),
            y,
            step_width,
            height
        };

        if (i == active_index) {
            SDL_SetRenderDrawColor(renderer, 255, 210, 120, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 84, 102, 142, 255);
        }

        SDL_RenderFillRect(renderer, &block);
    }
}

static void format_scientific(char *buffer, size_t buffer_size, double value) {
    snprintf(buffer, buffer_size, "%.3e", value);
}

static double vec_magnitude(Vec2 v) {
    return sqrt((v.x * v.x) + (v.y * v.y));
}

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int y = -radius; y <= radius; y++) {
        int x_span = (int)sqrt((double)((radius * radius) - (y * y)));
        SDL_RenderDrawLine(renderer, cx - x_span, cy + y, cx + x_span, cy + y);
    }
}

static void draw_trail(SDL_Renderer *renderer, const Body *body, const Camera *camera) {
    if (body->trail_count < 2) {
        return;
    }

    int oldest = (body->trail_next - body->trail_count + TRAIL_LENGTH) % TRAIL_LENGTH;

    for (int i = 1; i < body->trail_count; i++) {
        int index_a = (oldest + i - 1) % TRAIL_LENGTH;
        int index_b = (oldest + i) % TRAIL_LENGTH;
        double t = (double)i / (double)(body->trail_count - 1);
        Uint8 alpha = (Uint8)(20.0 + (160.0 * t));
        SDL_Point point_a = world_to_screen(body->trail[index_a], camera);
        SDL_Point point_b = world_to_screen(body->trail[index_b], camera);

        SDL_SetRenderDrawColor(renderer, body->color.r, body->color.g, body->color.b, alpha);
        SDL_RenderDrawLine(renderer, point_a.x, point_a.y, point_b.x, point_b.y);
    }
}

static void draw_spawn_preview(SDL_Renderer *renderer, const SpawnState *spawn, const Camera *camera) {
    if (!spawn->active) {
        return;
    }

    SDL_Color color = current_spawn_color(spawn);
    SDL_Color ghost_color = color;
    SDL_Color cursor_color = {255, 255, 255, 180};
    SDL_Point start = world_to_screen(spawn->start, camera);
    SDL_Point current = world_to_screen(spawn->current, camera);

    ghost_color.a = 120;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 220);
    SDL_RenderDrawLine(renderer, start.x, start.y, current.x, current.y);

    draw_filled_circle(
        renderer,
        start.x,
        start.y,
        draw_radius_pixels(radius_from_mass(spawn->mass), camera),
        ghost_color
    );

    draw_filled_circle(renderer, current.x, current.y, 3, cursor_color);
}

static void draw_hud(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn,
                     const SimulationDiagnostics *diagnostics, double simulated_time_seconds,
                     const Camera *camera, ScenePreset scene, bool paused, double time_scale,
                     int time_scale_index, int time_scale_count) {
    const SDL_Color title_color = {255, 214, 140, 255};
    const SDL_Color text_color = {228, 233, 245, 255};
    SimulationDiagnostics zero_diagnostics = {0};
    char line[160];
    char value[64];
    int panel_x = 18;
    int panel_y = 18;
    int panel_width = 468;
    int text_x = panel_x + 16;
    int title_height;
    int line_y;
    int body_step;
    int panel_height;
    double simulated_days = simulated_time_seconds / 86400.0;
    double view_km_per_pixel = camera->meters_per_pixel / 1000.0;
    double momentum_magnitude;

    if (diagnostics == NULL) {
        diagnostics = &zero_diagnostics;
    }

    momentum_magnitude = vec_magnitude(diagnostics->total_momentum);
    title_height = TTF_FontHeight(g_hud_title_font);
    body_step = TTF_FontLineSkip(g_hud_body_font) + 1;
    panel_height = 28 + title_height + 18 + (18 * body_step) + 18;

    draw_panel(renderer, panel_x, panel_y, panel_width, panel_height);
    draw_text_line(renderer, g_hud_title_font, text_x, panel_y + 14, title_color, "Gravity Sim");

    line_y = panel_y + 14 + title_height + 16;

    snprintf(line, sizeof(line), "Scene: %s", scene_name(scene));
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    snprintf(line, sizeof(line), "Status: %s", paused ? "paused" : "running");
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    snprintf(line, sizeof(line), "Sim days: %.2f", simulated_days);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    snprintf(line, sizeof(line), "Time scale: %.3gx", time_scale);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    draw_time_scale_bar(renderer, panel_x + 302, line_y + 5, 150, 10, time_scale_index, time_scale_count);
    line_y += body_step;

    snprintf(line, sizeof(line), "View scale: %.3g km/px", view_km_per_pixel);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    snprintf(line, sizeof(line), "Bodies: %d/%d", sim->body_count, MAX_BODIES);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    snprintf(line, sizeof(line), "Spawn mass: %.2f Earth", spawn->mass / EARTH_MASS);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step + 2;

    format_scientific(value, sizeof(value), diagnostics->kinetic_energy);
    snprintf(line, sizeof(line), "Kinetic: %s J", value);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    format_scientific(value, sizeof(value), diagnostics->potential_energy);
    snprintf(line, sizeof(line), "Potential: %s J", value);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    format_scientific(value, sizeof(value), diagnostics->total_energy);
    snprintf(line, sizeof(line), "Total energy: %s J", value);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    format_scientific(value, sizeof(value), momentum_magnitude);
    snprintf(line, sizeof(line), "Momentum |P|: %s kg m/s", value);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step;

    format_scientific(value, sizeof(value), diagnostics->angular_momentum_z);
    snprintf(line, sizeof(line), "Angular momentum Lz: %s kg m^2/s", value);
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, line);
    line_y += body_step + 4;

    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, "LMB drag spawn, RMB cancel");
    line_y += body_step;
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, "Wheel zoom, Q/E zoom");
    line_y += body_step;
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, "Middle drag or WASD pan");
    line_y += body_step;
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, "[ ] mass, Shift wheel mass");
    line_y += body_step;
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, "- / = time, T reset, C camera");
    line_y += body_step;
    draw_text_line(renderer, g_hud_body_font, text_x, line_y, text_color, "0-3 scenes, R reset, Space pause");
}

void render_simulation(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn,
                       const SimulationDiagnostics *diagnostics, double simulated_time_seconds,
                       const Camera *camera, ScenePreset scene, bool paused, bool hud_visible,
                       double time_scale, int time_scale_index, int time_scale_count) {
    SDL_SetRenderDrawColor(renderer, 8, 12, 20, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < sim->body_count; i++) {
        draw_trail(renderer, &sim->bodies[i], camera);
    }

    for (int i = 0; i < sim->body_count; i++) {
        const Body *body = &sim->bodies[i];
        SDL_Point point = world_to_screen(body->position, camera);

        draw_filled_circle(
            renderer,
            point.x,
            point.y,
            draw_radius_pixels(body->radius, camera),
            body->color
        );
    }

    draw_spawn_preview(renderer, spawn, camera);

    if (hud_visible) {
        draw_hud(
            renderer,
            sim,
            spawn,
            diagnostics,
            simulated_time_seconds,
            camera,
            scene,
            paused,
            time_scale,
            time_scale_index,
            time_scale_count
        );
    }

    SDL_RenderPresent(renderer);
}

void update_window_title(SDL_Window *window, const Simulation *sim, const SpawnState *spawn,
                         const Camera *camera, ScenePreset scene, bool paused, double time_scale) {
    char title[256];

    snprintf(
        title,
        sizeof(title),
        "Gravity Sim | %s | scene %s | bodies %d/%d | spawn %.2f Earth masses | time %.3gx | %.3g km/px",
        paused ? "paused" : "running",
        scene_name(scene),
        sim->body_count,
        MAX_BODIES,
        spawn->mass / EARTH_MASS,
        time_scale,
        camera->meters_per_pixel / 1000.0
    );
    SDL_SetWindowTitle(window, title);
}

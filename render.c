  #include "render.h"

  #include "scenes.h"
#include "simulation.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>

typedef struct {
    char character;
    Uint8 rows[7];
} Glyph;

static const Glyph FONT[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'/', {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {'=', {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}},
    {'[', {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}},
    {']', {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}}
};

static SDL_Point world_to_screen(Vec2 world) {
      SDL_Point point;
      point.x = (int)lround((WINDOW_WIDTH * 0.5) + (world.x / VIEW_METERS_PER_PIXEL));
      point.y = (int)lround((WINDOW_HEIGHT * 0.5) - (world.y / VIEW_METERS_PER_PIXEL));
      return point;
  }

  Vec2 screen_to_world(int screen_x, int screen_y) {
      return vec2(
          ((double)screen_x - (WINDOW_WIDTH * 0.5)) * VIEW_METERS_PER_PIXEL,
          ((WINDOW_HEIGHT * 0.5) - (double)screen_y) * VIEW_METERS_PER_PIXEL
      );
  }

  static int draw_radius_pixels(double physical_radius) {
      double real_radius_pixels = physical_radius / VIEW_METERS_PER_PIXEL;

      // Real radii are too small to see at orbital scale, so renderer compresses the size for visibility while keeping the physics radius intact
      double compressed_radius_pixels = 2.0 + (1.8 * cbrt(physical_radius / 1.0e6));
      double draw_radius = fmax(real_radius_pixels, compressed_radius_pixels);

      if (draw_radius < 4.0) {
          draw_radius = 4.0;
      }

    return (int)lround(draw_radius);
}

static const Uint8 *glyph_rows(char character) {
    size_t font_count = sizeof(FONT) / sizeof(FONT[0]);
    unsigned char upper = (unsigned char)toupper((unsigned char)character);

    for (size_t i = 0; i < font_count; i++) {
        if ((unsigned char)FONT[i].character == upper) {
            return FONT[i].rows;
        }
    }

    return FONT[0].rows;
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, SDL_Color color, const char *text) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int index = 0; text[index] != '\0'; index++) {
        const Uint8 *rows = glyph_rows(text[index]);
        int glyph_x = x + (index * scale * 6);

        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (rows[row] & (1u << (4 - col))) {
                    SDL_Rect pixel = {
                        glyph_x + (col * scale),
                        y + (row * scale),
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
    }
}

static void draw_panel(SDL_Renderer *renderer, int x, int y, int width, int height) {
    SDL_Rect panel = {x, y, width, height};
    SDL_Rect border = {x, y, width, height};

    SDL_SetRenderDrawColor(renderer, 10, 14, 24, 215);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 90, 110, 150, 255);
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
            SDL_SetRenderDrawColor(renderer, 80, 95, 125, 255);
        }

        SDL_RenderFillRect(renderer, &block);
    }
}

static void scene_label(ScenePreset scene, char *buffer, size_t buffer_size) {
    const char *name = scene_name(scene);
    size_t write_index = 0;

    while (name[write_index] != '\0' && write_index + 1 < buffer_size) {
        char character = name[write_index];
        buffer[write_index] = (character == '-') ? ' ' : (char)toupper((unsigned char)character);
        write_index++;
    }

    buffer[write_index] = '\0';
}

  static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

      for (int y = -radius; y <= radius; y++) {
          int x_span = (int)sqrt((double)((radius * radius) - (y * y)));
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
          SDL_Point point_a = world_to_screen(body->trail[index_a]);
          SDL_Point point_b = world_to_screen(body->trail[index_b]);

          SDL_SetRenderDrawColor(renderer, body->color.r, body->color.g, body->color.b, alpha);
          SDL_RenderDrawLine(
              renderer,
              point_a.x,
              point_a.y,
              point_b.x,
              point_b.y
          );
      }
  }

static void draw_spawn_preview(SDL_Renderer *renderer, const SpawnState *spawn) {
      if (!spawn->active) {
          return;
      }

      SDL_Color color = current_spawn_color(spawn);
      SDL_Color ghost_color = color;
      SDL_Color cursor_color = {255, 255, 255, 180};
      SDL_Point start = world_to_screen(spawn->start);
      SDL_Point current = world_to_screen(spawn->current);

      ghost_color.a = 120;

      SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 220);
      SDL_RenderDrawLine(
          renderer,
          start.x,
          start.y,
          current.x,
          current.y
      );

      draw_filled_circle(
          renderer,
          start.x,
          start.y,
          draw_radius_pixels(radius_from_mass(spawn->mass)),
          ghost_color
      );

      draw_filled_circle(
          renderer,
          current.x,
          current.y,
          3,
          cursor_color
    );
}

static void draw_hud(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn,
                     ScenePreset scene, bool paused, double time_scale,
                     int time_scale_index, int time_scale_count) {
    const SDL_Color title_color = {255, 214, 140, 255};
    const SDL_Color text_color = {220, 228, 245, 255};
    char line[128];
    char scene_buffer[64];

    draw_panel(renderer, 18, 18, 380, 232);
    draw_text(renderer, 32, 32, 3, title_color, "GRAVITY SIM");

    scene_label(scene, scene_buffer, sizeof(scene_buffer));
    snprintf(line, sizeof(line), "SCENE: %s", scene_buffer);
    draw_text(renderer, 32, 60, 2, text_color, line);

    snprintf(line, sizeof(line), "STATUS: %s", paused ? "PAUSED" : "RUNNING");
    draw_text(renderer, 32, 78, 2, text_color, line);

    snprintf(line, sizeof(line), "TIME: %.3gX", time_scale);
    draw_text(renderer, 32, 96, 2, text_color, line);
    draw_time_scale_bar(renderer, 180, 98, 190, 12, time_scale_index, time_scale_count);

    snprintf(line, sizeof(line), "BODIES: %d/%d", sim->body_count, MAX_BODIES);
    draw_text(renderer, 32, 118, 2, text_color, line);

    snprintf(line, sizeof(line), "SPAWN MASS: %.2f EARTH", spawn->mass / EARTH_MASS);
    draw_text(renderer, 32, 134, 2, text_color, line);

    draw_text(renderer, 32, 154, 2, text_color, "LMB DRAG SPAWN");
    draw_text(renderer, 32, 170, 2, text_color, "RMB CANCEL");
    draw_text(renderer, 32, 186, 2, text_color, "WHEEL / [ ] MASS");
    draw_text(renderer, 32, 202, 2, text_color, "- / = TIME   T RESET");
    draw_text(renderer, 32, 218, 2, text_color, "0 1 2 3 SCENES   R RESET");
    draw_text(renderer, 32, 234, 2, text_color, "SPACE PAUSE   H HIDE HUD");
}

void render_simulation(SDL_Renderer *renderer, const Simulation *sim, const SpawnState *spawn,
                       ScenePreset scene, bool paused, bool hud_visible,
                       double time_scale, int time_scale_index, int time_scale_count) {
    SDL_SetRenderDrawColor(renderer, 8, 12, 20, 255);
    SDL_RenderClear(renderer);

      for (int i = 0; i < sim->body_count; i++) {
          draw_trail(renderer, &sim->bodies[i]);
      }

      for (int i = 0; i < sim->body_count; i++) {
          const Body *body = &sim->bodies[i];
          SDL_Point point = world_to_screen(body->position);

          draw_filled_circle(
              renderer,
              point.x,
              point.y,
              draw_radius_pixels(body->radius),
              body->color
          );
    }

    draw_spawn_preview(renderer, spawn);

    if (hud_visible) {
        draw_hud(renderer, sim, spawn, scene, paused, time_scale, time_scale_index, time_scale_count);
    }

    SDL_RenderPresent(renderer);
}

void update_window_title(SDL_Window *window, const Simulation *sim, const SpawnState *spawn,
                         ScenePreset scene, bool paused, double time_scale) {
    char title[256];

    snprintf(
        title,
        sizeof(title),
        "Gravity Sim | %s | scene %s | bodies %d/%d | spawn %.2f Earth masses | time %.3gx",
        paused ? "paused" : "running",
        scene_name(scene),
        sim->body_count,
        MAX_BODIES,
        spawn->mass / EARTH_MASS,
        time_scale
    );
    SDL_SetWindowTitle(window, title);
}

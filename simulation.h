#ifndef SIMULATION_H
#define SIMULATION_H

#include "gravity.h"

Vec2 vec2(double x, double y);
Vec2 vec_add(Vec2 a, Vec2 b);
Vec2 vec_sub(Vec2 a, Vec2 b);
Vec2 vec_scale(Vec2 v, double scale);

double radius_from_mass_density(double mass, double density);
double density_from_mass_radius(double mass, double radius);

double radius_from_mass(double mass);
SDL_Color current_spawn_color(const SpawnState *spawn);
int spawn_color_count(void);
void adjust_spawn_mass(SpawnState *spawn, double delta);
bool add_body(Simulation *sim, Body body);

void push_trail_point(Body *body);
void reset_trail(Body *body);
Body make_body(double x, double y, double vx, double vy, double mass, double radius, double density, SDL_Color color);
SimulationDiagnostics compute_diagnostics(const Simulation *sim);
DiagnosticsBaseline make_diagnostics_baseline(const Simulation *sim);
SimulationDrift compute_diagnostics_drift(const SimulationDiagnostics *current, const DiagnosticsBaseline *baseline);
void step_simulation(Simulation *sim, double dt);

#endif

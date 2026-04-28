#ifndef SIMULATION_H
#define SIMULATION_H

#include "gravity.h"

Vec2 vec2(double x, double y);
Vec2 vec_add(Vec2 a, Vec2 b);
Vec2 vec_sub(Vec2 a, Vec2 b);
Vec2 vec_scale(Vec2 v, double scale);

const char *body_type_name(BodyType type);
double body_type_density(BodyType type);
double body_type_default_mass(BodyType type);
double body_type_min_mass(BodyType type);
double body_type_max_mass(BodyType type);
double body_type_mass_step(BodyType type);
double body_type_mass_display_scale(BodyType type);
const char *body_type_mass_display_unit(BodyType type);
BodyType next_body_type(BodyType type);
void set_spawn_body_type(SpawnState *spawn, BodyType type);

double radius_from_mass_density(double mass, double density);
double density_from_mass_radius(double mass, double radius);
double radius_from_mass_type(double mass, BodyType type);

SDL_Color current_spawn_color(const SpawnState *spawn);
void adjust_spawn_mass(SpawnState *spawn, double step_count);
bool add_body(Simulation *sim, Body body);

void push_trail_point(Body *body);
void reset_trail(Body *body);
Body make_body(double x, double y, double vx, double vy, BodyType type,
               double mass, double radius, SDL_Color color);
SimulationDiagnostics compute_diagnostics(const Simulation *sim);
DiagnosticsBaseline make_diagnostics_baseline(const Simulation *sim);
SimulationDrift compute_diagnostics_drift(const SimulationDiagnostics *current,
                                          const DiagnosticsBaseline *baseline);
const char *integrator_name(IntegratorMode integrator);
void step_simulation(Simulation *sim, double dt, IntegratorMode integrator);

#endif

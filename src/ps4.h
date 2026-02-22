#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

typedef struct PS4Context PS4Context;

typedef struct PS4Config {
  bool set_led_on_connect;
  uint8_t led_r, led_g, led_b;

  // filtro / estabilidade
  bool ignore_virtual_x360; // recomendado true
} PS4Config;

PS4Context* ps4_create(const PS4Config* cfg);
void ps4_destroy(PS4Context* ctx);

void ps4_open_existing(PS4Context* ctx);

// retorna true se consumiu o evento
bool ps4_handle_event(PS4Context* ctx, const SDL_Event* e);

// estado: usa 0..N slots, mas aqui vamos focar em 1 ativo
SDL_JoystickID ps4_get_primary_instance_id(const PS4Context* ctx);

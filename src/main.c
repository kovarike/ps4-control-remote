#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h> // timeBeginPeriod/timeEndPeriod
#pragma comment(lib, "winmm.lib")

#include "ps4.h"
#include "output_vigem_x360_dyn.h"
#include "xinput_map.h"

// ---------- Ajuste fino de latência ----------
static const uint32_t SEND_HZ = 500;               // 500 Hz (2ms) - bom “máximo útil”
static const uint32_t SEND_DT_MS = 1000 / SEND_HZ; // 2 ms

static const float STICK_DEADZONE   = 0.10f;  // teste 0.08..0.12 (0.10 costuma ser bom)
static const float STICK_JITTER_CUT = 0.006f; // mais responsivo que 0.010

// ---------- Inversão (runtime) ----------
// Em XInput (X360), Y+ = pra cima.
// Em SDL (muito comum), “pra cima” vem NEGATIVO.
// Logo: em geral, precisa INVERTER para ficar certo no jogo.
// Se mesmo invertendo continuar errado no seu PC, use F7/F8 em runtime.
static bool g_invert_left_y  = true;
static bool g_invert_right_y = true;

// imprime debug do eixo quando ultrapassa um limiar
static bool g_axis_debug = false;

// Negação segura para int16 (corrige -32768)
static inline int16_t neg_s16_safe(int16_t v) {
  if (v == (int16_t)-32768) return (int16_t)32767;
  return (int16_t)(-v);
}

static void set_low_latency_mode(void) {
  timeBeginPeriod(1);
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

static void restore_latency_mode(void) {
  timeEndPeriod(1);
}

static void xs_reset(XState* xs) {
  if (!xs) return;
  xs->buttons = 0;
  xs->lt = 0; xs->rt = 0;
  xs->lx = 0; xs->ly = 0;
  xs->rx = 0; xs->ry = 0;
}

static bool xs_equal(const XState* a, const XState* b) {
  return a->buttons == b->buttons &&
         a->lt == b->lt && a->rt == b->rt &&
         a->lx == b->lx && a->ly == b->ly &&
         a->rx == b->rx && a->ry == b->ry;
}

// aplica inversão Y no “raw” de forma consistente
static inline int16_t apply_y_inversion(int16_t v, bool invert) {
  return invert ? neg_s16_safe(v) : v;
}

// envia estado (já “proc”) para o controle virtual
static inline void send_state(X360Out* xout, const XState* s) {
  (void)x360_out_update_dyn(
    xout,
    s->buttons,
    s->lt, s->rt,
    s->lx, s->ly,
    s->rx, s->ry
  );
}

// Sleep inteligente: reduz “overshoot” e mantém CPU sob controle
static inline void smart_sleep_until_next_tick(uint32_t now, uint32_t last_send) {
  uint32_t elapsed = now - last_send;
  if (elapsed >= SEND_DT_MS) {
    SDL_Delay(0);
    return;
  }

  uint32_t wait = SEND_DT_MS - elapsed;

  // se falta >= 2ms, dorme quase tudo; se falta 0..1ms, não dorme (evita passar do ponto)
  if (wait >= 2) SDL_Delay(wait - 1);
  else SDL_Delay(0);
}

int main(void) {
  set_low_latency_mode();

  // Permite eventos de controle mesmo sem foco
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    restore_latency_mode();
    return 1;
  }

  SDL_Window* win = SDL_CreateWindow("DS4 -> X360 (ViGEm) - Low Latency", 640, 360, 0);
  if (!win) printf("SDL_CreateWindow failed: %s\n", SDL_GetError());

  SDL_SetGamepadEventsEnabled(true);

  // PS4 input
  PS4Config cfg = {
    .set_led_on_connect = true,
    .led_r = 30, .led_g = 144, .led_b = 255,
    .ignore_virtual_x360 = true
  };

  PS4Context* ps4 = ps4_create(&cfg);
  if (!ps4) {
    SDL_Quit();
    restore_latency_mode();
    return 1;
  }
  ps4_open_existing(ps4);

  // Virtual X360
  X360Out* xout = x360_out_create_dyn(L"ViGEmClient.dll");
  if (!xout) {
    printf("[vigem] failed to create virtual controller (check ViGEmBus + DLL)\n");
  }

  // Estados
  XState raw = {0};
  XState proc = {0};
  XState last_sent = {0};
  xs_reset(&raw);
  xs_reset(&proc);
  xs_reset(&last_sent);

  SDL_JoystickID active_iid = ps4_get_primary_instance_id(ps4);

  uint32_t last_send = SDL_GetTicks();
  bool running = true;

  // “event-driven”: quando chega evento relevante, marca dirty e tenta enviar mais cedo
  bool dirty = false;

  // minimo entre envios imediatos (pra não spammar a cada micro-evento)
  // (1ms é agressivo, mas ok com timeBeginPeriod(1) + filtro/igualdade)
  static const uint32_t MIN_IMMEDIATE_MS = 1;
  uint32_t last_immediate_try = 0;

  printf("[keys] ESC=quit | F7=toggle invert LEFT Y | F8=toggle invert RIGHT Y | F9=toggle axis debug\n");
  printf("[invert] leftY=%s rightY=%s\n", g_invert_left_y ? "ON" : "OFF", g_invert_right_y ? "ON" : "OFF");

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) running = false;

      if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_ESCAPE) running = false;

        // toggles
        if (e.key.key == SDLK_F7) {
          g_invert_left_y = !g_invert_left_y;
          printf("[invert] leftY=%s\n", g_invert_left_y ? "ON" : "OFF");
          dirty = true;
        }
        if (e.key.key == SDLK_F8) {
          g_invert_right_y = !g_invert_right_y;
          printf("[invert] rightY=%s\n", g_invert_right_y ? "ON" : "OFF");
          dirty = true;
        }
        if (e.key.key == SDLK_F9) {
          g_axis_debug = !g_axis_debug;
          printf("[debug] axis_debug=%s\n", g_axis_debug ? "ON" : "OFF");
        }
      }

      // Hotplug
      ps4_handle_event(ps4, &e);
      active_iid = ps4_get_primary_instance_id(ps4);

      if (!xout) continue;
      if (active_iid == 0) continue;

      // Apenas eventos do gamepad ativo
      if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if ((SDL_JoystickID)e.gbutton.which != active_iid) continue;

        bool down = (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        SDL_GamepadButton b = (SDL_GamepadButton)e.gbutton.button;

        uint16_t bit = ds4_btn_to_xusb(b);
        if (bit) {
          if (down) raw.buttons |= bit;
          else      raw.buttons &= (uint16_t)~bit;
          dirty = true;
        }
      }
      else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        if ((SDL_JoystickID)e.gaxis.which != active_iid) continue;

        SDL_GamepadAxis a = (SDL_GamepadAxis)e.gaxis.axis;
        int16_t v = (int16_t)e.gaxis.value;

        switch (a) {
          case SDL_GAMEPAD_AXIS_LEFTX:
            raw.lx = v;
            dirty = true;
            break;

          case SDL_GAMEPAD_AXIS_LEFTY:
            // inversão aplicada AQUI (antes de deadzone/filtro e antes de enviar)
            raw.ly = apply_y_inversion(v, g_invert_left_y);
            dirty = true;

            if (g_axis_debug && (v > 8000 || v < -8000)) {
              printf("[axis] LEFTY sdl=%d -> raw.ly=%d (invert=%s)\n", (int)v, (int)raw.ly, g_invert_left_y ? "ON" : "OFF");
            }
            break;

          case SDL_GAMEPAD_AXIS_RIGHTX:
            raw.rx = v;
            dirty = true;
            break;

          case SDL_GAMEPAD_AXIS_RIGHTY:
            // inversão aplicada AQUI
            raw.ry = apply_y_inversion(v, g_invert_right_y);
            dirty = true;

            if (g_axis_debug && (v > 8000 || v < -8000)) {
              printf("[axis] RIGHTY sdl=%d -> raw.ry=%d (invert=%s)\n", (int)v, (int)raw.ry, g_invert_right_y ? "ON" : "OFF");
            }
            break;

          case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            raw.lt = sdl_trigger_to_u8(v);
            dirty = true;
            break;

          case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            raw.rt = sdl_trigger_to_u8(v);
            dirty = true;
            break;

          default:
            break;
        }
      }
      else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        if ((SDL_JoystickID)e.gdevice.which == active_iid) {
          active_iid = 0;
          xs_reset(&raw);
          xs_reset(&proc);
          xs_reset(&last_sent);
          (void)x360_out_update_dyn(xout, 0, 0, 0, 0, 0, 0, 0);
          printf("[state] active pad removed -> reset\n");
        }
      }

      // Envio imediato (event-driven) com limite mínimo
      // - Se chegou evento (dirty) e já passou MIN_IMMEDIATE_MS desde a última tentativa, processa e tenta enviar agora.
      uint32_t now = SDL_GetTicks();
      if (dirty && (now - last_immediate_try) >= MIN_IMMEDIATE_MS) {
        last_immediate_try = now;

        // processa
        proc = raw;
        apply_deadzone_circular(raw.lx, raw.ly, STICK_DEADZONE, STICK_JITTER_CUT, &proc.lx, &proc.ly);
        apply_deadzone_circular(raw.rx, raw.ry, STICK_DEADZONE, STICK_JITTER_CUT, &proc.rx, &proc.ry);

        if (!xs_equal(&proc, &last_sent)) {
          send_state(xout, &proc);
          last_sent = proc;
          last_send = now; // atualiza “clock” do heartbeat
        }

        dirty = false; // evento consumido
      }
    }

    // Heartbeat fixo (mantém taxa constante e captura mudanças mesmo sem eventos)
    uint32_t now = SDL_GetTicks();
    if (xout && active_iid != 0 && (now - last_send) >= SEND_DT_MS) {
      last_send = now;

      proc = raw;
      apply_deadzone_circular(raw.lx, raw.ly, STICK_DEADZONE, STICK_JITTER_CUT, &proc.lx, &proc.ly);
      apply_deadzone_circular(raw.rx, raw.ry, STICK_DEADZONE, STICK_JITTER_CUT, &proc.rx, &proc.ry);

      if (!xs_equal(&proc, &last_sent)) {
        send_state(xout, &proc);
        last_sent = proc;
      }
    }

    smart_sleep_until_next_tick(now, last_send);
  }

  if (xout) {
    (void)x360_out_update_dyn(xout, 0, 0, 0, 0, 0, 0, 0);
    x360_out_destroy_dyn(xout);
  }
  ps4_destroy(ps4);

  if (win) SDL_DestroyWindow(win);
  SDL_Quit();

  restore_latency_mode();
  return 0;
}

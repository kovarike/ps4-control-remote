#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

// XInput (XUSB) button bits (Xbox 360)
#define XUSB_GAMEPAD_DPAD_UP        0x0001
#define XUSB_GAMEPAD_DPAD_DOWN      0x0002
#define XUSB_GAMEPAD_DPAD_LEFT      0x0004
#define XUSB_GAMEPAD_DPAD_RIGHT     0x0008
#define XUSB_GAMEPAD_START          0x0010
#define XUSB_GAMEPAD_BACK           0x0020
#define XUSB_GAMEPAD_LEFT_THUMB     0x0040
#define XUSB_GAMEPAD_RIGHT_THUMB    0x0080
#define XUSB_GAMEPAD_LEFT_SHOULDER  0x0100
#define XUSB_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XUSB_GAMEPAD_GUIDE          0x0400
#define XUSB_GAMEPAD_A              0x1000
#define XUSB_GAMEPAD_B              0x2000
#define XUSB_GAMEPAD_X              0x4000
#define XUSB_GAMEPAD_Y              0x8000

typedef struct XState {
  uint16_t buttons;
  uint8_t  lt, rt;      // 0..255
  int16_t  lx, ly;      // -32768..32767
  int16_t  rx, ry;
} XState;

static inline uint8_t sdl_trigger_to_u8(int16_t v) {
  // SDL trigger: normalmente 0..32767 (às vezes pode vir negativo)
  if (v <= 0) return 0;
  if (v >= 32767) return 255;
  return (uint8_t)((v * 255) / 32767);
}

static inline uint16_t ds4_btn_to_xusb(SDL_GamepadButton b) {
  switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return XUSB_GAMEPAD_A; // Cross -> A
    case SDL_GAMEPAD_BUTTON_EAST:  return XUSB_GAMEPAD_B; // Circle -> B
    case SDL_GAMEPAD_BUTTON_WEST:  return XUSB_GAMEPAD_X; // Square -> X
    case SDL_GAMEPAD_BUTTON_NORTH: return XUSB_GAMEPAD_Y; // Triangle -> Y

    case SDL_GAMEPAD_BUTTON_BACK:  return XUSB_GAMEPAD_BACK;
    case SDL_GAMEPAD_BUTTON_START: return XUSB_GAMEPAD_START;
    case SDL_GAMEPAD_BUTTON_GUIDE: return XUSB_GAMEPAD_GUIDE;

    case SDL_GAMEPAD_BUTTON_LEFT_STICK:  return XUSB_GAMEPAD_LEFT_THUMB;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return XUSB_GAMEPAD_RIGHT_THUMB;

    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return XUSB_GAMEPAD_LEFT_SHOULDER;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return XUSB_GAMEPAD_RIGHT_SHOULDER;

    case SDL_GAMEPAD_BUTTON_DPAD_UP:    return XUSB_GAMEPAD_DPAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return XUSB_GAMEPAD_DPAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return XUSB_GAMEPAD_DPAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return XUSB_GAMEPAD_DPAD_RIGHT;

    default: return 0;
  }
}

/*
  Deadzone circular + anti-jitter:
  - deadzone circular remove "drift"
  - jitter_cut corta micro mudanças (evita tremedeira e “troca de modo” em alguns jogos/overlays)
*/
static inline int16_t clamp_s16(int32_t v) {
  if (v < -32768) return -32768;
  if (v >  32767) return  32767;
  return (int16_t)v;
}

static inline void apply_deadzone_circular(int16_t inx, int16_t iny,
                                           float dz, float jitter_cut,
                                           int16_t* outx, int16_t* outy)
{
  // normaliza pra -1..1
  float x = (inx < 0) ? (float)inx / 32768.0f : (float)inx / 32767.0f;
  float y = (iny < 0) ? (float)iny / 32768.0f : (float)iny / 32767.0f;

  float mag = SDL_sqrtf(x*x + y*y);

  if (mag < dz) {
    *outx = 0; *outy = 0;
    return;
  }

  // reescala a zona útil: (mag - dz)/(1 - dz)
  float legal = (mag - dz) / (1.0f - dz);
  if (legal < 0) legal = 0;
  if (legal > 1) legal = 1;

  float scale = legal / mag;
  x *= scale;
  y *= scale;

  // anti-jitter: corta valores muito pequenos
  if (SDL_fabsf(x) < jitter_cut) x = 0;
  if (SDL_fabsf(y) < jitter_cut) y = 0;

  int32_t ox = (int32_t)(x * 32767.0f);
  int32_t oy = (int32_t)(y * 32767.0f);
  *outx = clamp_s16(ox);
  *outy = clamp_s16(oy);
}

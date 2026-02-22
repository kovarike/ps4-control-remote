#include "ps4.h"
#include <stdio.h>
#include <string.h>

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof((x)[0])))
#endif

typedef struct PadSlot {
  bool used;
  SDL_Gamepad* pad;
  SDL_JoystickID instance_id;
  char name[128];
  uint16_t vid, pid;
} PadSlot;

struct PS4Context {
  PS4Config cfg;
  PadSlot pads[4];
  SDL_JoystickID primary_iid;
};

static bool is_virtual_x360(uint16_t vid, uint16_t pid, const char* name) {
  // pelo seu log: 045e:028e
  if (vid == 0x045E && pid == 0x028E) return true;
  if (name && (strstr(name, "Xbox 360") || strstr(name, "XInput"))) return true;
  return false;
}

static int find_free_slot(PS4Context* ctx) {
  for (int i = 0; i < ARRAYSIZE(ctx->pads); i++) {
    if (!ctx->pads[i].used) return i;
  }
  return -1;
}

static int find_slot_by_instance(PS4Context* ctx, SDL_JoystickID iid) {
  for (int i = 0; i < ARRAYSIZE(ctx->pads); i++) {
    if (ctx->pads[i].used && ctx->pads[i].instance_id == iid) return i;
  }
  return -1;
}

static void try_set_led(PS4Context* ctx, SDL_Gamepad* pad) {
  if (!ctx->cfg.set_led_on_connect) return;

  SDL_PropertiesID props = SDL_GetGamepadProperties(pad);
  bool can_led =
      props &&
      (SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false) ||
       SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN, false));

  if (can_led) {
    (void)SDL_SetGamepadLED(pad, ctx->cfg.led_r, ctx->cfg.led_g, ctx->cfg.led_b);
  }
}

static void open_pad_by_instance_id(PS4Context* ctx, SDL_JoystickID instance_id) {
  if (!SDL_IsGamepad(instance_id)) return;

  if (find_slot_by_instance(ctx, instance_id) >= 0) return;

  int slot = find_free_slot(ctx);
  if (slot < 0) return;

  SDL_Gamepad* pad = SDL_OpenGamepad(instance_id);
  if (!pad) {
    printf("[hotplug] SDL_OpenGamepad(id=%d) failed: %s\n", (int)instance_id, SDL_GetError());
    return;
  }

  const char* name = SDL_GetGamepadName(pad);
  if (!name) name = "Unknown";

  uint16_t vid = SDL_GetGamepadVendor(pad);
  uint16_t pid = SDL_GetGamepadProduct(pad);

  if (ctx->cfg.ignore_virtual_x360 && is_virtual_x360(vid, pid, name)) {
    printf("[ignore] virtual x360 seen as input: %s (%04x:%04x)\n", name, vid, pid);
    SDL_CloseGamepad(pad);
    return;
  }

  ctx->pads[slot].used = true;
  ctx->pads[slot].pad = pad;
  ctx->pads[slot].instance_id = instance_id;
  ctx->pads[slot].vid = vid;
  ctx->pads[slot].pid = pid;
  snprintf(ctx->pads[slot].name, sizeof(ctx->pads[slot].name), "%s", name);

  if (ctx->primary_iid == 0) ctx->primary_iid = instance_id;

  printf("[connected] slot=%d iid=%d name=\"%s\" vid:pid=%04x:%04x\n",
         slot, (int)instance_id, name, vid, pid);

  try_set_led(ctx, pad);
}

static void close_pad_by_instance_id(PS4Context* ctx, SDL_JoystickID instance_id) {
  int slot = find_slot_by_instance(ctx, instance_id);
  if (slot < 0) return;

  printf("[disconnected] slot=%d iid=%d name=\"%s\"\n",
         slot, (int)instance_id, ctx->pads[slot].name);

  if (ctx->pads[slot].pad) SDL_CloseGamepad(ctx->pads[slot].pad);
  memset(&ctx->pads[slot], 0, sizeof(ctx->pads[slot]));

  if (ctx->primary_iid == instance_id) ctx->primary_iid = 0;
}

PS4Context* ps4_create(const PS4Config* cfg) {
  PS4Context* ctx = (PS4Context*)SDL_calloc(1, sizeof(PS4Context));
  if (!ctx) return NULL;

  // defaults
  ctx->cfg.set_led_on_connect = true;
  ctx->cfg.led_r = 30; ctx->cfg.led_g = 144; ctx->cfg.led_b = 255;
  ctx->cfg.ignore_virtual_x360 = true;

  if (cfg) ctx->cfg = *cfg;

  ctx->primary_iid = 0;
  return ctx;
}

void ps4_destroy(PS4Context* ctx) {
  if (!ctx) return;

  for (int i = 0; i < ARRAYSIZE(ctx->pads); i++) {
    if (ctx->pads[i].used && ctx->pads[i].pad) SDL_CloseGamepad(ctx->pads[i].pad);
  }
  SDL_free(ctx);
}

void ps4_open_existing(PS4Context* ctx) {
  if (!ctx) return;

  int count = 0;
  SDL_JoystickID* ids = SDL_GetJoysticks(&count);
  printf("[init] joysticks detected: %d\n", count);

  if (!ids) {
    printf("SDL_GetJoysticks failed: %s\n", SDL_GetError());
    return;
  }

  for (int i = 0; i < count; i++) {
    SDL_JoystickID id = ids[i];
    if (SDL_IsGamepad(id)) open_pad_by_instance_id(ctx, id);
  }
  SDL_free(ids);
}

bool ps4_handle_event(PS4Context* ctx, const SDL_Event* e) {
  if (!ctx || !e) return false;

  switch (e->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
      open_pad_by_instance_id(ctx, (SDL_JoystickID)e->gdevice.which);
      return true;

    case SDL_EVENT_GAMEPAD_REMOVED:
      close_pad_by_instance_id(ctx, (SDL_JoystickID)e->gdevice.which);
      return true;

    default:
      return false;
  }
}

SDL_JoystickID ps4_get_primary_instance_id(const PS4Context* ctx) {
  return ctx ? ctx->primary_iid : 0;
}

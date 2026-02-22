#include "output_vigem_x360_dyn.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void* PVIGEM_CLIENT;
typedef void* PVIGEM_TARGET;

typedef uint32_t VIGEM_ERROR;
#define VIGEM_ERROR_NONE 0x20000000u
#define VIGEM_SUCCESS(err) ((err) == VIGEM_ERROR_NONE)

#pragma pack(push, 1)
typedef struct XUSB_REPORT {
  uint16_t wButtons;
  uint8_t  bLeftTrigger;
  uint8_t  bRightTrigger;
  int16_t  sThumbLX;
  int16_t  sThumbLY;
  int16_t  sThumbRX;
  int16_t  sThumbRY;
} XUSB_REPORT;
#pragma pack(pop)

typedef PVIGEM_CLIENT (__stdcall *PFN_vigem_alloc)(void);
typedef void         (__stdcall *PFN_vigem_free)(PVIGEM_CLIENT);
typedef VIGEM_ERROR  (__stdcall *PFN_vigem_connect)(PVIGEM_CLIENT);
typedef void         (__stdcall *PFN_vigem_disconnect)(PVIGEM_CLIENT);

typedef PVIGEM_TARGET(__stdcall *PFN_vigem_target_x360_alloc)(void);
typedef void         (__stdcall *PFN_vigem_target_free)(PVIGEM_TARGET);
typedef VIGEM_ERROR  (__stdcall *PFN_vigem_target_add)(PVIGEM_CLIENT, PVIGEM_TARGET);
typedef VIGEM_ERROR  (__stdcall *PFN_vigem_target_remove)(PVIGEM_CLIENT, PVIGEM_TARGET);
typedef VIGEM_ERROR  (__stdcall *PFN_vigem_target_x360_update)(PVIGEM_CLIENT, PVIGEM_TARGET, XUSB_REPORT);

struct X360Out {
  HMODULE dll;

  PFN_vigem_alloc vigem_alloc;
  PFN_vigem_free vigem_free;
  PFN_vigem_connect vigem_connect;
  PFN_vigem_disconnect vigem_disconnect;

  PFN_vigem_target_x360_alloc vigem_target_x360_alloc;
  PFN_vigem_target_free vigem_target_free;
  PFN_vigem_target_add vigem_target_add;
  PFN_vigem_target_remove vigem_target_remove;
  PFN_vigem_target_x360_update vigem_target_x360_update;

  PVIGEM_CLIENT client;
  PVIGEM_TARGET target;
  bool added;
};

static FARPROC must_get(HMODULE dll, const char* name) {
  FARPROC p = GetProcAddress(dll, name);
  if (!p) printf("[vigem] missing export: %s\n", name);
  return p;
}

static bool load_api(X360Out* o) {
  o->vigem_alloc = (PFN_vigem_alloc)must_get(o->dll, "vigem_alloc");
  o->vigem_free = (PFN_vigem_free)must_get(o->dll, "vigem_free");
  o->vigem_connect = (PFN_vigem_connect)must_get(o->dll, "vigem_connect");
  o->vigem_disconnect = (PFN_vigem_disconnect)must_get(o->dll, "vigem_disconnect");

  o->vigem_target_x360_alloc = (PFN_vigem_target_x360_alloc)must_get(o->dll, "vigem_target_x360_alloc");
  o->vigem_target_free = (PFN_vigem_target_free)must_get(o->dll, "vigem_target_free");
  o->vigem_target_add = (PFN_vigem_target_add)must_get(o->dll, "vigem_target_add");
  o->vigem_target_remove = (PFN_vigem_target_remove)must_get(o->dll, "vigem_target_remove");
  o->vigem_target_x360_update = (PFN_vigem_target_x360_update)must_get(o->dll, "vigem_target_x360_update");

  return o->vigem_alloc && o->vigem_free && o->vigem_connect && o->vigem_disconnect &&
         o->vigem_target_x360_alloc && o->vigem_target_free && o->vigem_target_add &&
         o->vigem_target_remove && o->vigem_target_x360_update;
}

X360Out* x360_out_create_dyn(const wchar_t* dll_path_optional) {
  X360Out* o = (X360Out*)calloc(1, sizeof(X360Out));
  if (!o) return NULL;

  o->dll = dll_path_optional ? LoadLibraryW(dll_path_optional) : LoadLibraryW(L"ViGEmClient.dll");
  if (!o->dll) {
    printf("[vigem] LoadLibrary(ViGEmClient.dll) failed: %lu\n", GetLastError());
    free(o);
    return NULL;
  }

  if (!load_api(o)) {
    printf("[vigem] failed to load API exports\n");
    FreeLibrary(o->dll);
    free(o);
    return NULL;
  }

  o->client = o->vigem_alloc();
  if (!o->client) {
    printf("[vigem] vigem_alloc failed\n");
    FreeLibrary(o->dll);
    free(o);
    return NULL;
  }

  VIGEM_ERROR rc = o->vigem_connect(o->client);
  if (!VIGEM_SUCCESS(rc)) {
    printf("[vigem] vigem_connect failed: 0x%08X (install ViGEmBus)\n", (unsigned)rc);
    o->vigem_free(o->client);
    FreeLibrary(o->dll);
    free(o);
    return NULL;
  }

  o->target = o->vigem_target_x360_alloc();
  if (!o->target) {
    printf("[vigem] vigem_target_x360_alloc failed\n");
    o->vigem_disconnect(o->client);
    o->vigem_free(o->client);
    FreeLibrary(o->dll);
    free(o);
    return NULL;
  }

  VIGEM_ERROR ra = o->vigem_target_add(o->client, o->target);
  if (!VIGEM_SUCCESS(ra)) {
    printf("[vigem] vigem_target_add failed: 0x%08X\n", (unsigned)ra);
    o->vigem_target_free(o->target);
    o->vigem_disconnect(o->client);
    o->vigem_free(o->client);
    FreeLibrary(o->dll);
    free(o);
    return NULL;
  }

  o->added = true;
  printf("[vigem] Xbox 360 virtual controller created\n");
  return o;
}

void x360_out_destroy_dyn(X360Out* o) {
  if (!o) return;

  if (o->client && o->target && o->added) {
    o->vigem_target_remove(o->client, o->target);
  }
  if (o->target) o->vigem_target_free(o->target);
  if (o->client) {
    o->vigem_disconnect(o->client);
    o->vigem_free(o->client);
  }
  if (o->dll) FreeLibrary(o->dll);
  free(o);
}

bool x360_out_update_dyn(
  X360Out* o,
  uint16_t buttons,
  uint8_t lt,
  uint8_t rt,
  int16_t lx, int16_t ly,
  int16_t rx, int16_t ry
) {
  if (!o || !o->client || !o->target || !o->added) return false;

  XUSB_REPORT r;
  memset(&r, 0, sizeof(r));
  r.wButtons = buttons;
  r.bLeftTrigger = lt;
  r.bRightTrigger = rt;
  r.sThumbLX = lx;
  r.sThumbLY = ly;
  r.sThumbRX = rx;
  r.sThumbRY = ry;

  VIGEM_ERROR ru = o->vigem_target_x360_update(o->client, o->target, r);
  return VIGEM_SUCCESS(ru);
}

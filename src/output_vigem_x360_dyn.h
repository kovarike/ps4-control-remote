#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef struct X360Out X360Out;

X360Out* x360_out_create_dyn(const wchar_t* dll_path_optional);
void x360_out_destroy_dyn(X360Out* o);

bool x360_out_update_dyn(
  X360Out* o,
  uint16_t buttons,
  uint8_t lt,
  uint8_t rt,
  int16_t lx, int16_t ly,
  int16_t rx, int16_t ry
);

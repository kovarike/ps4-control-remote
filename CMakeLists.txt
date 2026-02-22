cmake_minimum_required(VERSION 3.20)
project(ps4ControlrRemote C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ps4ControlrRemote
  src/main.c
  src/ps4.c
  src/focus_win32.c
  src/output_vigem_x360_dyn.c

)

# ---- SDL3 (prebuilt mingw64) ----
set(SDL_ROOT "${CMAKE_SOURCE_DIR}/third_party/SDL3-mingw64")

target_include_directories(ps4ControlrRemote PRIVATE
  "${SDL_ROOT}/include"
)

target_link_directories(ps4ControlrRemote PRIVATE
  "${SDL_ROOT}/lib"
)

# SDL3 normalmente exporta "SDL3"
target_link_libraries(ps4ControlrRemote PRIVATE SDL3)

# ---- Win32 libs ----
if (WIN32)
  target_link_libraries(ps4ControlrRemote PRIVATE
    user32
    gdi32
    winmm
    imm32
    ole32
    oleaut32
    version
    uuid
    setupapi
  )
endif()

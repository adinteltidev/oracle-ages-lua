# The official devkitPro Switch toolchain supplies __SWITCH__, startup and link flags.
if(NOT DEFINED ENV{DEVKITPRO})
  message(FATAL_ERROR "Set DEVKITPRO to the devkitPro SDK root")
endif()
set(SWITCH_SDK "$ENV{DEVKITPRO}")
set(SWITCH_PORTLIBS "${SWITCH_SDK}/portlibs/switch")
set(ORACLES_SWITCH_BUILD "local" CACHE STRING "Build identifier")
# Compile the existing desktop widgets against a scoped SDL2 adapter.
include(${CMAKE_SOURCE_DIR}/platform/switch/menu.cmake)
add_executable(oracles-switch src/platform/switch_main.c src/platform/switch_storage.c
  src/platform/switch_menu_state.c src/platform/switch_menu_boot.c src/platform/switch_menu.c src/ui/savefile.c)
target_link_libraries(oracles-switch PRIVATE oraclesswitchgui)
target_include_directories(oracles-switch PRIVATE
  "${SWITCH_PORTLIBS}/include" "${SWITCH_PORTLIBS}/include/SDL2")
target_compile_definitions(oracles-switch PRIVATE
  ORACLES_NATIVE=1 ORACLES_SWITCH_BUILD="${ORACLES_SWITCH_BUILD}")
target_link_directories(oracles-switch PRIVATE
  "${SWITCH_PORTLIBS}/lib" "${SWITCH_SDK}/libnx/lib")
target_link_libraries(oracles-switch PRIVATE oraclesnative SDL2 EGL glapi drm_nouveau stdc++ nx pthread m)
set_target_properties(oracles-switch PROPERTIES SUFFIX ".elf")
find_program(ORACLES_ELF2NRO elf2nro HINTS "$ENV{ORACLES_SWITCH_TOOLS}" REQUIRED NO_CMAKE_FIND_ROOT_PATH)
find_program(ORACLES_NACPTOOL nacptool HINTS "$ENV{ORACLES_SWITCH_TOOLS}" REQUIRED NO_CMAKE_FIND_ROOT_PATH)
set(SWITCH_NACP_VERSION "${ORACLES_SWITCH_VERSION}")
if(NOT SWITCH_NACP_VERSION)
  set(SWITCH_NACP_VERSION "0.1.0")
endif()
set(ORACLES_SWITCH_TITLE "Oracle of Ages Native")
if(ORACLES_LUA_GAMEPLAY)
  set(ORACLES_SWITCH_TITLE "Oracle of Ages - Lua pilot")
endif()
if(ORACLES_LUA_COLORED_CUBE)
  set(ORACLES_SWITCH_TITLE "Oracle of Ages - Lua phase 2")
elseif(ORACLES_LUA_BLOCK_PUZZLES)
  set(ORACLES_SWITCH_TITLE "Oracle of Ages - Lua phase 1")
endif()
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/oracles-switch.nro"
  COMMAND "${ORACLES_NACPTOOL}" --create "${ORACLES_SWITCH_TITLE}" "Native port contributors" "${SWITCH_NACP_VERSION}" "${CMAKE_BINARY_DIR}/oracles-switch.nacp"
  COMMAND "${ORACLES_ELF2NRO}" "$<TARGET_FILE:oracles-switch>" "${CMAKE_BINARY_DIR}/oracles-switch.nro"
    --nacp=${CMAKE_BINARY_DIR}/oracles-switch.nacp
    --icon=${CMAKE_SOURCE_DIR}/platform/switch/icon.jpg
  DEPENDS oracles-switch "${CMAKE_SOURCE_DIR}/platform/switch/icon.jpg" VERBATIM)
add_custom_target(switch-nro ALL DEPENDS "${CMAKE_BINARY_DIR}/oracles-switch.nro")

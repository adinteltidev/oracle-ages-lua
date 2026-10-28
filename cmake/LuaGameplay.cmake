option(ORACLES_LUA_GAMEPLAY "Experimental Ages gameplay decisions in Lua" OFF)
file(GLOB ORACLES_LUA_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/third_party/lua/*.c")
list(FILTER ORACLES_LUA_SRC EXCLUDE REGEX "/lua(c)?[.]c$")
add_library(oracleslua STATIC ${ORACLES_LUA_SRC})
target_include_directories(oracleslua PUBLIC third_party/lua)
if(NOT WIN32 AND NOT ORACLES_SWITCH AND NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
  target_compile_definitions(oracleslua PRIVATE LUA_USE_POSIX)
  target_link_libraries(oracleslua PUBLIC m)
endif()
if(ORACLES_LUA_GAMEPLAY)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ages_switch_lua.c
    COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/gameplay/ages/switch_tiles.lua
      -DOUT=${CMAKE_BINARY_DIR}/ages_switch_lua.c -DNAME=ages_switch_lua
      -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS gameplay/ages/switch_tiles.lua cmake/embed.cmake VERBATIM)
  add_library(oraclesgameplay STATIC src/script/gameplay.c ${CMAKE_BINARY_DIR}/ages_switch_lua.c)
  target_include_directories(oraclesgameplay PUBLIC src)
  target_link_libraries(oraclesgameplay PUBLIC oracleslua)
endif()

# Use the release override when supplied, otherwise the project's VERSION file.
set(ORACLES_SWITCH_VERSION "${ORACLES_VERSION_NUMBER}")
if(NOT ORACLES_SWITCH_VERSION)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
  file(READ "${CMAKE_SOURCE_DIR}/VERSION" ORACLES_SWITCH_VERSION)
  string(STRIP "${ORACLES_SWITCH_VERSION}" ORACLES_SWITCH_VERSION)
endif()
set(ORACLES_FONT ${CMAKE_SOURCE_DIR}/third_party/fonts/VT323-Regular.ttf)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/gui_vt323.c
  COMMAND ${CMAKE_COMMAND} -DIN=${ORACLES_FONT} -DOUT=${CMAKE_BINARY_DIR}/gui_vt323.c -DNAME=gui_vt323 -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${ORACLES_FONT} ${CMAKE_SOURCE_DIR}/cmake/embed.cmake VERBATIM)
add_library(oraclesswitchgui STATIC src/gui/gui.c src/gui/widgets.c src/gui/slots.c
  src/gui/vendor.c src/platform/switch_menu_gui.c ${CMAKE_BINARY_DIR}/gui_vt323.c)
target_include_directories(oraclesswitchgui PUBLIC src PRIVATE
  platform/switch/compat third_party/clay third_party/stb
  "${SWITCH_PORTLIBS}/include/SDL2")
set_source_files_properties(src/gui/vendor.c PROPERTIES COMPILE_OPTIONS "-w")
target_compile_definitions(oraclesswitchgui PRIVATE ORACLES_SWITCH_VERSION="${ORACLES_SWITCH_VERSION}")

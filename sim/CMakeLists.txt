cmake_minimum_required(VERSION 3.16)
project(panadapter_sdl C)
if(APPLE)
    enable_language(OBJC) # desktop/native_menu_macos.m (menus + Settings dialog)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(MAIN ${REPO_ROOT}/main)
set(FT8_LIB_DIR ${REPO_ROOT}/components/ft8_lib)

# --- LVGL, built standalone (env_support/cmake/custom.cmake) with our own
# desktop lv_conf.h (LV_USE_SDL=1) - see that file's header comment.
# Prefer the same tree the firmware builds against (managed_components/,
# resolved by the IDF component manager); fall back to fetching the pinned
# 9.2.x release from GitHub so this port builds on a machine (or CI runner)
# that has never run idf.py. ---
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
if(EXISTS ${REPO_ROOT}/managed_components/lvgl__lvgl/CMakeLists.txt)
    set(LVGL_DIR ${REPO_ROOT}/managed_components/lvgl__lvgl)
    add_subdirectory(${LVGL_DIR} lvgl_build)
else()
    include(FetchContent)
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG        v9.2.2   # matches main/idf_component.yml's lvgl/lvgl "~9.2.0"
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(lvgl)
    set(LVGL_DIR ${lvgl_SOURCE_DIR})
endif()
target_include_directories(lvgl SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_directories(lvgl PUBLIC ${SDL2_LIBRARY_DIRS})
target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})

# --- The REAL main/ui/*.c (minus reader_view.c/qmx_term_view.c - see
# stubs/reader_view_stub.c and stubs/qmx_term_view_stub.c), plus the render
# and util files portable/small enough to compile unmodified. ---
file(GLOB UI_REAL_SOURCES ${MAIN}/ui/*.c)
list(FILTER UI_REAL_SOURCES EXCLUDE REGEX ".*/reader_view\\.c$")
list(FILTER UI_REAL_SOURCES EXCLUDE REGEX ".*/qmx_term_view\\.c$")

set(REAL_SOURCES
    ${UI_REAL_SOURCES}
    ${MAIN}/util/db_gridlines.c
    ${MAIN}/util/bandplan.c
    ${MAIN}/util/dxcc.c
    ${MAIN}/util/maidenhead.c
    ${MAIN}/util/status.c
    ${MAIN}/render/render.c
    ${MAIN}/render/render_waterfall.c
)

# --- The SDL-port module implementations. These live IN main/, beside the
# hardware implementations they mirror, as *_sdl.c siblings - so a header
# change and its port implementation travel in the same directory and the
# same review. The ESP-IDF build never sees them (main/CMakeLists.txt lists
# its SRCS explicitly). See docs/porting.md. ---
set(PORT_SOURCES
    desktop/main.c                # entry point (the port's app_main stand-in)
    desktop/ring_buffer.c         # helpers used by audio_sdl.c
    desktop/wav_loader.c
    desktop/device_list.c         # audio/serial device enumeration (Settings dialog)
    desktop/mock_rig.c            # in-process QMX CAT emulation (fake peripheral)
    desktop/cat_log.c             # CAT traffic ring behind the CAT Log window
    desktop/ft8_rx_sdl.c          # slot-aligned FT8 decode via the real ft8_lib
    ${MAIN}/display/display_sdl.c
    ${MAIN}/storage/settings_sdl.c
    ${MAIN}/audio/audio_sdl.c
    ${MAIN}/cat/cat_sdl.c
    ${MAIN}/dsp/dsp_sdl.c
    ${MAIN}/ft8_hash.c            # REAL hash table - nonstandard calls decode as text, not <...>
    ${FT8_LIB_DIR}/fft/kiss_fft.c
    ${FT8_LIB_DIR}/fft/kiss_fftr.c
    ${FT8_LIB_DIR}/common/monitor.c
    ${FT8_LIB_DIR}/ft8/decode.c
    ${FT8_LIB_DIR}/ft8/ldpc.c
    ${FT8_LIB_DIR}/ft8/crc.c
    ${FT8_LIB_DIR}/ft8/constants.c
    ${FT8_LIB_DIR}/ft8/message.c
    ${FT8_LIB_DIR}/ft8/text.c
)
if(APPLE)
    list(APPEND PORT_SOURCES desktop/native_menu_macos.m)
    set_source_files_properties(desktop/native_menu_macos.m PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
else()
    list(APPEND PORT_SOURCES desktop/native_menu_stub.c)
endif()

file(GLOB STUB_SOURCES stubs/*.c)

add_executable(panadapter_sdl
    ${REAL_SOURCES}
    ${PORT_SOURCES}
    ${STUB_SOURCES}
)

target_include_directories(panadapter_sdl PRIVATE
    esp_shim
    esp_shim/freertos
    desktop                              # ring_buffer.h, wav_loader.h, device_list.h
    ${LVGL_DIR}/src/drivers/sdl
    ${FT8_LIB_DIR}/fft                   # kiss_fft.h
    ${FT8_LIB_DIR}                       # ft8/constants.h, ft8/message.h (types only - see stubs/ft8_engine_stubs.c)
    # Mirrors main/CMakeLists.txt's flattened INCLUDE_DIRS exactly, so every
    # quote-include in the real ui/*.c files resolves against the REAL
    # headers with zero path rewriting.
    ${MAIN}
    ${MAIN}/display
    ${MAIN}/ui
    ${MAIN}/util
    ${MAIN}/cat
    ${MAIN}/audio
    ${MAIN}/dsp
    ${MAIN}/render
    ${MAIN}/screenshot
    ${MAIN}/storage
    ${MAIN}/wifi
    ${MAIN}/net
    ${MAIN}/rtc
    ${MAIN}/time_sync
    ${MAIN}/adif
    ${MAIN}/keyboard
    ${SDL2_INCLUDE_DIRS}
)
target_link_directories(panadapter_sdl PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(panadapter_sdl PRIVATE lvgl ${SDL2_LIBRARIES} m)
if(APPLE)
    target_link_libraries(panadapter_sdl PRIVATE "-framework Cocoa")
endif()
target_compile_options(panadapter_sdl PRIVATE -Wno-unused-parameter)

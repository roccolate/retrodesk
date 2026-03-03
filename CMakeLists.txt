cmake_minimum_required(VERSION 3.5)
project(RetroDesk C)

set(CMAKE_C_STANDARD 11)

# Emit a compilation database so external tools (clang-tidy, language servers)
# can analyze the project.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable stricter compiler warnings by default and option to treat them as errors.
option(ENABLE_STRICT_WARNINGS "Enable additional compiler warnings (-Wall -Wextra -Wpedantic)" ON)
option(ENABLE_WERROR "Treat warnings as errors (adds -Werror or /WX)" ON)

option(PDCURSES_ROOT "Path to PDCurses root directory" "")

# Detect PDCurses: either a user-provided PDCURSES_ROOT or system Curses package
if (PDCURSES_ROOT)
    message(STATUS "Using PDCurses from ${PDCURSES_ROOT}")
    set(PDCURSES_INCLUDE_DIR ${PDCURSES_ROOT}/include)
    if (EXISTS ${PDCURSES_ROOT}/lib/libpdcurses.a)
        set(PDCURSES_LIB ${PDCURSES_ROOT}/lib/libpdcurses.a)
    elseif (EXISTS ${PDCURSES_ROOT}/lib/libpdcurses.so)
        set(PDCURSES_LIB ${PDCURSES_ROOT}/lib/libpdcurses.so)
    elseif (EXISTS ${PDCURSES_ROOT}/lib/pdcurses.lib)
        set(PDCURSES_LIB ${PDCURSES_ROOT}/lib/pdcurses.lib)
    endif()
else()
    find_package(Curses QUIET)
    if (Curses_FOUND)
        set(PDCURSES_INCLUDE_DIR ${CURSES_INCLUDE_DIR})
        set(PDCURSES_LIB ${CURSES_LIBRARIES})
    endif()
endif()

# Platform source (single unified implementation)
set(PLATFORM_SRC src/platform/platform.c)

# UI sources
set(UI_SRCS
    src/ui/dialog.c
    src/ui/menu.c
    src/ui/mouse.c
    src/ui/statusbar.c
    src/ui/themes.c
    src/ui/window.c
)

add_executable(retrodesk src/main.c src/desktop.c ${UI_SRCS} ${PLATFORM_SRC})

# Ensure project headers in src/ are available to targets (so modules can include "desktop.h").
target_include_directories(retrodesk PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Configure warning flags per-compiler when enabled
if (ENABLE_STRICT_WARNINGS)
    target_compile_options(retrodesk PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
    )
endif()

if (ENABLE_WERROR)
    target_compile_options(retrodesk PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/WX>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>
    )
endif()

# If vcpkg installed PDCurses in-tree, prefer those includes/libs
if (EXISTS ${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/include/curses.h)
    target_include_directories(retrodesk PRIVATE ${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/include)
    if (EXISTS ${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/pdcurses.lib)
        set(PDCURSES_LIB ${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/pdcurses.lib)
    endif()
endif()

# If we detected PDCurses include dir earlier, add it to the target
if (PDCURSES_INCLUDE_DIR)
    target_include_directories(retrodesk PRIVATE ${PDCURSES_INCLUDE_DIR})
endif()

if (PDCURSES_LIB)
    target_compile_definitions(retrodesk PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
    target_link_libraries(retrodesk PRIVATE ${PDCURSES_LIB})
endif()

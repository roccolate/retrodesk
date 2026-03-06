cmake_minimum_required(VERSION 3.16)
project(RetroDesk C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_WERROR "Treat warnings as errors" ON)
option(ENABLE_TESTS "Build lightweight unit tests" ON)
set(PDCURSES_ROOT "" CACHE PATH "Path to PDCurses root")

set(RETRODESK_SOURCES
    src/main.c
    src/core/desktop.c
    src/platform/platform.c
    src/render/render.c
    src/wm/window_manager.c
    src/app/app_runtime.c
    src/ui/statusbar.c
    src/apps/apps.c
    src/apps/filemanager_app.c
    src/apps/notepad_app.c
    src/apps/terminal_app.c
)

add_executable(retrodesk ${RETRODESK_SOURCES})
target_include_directories(retrodesk PRIVATE ${CMAKE_SOURCE_DIR}/src)

if (ENABLE_STRICT_WARNINGS)
    target_compile_options(retrodesk PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
    )
endif()

if (ENABLE_TESTS)
    include(CTest)
    if (BUILD_TESTING)
        add_executable(app_registry_test
            tests/app_registry_test.c
            src/app/app_runtime.c
        )
        target_include_directories(app_registry_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME app_registry_test COMMAND app_registry_test)

        add_executable(platform_features_test tests/platform_features_test.c)
        target_include_directories(platform_features_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME platform_features_test COMMAND platform_features_test)
    endif()
endif()

if (ENABLE_WERROR)
    target_compile_options(retrodesk PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/WX>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>
    )
endif()

if (PDCURSES_ROOT)
    set(_curses_include "${PDCURSES_ROOT}/include")
    find_library(_curses_lib
        NAMES pdcurses pdcursesd
        PATHS "${PDCURSES_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    if (NOT _curses_lib)
        message(FATAL_ERROR "PDCurses library not found under ${PDCURSES_ROOT}/lib")
    endif()
    target_include_directories(retrodesk PRIVATE "${_curses_include}")
    target_link_libraries(retrodesk PRIVATE "${_curses_lib}")
    target_compile_definitions(retrodesk PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
else()
    find_package(Curses REQUIRED)
    if (CURSES_INCLUDE_DIRS)
        target_include_directories(retrodesk PRIVATE ${CURSES_INCLUDE_DIRS})
    elseif (CURSES_INCLUDE_DIR)
        target_include_directories(retrodesk PRIVATE ${CURSES_INCLUDE_DIR})
    endif()

    if (TARGET Curses::Curses)
        target_link_libraries(retrodesk PRIVATE Curses::Curses)
    else()
        target_link_libraries(retrodesk PRIVATE ${CURSES_LIBRARIES})
    endif()
endif()

cmake_minimum_required(VERSION 3.16)
project(RetroDesk C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_WERROR "Treat warnings as errors" ON)
option(ENABLE_TESTS "Build lightweight unit tests" ON)
set(PDCURSES_ROOT "" CACHE PATH "Path to PDCurses root")
set(RETROCORE_SPEC_DIR "${CMAKE_SOURCE_DIR}/../retrocore-spec" CACHE PATH
    "Path to a retrocore-spec checkout for shared contract fixture tests")

set(RETRODESK_SOURCES
    src/main.c
    src/core/cli.c
    src/core/desktop.c
    src/platform/platform.c
    src/platform/platform_curses.c
    src/platform/platform_tty_raw.c
    src/platform/tty_decoder.c
    src/render/render.c
    src/render/ansi_renderer.c
    src/wm/window_manager.c
    src/app/app_runtime.c
    src/ui/statusbar.c
    src/ui/text_input.c
    src/ui/scroll_list.c
    src/ui/text_buffer.c
    src/ui/button.c
    src/ui/dialog.c
    src/ui/menu_bar.c
    src/ui/theme.c
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
        add_executable(cli_parse_test
            tests/cli_parse_test.c
            src/core/cli.c
            src/ui/theme.c
        )
        target_include_directories(cli_parse_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME cli_parse_test COMMAND cli_parse_test)

        add_executable(app_registry_test
            tests/app_registry_test.c
            src/app/app_runtime.c
        )
        target_include_directories(app_registry_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME app_registry_test COMMAND app_registry_test)

    add_executable(platform_features_test tests/platform_features_test.c)
    target_include_directories(platform_features_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME platform_features_test COMMAND platform_features_test)

    add_executable(wm_event_replay_test
        tests/wm_event_replay_test.c
        tests/wm_render_stub.c
        src/wm/window_manager.c
        src/ui/theme.c
    )
    target_include_directories(wm_event_replay_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME wm_event_replay_test COMMAND wm_event_replay_test)

    add_executable(theme_catalog_test
        tests/theme_catalog_test.c
        src/ui/theme.c
    )
    target_include_directories(theme_catalog_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME theme_catalog_test COMMAND theme_catalog_test)
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

if (ENABLE_TESTS AND BUILD_TESTING)
    add_executable(draw_list_contract_test
        tests/draw_list_contract_test.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(draw_list_contract_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME draw_list_contract_test COMMAND draw_list_contract_test)

    add_executable(statusbar_drawlist_test
        tests/statusbar_drawlist_test.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/ui/statusbar.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(statusbar_drawlist_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME statusbar_drawlist_test COMMAND statusbar_drawlist_test)

    add_executable(ansi_renderer_diff_test
        tests/ansi_renderer_diff_test.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(ansi_renderer_diff_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ansi_renderer_diff_test COMMAND ansi_renderer_diff_test)

    add_executable(tty_decoder_test
        tests/tty_decoder_test.c
        src/platform/tty_decoder.c
    )
    target_include_directories(tty_decoder_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME tty_decoder_test COMMAND tty_decoder_test)

    add_executable(text_input_test
        tests/text_input_test.c
        src/ui/text_input.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(text_input_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME text_input_test COMMAND text_input_test)

    add_executable(scroll_list_test
        tests/scroll_list_test.c
        src/ui/scroll_list.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(scroll_list_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME scroll_list_test COMMAND scroll_list_test)

    add_executable(text_buffer_test
        tests/text_buffer_test.c
        src/ui/text_buffer.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(text_buffer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME text_buffer_test COMMAND text_buffer_test)

    add_executable(button_test
        tests/button_test.c
        src/ui/button.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(button_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME button_test COMMAND button_test)

    add_executable(dialog_test
        tests/dialog_test.c
        src/ui/dialog.c
        src/ui/button.c
        src/ui/text_input.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(dialog_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME dialog_test COMMAND dialog_test)

    add_executable(menu_bar_test
        tests/menu_bar_test.c
        src/ui/menu_bar.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(menu_bar_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME menu_bar_test COMMAND menu_bar_test)

    add_executable(desktop_runtime_test
        tests/desktop_runtime_test.c
        tests/wm_render_stub.c
        src/core/desktop.c
        src/app/app_runtime.c
        src/apps/apps.c
        src/apps/filemanager_app.c
        src/apps/notepad_app.c
        src/apps/terminal_app.c
        src/wm/window_manager.c
        src/ui/statusbar.c
        src/ui/theme.c
    )
    target_include_directories(desktop_runtime_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME desktop_runtime_test COMMAND desktop_runtime_test)

    if (EXISTS "${RETROCORE_SPEC_DIR}/fixtures/events/open-files-and-focus.json")
        add_executable(retrocore_event_fixture_test
            tests/retrocore_event_fixture_test.c
            tests/wm_render_stub.c
            src/core/desktop.c
            src/app/app_runtime.c
            src/apps/apps.c
            src/apps/filemanager_app.c
            src/apps/notepad_app.c
            src/apps/terminal_app.c
            src/wm/window_manager.c
            src/ui/statusbar.c
            src/ui/theme.c
        )
        target_include_directories(retrocore_event_fixture_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        target_compile_definitions(retrocore_event_fixture_test PRIVATE
            RETROCORE_SPEC_DIR="${RETROCORE_SPEC_DIR}"
        )
        add_test(NAME retrocore_event_fixture_test COMMAND retrocore_event_fixture_test)
    else()
        message(STATUS "retrocore-spec fixtures not found at ${RETROCORE_SPEC_DIR}; skipping retrocore_event_fixture_test")
    endif()

    if (PDCURSES_ROOT)
        target_include_directories(draw_list_contract_test PRIVATE "${_curses_include}")
        target_include_directories(statusbar_drawlist_test PRIVATE "${_curses_include}")
        target_include_directories(ansi_renderer_diff_test PRIVATE "${_curses_include}")
        target_include_directories(text_input_test PRIVATE "${_curses_include}")
        target_include_directories(scroll_list_test PRIVATE "${_curses_include}")
        target_include_directories(text_buffer_test PRIVATE "${_curses_include}")
        target_include_directories(button_test PRIVATE "${_curses_include}")
        target_include_directories(dialog_test PRIVATE "${_curses_include}")
        target_include_directories(menu_bar_test PRIVATE "${_curses_include}")
        target_link_libraries(draw_list_contract_test PRIVATE "${_curses_lib}")
        target_link_libraries(statusbar_drawlist_test PRIVATE "${_curses_lib}")
        target_link_libraries(ansi_renderer_diff_test PRIVATE "${_curses_lib}")
        target_link_libraries(text_input_test PRIVATE "${_curses_lib}")
        target_link_libraries(scroll_list_test PRIVATE "${_curses_lib}")
        target_link_libraries(text_buffer_test PRIVATE "${_curses_lib}")
        target_link_libraries(button_test PRIVATE "${_curses_lib}")
        target_link_libraries(dialog_test PRIVATE "${_curses_lib}")
        target_link_libraries(menu_bar_test PRIVATE "${_curses_lib}")
        target_compile_definitions(draw_list_contract_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(statusbar_drawlist_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(ansi_renderer_diff_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(text_input_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(scroll_list_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(text_buffer_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(button_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(dialog_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        target_compile_definitions(menu_bar_test PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
    else()
        if (CURSES_INCLUDE_DIRS)
            target_include_directories(draw_list_contract_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(statusbar_drawlist_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(ansi_renderer_diff_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(text_input_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(scroll_list_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(text_buffer_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(button_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(dialog_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(menu_bar_test PRIVATE ${CURSES_INCLUDE_DIRS})
        elseif (CURSES_INCLUDE_DIR)
            target_include_directories(draw_list_contract_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(statusbar_drawlist_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(ansi_renderer_diff_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(text_input_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(scroll_list_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(text_buffer_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(button_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(dialog_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(menu_bar_test PRIVATE ${CURSES_INCLUDE_DIR})
        endif()

        if (TARGET Curses::Curses)
            target_link_libraries(draw_list_contract_test PRIVATE Curses::Curses)
            target_link_libraries(statusbar_drawlist_test PRIVATE Curses::Curses)
            target_link_libraries(ansi_renderer_diff_test PRIVATE Curses::Curses)
            target_link_libraries(text_input_test PRIVATE Curses::Curses)
            target_link_libraries(scroll_list_test PRIVATE Curses::Curses)
            target_link_libraries(text_buffer_test PRIVATE Curses::Curses)
            target_link_libraries(button_test PRIVATE Curses::Curses)
            target_link_libraries(dialog_test PRIVATE Curses::Curses)
            target_link_libraries(menu_bar_test PRIVATE Curses::Curses)
        else()
            target_link_libraries(draw_list_contract_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(statusbar_drawlist_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(ansi_renderer_diff_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(text_input_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(scroll_list_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(text_buffer_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(button_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(dialog_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(menu_bar_test PRIVATE ${CURSES_LIBRARIES})
        endif()
    endif()
endif()

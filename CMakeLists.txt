cmake_minimum_required(VERSION 3.16)
project(RetroDesk VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_WERROR "Treat warnings as errors" ON)
option(ENABLE_SANITIZERS "Enable Address and UndefinedBehavior sanitizers" OFF)
option(ENABLE_TESTS "Build lightweight unit tests" ON)
option(RETROCORE_SPEC_REQUIRED "Fail when shared retrocore fixtures are missing" OFF)
set(PDCURSES_ROOT "" CACHE PATH "Path to PDCurses root")
set(RETROCORE_SPEC_DIR "${CMAKE_SOURCE_DIR}/../retrocore-spec" CACHE PATH
    "Path to a retrocore-spec checkout for shared contract fixture tests")

set(RETRODESK_SOURCES
    src/main.c
    src/core/cli.c
    src/core/clipboard.c
    src/core/desktop.c
    src/core/key_chord.c
    src/core/utf8.c
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
    src/ui/progress_bar.c
    src/ui/tab.c
    src/ui/theme.c
    src/apps/apps.c
    src/apps/filemanager_app.c
    src/apps/notepad_app.c
    src/apps/terminal_app.c
)

# DJGPP/DOS profile sources. Identical to RETRODESK_SOURCES minus the
# raw-TTY backend (POSIX poll/ioctl/termios — not available on DOS).
# Makefile.djgpp must stay in sync with this list; the script
# scripts/check_djgpp_sources.py enforces that.
set(RETRODESK_DJGPP_SOURCES
    src/main.c
    src/core/cli.c
    src/core/clipboard.c
    src/core/desktop.c
    src/core/key_chord.c
    src/core/utf8.c
    src/platform/platform.c
    src/platform/platform_curses.c
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
    src/ui/progress_bar.c
    src/ui/tab.c
    src/ui/theme.c
    src/apps/apps.c
    src/apps/filemanager_app.c
    src/apps/notepad_app.c
    src/apps/terminal_app.c
)

# The first product slice uses the POSIX storage adapter. Windows and DOS use
# the header-only unsupported adapter until their native implementations land.
set(RETRODESK_STORAGE_SOURCE)
if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(RETRODESK_STORAGE_SOURCE src/storage/retro_fs_posix.c)
    list(APPEND RETRODESK_SOURCES ${RETRODESK_STORAGE_SOURCE})
endif()

function(retrodesk_apply_quality target)
    target_compile_features(${target} PRIVATE c_std_11)
    target_compile_definitions(${target} PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:_POSIX_C_SOURCE=200809L>
    )
    if (ENABLE_STRICT_WARNINGS)
        target_compile_options(${target} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:/W4>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>
        )
    endif()
    if (ENABLE_WERROR)
        target_compile_options(${target} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:/WX>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>
        )
    endif()
    if (ENABLE_SANITIZERS)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
        else()
            message(FATAL_ERROR
                "ENABLE_SANITIZERS requires a GCC- or Clang-compatible compiler")
        endif()
    endif()
endfunction()

add_executable(retrodesk ${RETRODESK_SOURCES})
target_include_directories(retrodesk PRIVATE ${CMAKE_SOURCE_DIR}/src)
retrodesk_apply_quality(retrodesk)

include(GNUInstallDirs)
install(TARGETS retrodesk RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md INSTALL.md LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

if (ENABLE_TESTS)
    include(CTest)
    if (BUILD_TESTING)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME test_oracle_guard
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_test_oracles.py)
        add_custom_target(check-test-oracles
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_test_oracles.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM)
        add_executable(cli_parse_test
            tests/cli_parse_test.c
            src/core/cli.c
            src/ui/theme.c
        )
        target_include_directories(cli_parse_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME cli_parse_test COMMAND cli_parse_test)

        add_executable(key_chord_test
            tests/key_chord_test.c
            src/core/key_chord.c
            src/core/utf8.c
        )
        target_include_directories(key_chord_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME key_chord_test COMMAND key_chord_test)

        add_executable(utf8_test
            tests/utf8_test.c
            src/core/utf8.c
        )
        target_include_directories(utf8_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME utf8_test COMMAND utf8_test)

        add_executable(clipboard_test
            tests/clipboard_test.c
            src/core/clipboard.c
            src/core/utf8.c
        )
        target_include_directories(clipboard_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME clipboard_test COMMAND clipboard_test)

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
    if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        set(CURSES_NEED_WIDE TRUE)
    endif()
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

    # When Curses was supplied by vcpkg's pdcurses port (or any other
    # PDCurses provider), activate HAVE_PDCURSES + PDC_NCMOUSE so the
    # platform layer enables extended mouse and resize behavior.
    string(TOLOWER "${CURSES_LIBRARIES}" _curses_lib_lower)
    string(FIND "${_curses_lib_lower}" "pdcurses" _pdcurses_idx)
    if (NOT _pdcurses_idx EQUAL -1)
        target_compile_definitions(retrodesk PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
    endif()
endif()

if (ENABLE_TESTS AND BUILD_TESTING)
    add_executable(draw_list_contract_test
        tests/draw_list_contract_test.c
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
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
        src/ui/text_input.c
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
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
        src/core/key_chord.c
        src/core/utf8.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(menu_bar_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME menu_bar_test COMMAND menu_bar_test)

    add_executable(progress_bar_test
        tests/progress_bar_test.c
        src/ui/progress_bar.c
        src/core/key_chord.c
        src/core/utf8.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(progress_bar_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME progress_bar_test COMMAND progress_bar_test)

    add_executable(tab_test
        tests/tab_test.c
        src/ui/tab.c
        src/core/key_chord.c
        src/core/utf8.c
        src/platform/platform.c
        src/platform/platform_curses.c
        src/platform/platform_tty_raw.c
        src/platform/tty_decoder.c
        src/render/render.c
        src/render/ansi_renderer.c
    )
    target_include_directories(tab_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME tab_test COMMAND tab_test)

    add_executable(desktop_runtime_test
        tests/desktop_runtime_test.c
        tests/wm_render_stub.c
        src/core/desktop.c
        src/core/clipboard.c
        src/core/utf8.c
        src/app/app_runtime.c
        src/apps/apps.c
        src/apps/filemanager_app.c
        src/apps/notepad_app.c
        src/apps/terminal_app.c
        ${RETRODESK_STORAGE_SOURCE}
        src/ui/text_buffer.c
        src/ui/text_input.c
        src/wm/window_manager.c
        src/ui/statusbar.c
        src/ui/theme.c
    )
    target_include_directories(desktop_runtime_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME desktop_runtime_test COMMAND desktop_runtime_test)

    if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        add_executable(storage_test
            tests/storage_test.c
            src/storage/retro_fs_posix.c
            src/core/utf8.c
        )
        target_include_directories(storage_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME storage_test COMMAND storage_test)
    endif()

    if (EXISTS "${RETROCORE_SPEC_DIR}/fixtures/events/open-files-and-focus.json")
        add_executable(retrocore_event_fixture_test
            tests/retrocore_event_fixture_test.c
            tests/wm_render_stub.c
            src/core/desktop.c
            src/core/clipboard.c
            src/core/utf8.c
            src/app/app_runtime.c
            src/apps/apps.c
            src/apps/filemanager_app.c
            src/apps/notepad_app.c
            src/apps/terminal_app.c
            ${RETRODESK_STORAGE_SOURCE}
            src/ui/text_buffer.c
            src/ui/text_input.c
            src/wm/window_manager.c
            src/ui/statusbar.c
            src/ui/theme.c
        )
        target_include_directories(retrocore_event_fixture_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        target_compile_definitions(retrocore_event_fixture_test PRIVATE
            RETROCORE_SPEC_DIR=${RETROCORE_SPEC_DIR}
        )
        add_test(NAME retrocore_event_fixture_test COMMAND retrocore_event_fixture_test)
    else()
        if (RETROCORE_SPEC_REQUIRED)
            message(FATAL_ERROR
                "retrocore-spec fixture missing: ${RETROCORE_SPEC_DIR}/fixtures/events/open-files-and-focus.json")
        endif()
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
        target_include_directories(progress_bar_test PRIVATE "${_curses_include}")
        target_include_directories(tab_test PRIVATE "${_curses_include}")
        target_link_libraries(draw_list_contract_test PRIVATE "${_curses_lib}")
        target_link_libraries(statusbar_drawlist_test PRIVATE "${_curses_lib}")
        target_link_libraries(ansi_renderer_diff_test PRIVATE "${_curses_lib}")
        target_link_libraries(text_input_test PRIVATE "${_curses_lib}")
        target_link_libraries(scroll_list_test PRIVATE "${_curses_lib}")
        target_link_libraries(text_buffer_test PRIVATE "${_curses_lib}")
        target_link_libraries(button_test PRIVATE "${_curses_lib}")
        target_link_libraries(dialog_test PRIVATE "${_curses_lib}")
        target_link_libraries(menu_bar_test PRIVATE "${_curses_lib}")
        target_link_libraries(progress_bar_test PRIVATE "${_curses_lib}")
        target_link_libraries(tab_test PRIVATE "${_curses_lib}")
        foreach(_t draw_list_contract_test statusbar_drawlist_test ansi_renderer_diff_test
                text_input_test scroll_list_test text_buffer_test button_test
                dialog_test menu_bar_test progress_bar_test tab_test)
            target_compile_definitions(${_t} PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
        endforeach()
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
            target_include_directories(progress_bar_test PRIVATE ${CURSES_INCLUDE_DIRS})
            target_include_directories(tab_test PRIVATE ${CURSES_INCLUDE_DIRS})
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
            target_include_directories(progress_bar_test PRIVATE ${CURSES_INCLUDE_DIR})
            target_include_directories(tab_test PRIVATE ${CURSES_INCLUDE_DIR})
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
            target_link_libraries(progress_bar_test PRIVATE Curses::Curses)
            target_link_libraries(tab_test PRIVATE Curses::Curses)
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
            target_link_libraries(progress_bar_test PRIVATE ${CURSES_LIBRARIES})
            target_link_libraries(tab_test PRIVATE ${CURSES_LIBRARIES})
        endif()

        # Mirror the retrodesk target's PDCurses auto-detection so test
        # binaries see the same defines when built via vcpkg's pdcurses
        # port (or any other PDCurses-resolving Curses package).
        if (NOT _pdcurses_idx EQUAL -1)
            foreach(_t draw_list_contract_test statusbar_drawlist_test ansi_renderer_diff_test
                    text_input_test scroll_list_test text_buffer_test button_test
                    dialog_test menu_bar_test progress_bar_test tab_test)
                target_compile_definitions(${_t} PRIVATE HAVE_PDCURSES PDC_NCMOUSE)
            endforeach()
        endif()
    endif()

    set(RETRODESK_TEST_TARGETS
        cli_parse_test
        key_chord_test
        utf8_test
        clipboard_test
        app_registry_test
        platform_features_test
        wm_event_replay_test
        theme_catalog_test
        draw_list_contract_test
        statusbar_drawlist_test
        ansi_renderer_diff_test
        tty_decoder_test
        text_input_test
        scroll_list_test
        text_buffer_test
        button_test
        dialog_test
        menu_bar_test
        progress_bar_test
        tab_test
        desktop_runtime_test
    )
    if (TARGET storage_test)
        list(APPEND RETRODESK_TEST_TARGETS storage_test)
    endif()
    if (TARGET retrocore_event_fixture_test)
        list(APPEND RETRODESK_TEST_TARGETS retrocore_event_fixture_test)
    endif()
    foreach(_test_target IN LISTS RETRODESK_TEST_TARGETS)
        retrodesk_apply_quality(${_test_target})
    endforeach()
endif()

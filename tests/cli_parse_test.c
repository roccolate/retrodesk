#include <assert.h>

#include "core/cli.h"

int main(void) {
    RetroCliOptions opt = {0};

    char *argv_default[] = {"retrodesk"};
    assert(retro_cli_parse(1, argv_default, &opt, NULL) == RETRO_CLI_OK);
    assert(!opt.bench_mode);
    assert(opt.render_backend == RENDER_BACKEND_AUTO);
    assert(opt.input_backend == INPUT_BACKEND_UNKNOWN);
    assert(opt.theme_kind == RETRO_THEME_XP);

    char *argv_ansi[] = {"retrodesk", "--render-backend=ansi"};
    assert(retro_cli_parse(2, argv_ansi, &opt, NULL) == RETRO_CLI_OK);
    assert(opt.render_backend == RENDER_BACKEND_ANSI);
    assert(opt.input_backend == INPUT_BACKEND_TTY_RAW);

    char *argv_tty[] = {"retrodesk", "--input-backend=tty"};
    assert(retro_cli_parse(2, argv_tty, &opt, NULL) == RETRO_CLI_OK);
    assert(opt.input_backend == INPUT_BACKEND_TTY_RAW);
    assert(opt.render_backend == RENDER_BACKEND_ANSI);

    char *argv_combo_ok[] = {"retrodesk", "--bench-startup", "--render-backend=ansi",
                             "--input-backend=tty", "--theme=amiga"};
    assert(retro_cli_parse(5, argv_combo_ok, &opt, NULL) == RETRO_CLI_OK);
    assert(opt.bench_mode);
    assert(opt.theme_kind == RETRO_THEME_AMIGA);

    char *argv_help[] = {"retrodesk", "--help"};
    assert(retro_cli_parse(2, argv_help, &opt, NULL) == RETRO_CLI_SHOWED_USAGE);

    char *argv_short_help[] = {"retrodesk", "-h"};
    assert(retro_cli_parse(2, argv_short_help, &opt, NULL) == RETRO_CLI_SHOWED_USAGE);

    char *argv_invalid_combo[] = {"retrodesk", "--input-backend=tty",
                                  "--render-backend=curses"};
    assert(retro_cli_parse(3, argv_invalid_combo, &opt, NULL) == RETRO_CLI_PARSE_ERROR);

    char *argv_invalid_combo_reverse[] = {"retrodesk", "--input-backend=curses",
                                          "--render-backend=ansi"};
    assert(retro_cli_parse(3, argv_invalid_combo_reverse, &opt, NULL) ==
           RETRO_CLI_PARSE_ERROR);

    char *argv_invalid_theme[] = {"retrodesk", "--theme=bad-theme"};
    assert(retro_cli_parse(2, argv_invalid_theme, &opt, NULL) == RETRO_CLI_PARSE_ERROR);

    char *argv_unknown_flag[] = {"retrodesk", "--wat"};
    assert(retro_cli_parse(2, argv_unknown_flag, &opt, NULL) == RETRO_CLI_PARSE_ERROR);

    assert(retro_cli_parse(0, argv_default, &opt, NULL) == RETRO_CLI_PARSE_ERROR);
    assert(retro_cli_parse(1, NULL, &opt, NULL) == RETRO_CLI_PARSE_ERROR);
    assert(retro_cli_parse(1, argv_default, NULL, NULL) == RETRO_CLI_PARSE_ERROR);

    return 0;
}

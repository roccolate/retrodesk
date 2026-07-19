#include "test_harness.h"

#include <stdio.h>

#include "core/cli.h"

static long stream_size(FILE *stream) {
    TEST_REQUIRE(stream != NULL);
    TEST_REQUIRE(fflush(stream) == 0);
    long size = ftell(stream);
    TEST_REQUIRE(size >= 0);
    return size;
}

int main(void) {
    RetroCliOptions opt = {0};

    char *argv_default[] = {"retrodesk"};
    TEST_REQUIRE(retro_cli_parse(1, argv_default, &opt, NULL, NULL) == RETRO_CLI_OK);
    TEST_REQUIRE(!opt.bench_mode);
    TEST_REQUIRE(!opt.diagnose_mode);
    TEST_REQUIRE(!opt.version_mode);
    TEST_REQUIRE(opt.render_backend == RENDER_BACKEND_AUTO);
    TEST_REQUIRE(opt.input_backend == INPUT_BACKEND_UNKNOWN);
    TEST_REQUIRE(opt.theme_kind == RETRO_THEME_XP);

    char *argv_ansi[] = {"retrodesk", "--render-backend=ansi"};
    TEST_REQUIRE(retro_cli_parse(2, argv_ansi, &opt, NULL, NULL) == RETRO_CLI_OK);
    TEST_REQUIRE(opt.render_backend == RENDER_BACKEND_ANSI);
    TEST_REQUIRE(opt.input_backend == INPUT_BACKEND_TTY_RAW);

    char *argv_tty[] = {"retrodesk", "--input-backend=tty"};
    TEST_REQUIRE(retro_cli_parse(2, argv_tty, &opt, NULL, NULL) == RETRO_CLI_OK);
    TEST_REQUIRE(opt.input_backend == INPUT_BACKEND_TTY_RAW);
    TEST_REQUIRE(opt.render_backend == RENDER_BACKEND_ANSI);

    char *argv_combo_ok[] = {"retrodesk", "--bench-startup", "--render-backend=ansi",
                             "--input-backend=tty", "--theme=amiga"};
    TEST_REQUIRE(retro_cli_parse(5, argv_combo_ok, &opt, NULL, NULL) == RETRO_CLI_OK);
    TEST_REQUIRE(opt.bench_mode);
    TEST_REQUIRE(opt.theme_kind == RETRO_THEME_AMIGA);

    char *argv_diagnose[] = {"retrodesk", "--diagnose"};
    TEST_REQUIRE(retro_cli_parse(2, argv_diagnose, &opt, NULL, NULL) == RETRO_CLI_OK);
    TEST_REQUIRE(opt.diagnose_mode);
    TEST_REQUIRE(opt.bench_mode);

    char *argv_version[] = {"retrodesk", "--version"};
    TEST_REQUIRE(retro_cli_parse(2, argv_version, &opt, NULL, NULL) == RETRO_CLI_OK);
    TEST_REQUIRE(opt.version_mode);

    char *argv_invalid_combo[] = {"retrodesk", "--input-backend=tty",
                                  "--render-backend=curses"};
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    TEST_REQUIRE(out != NULL);
    TEST_REQUIRE(err != NULL);
    TEST_REQUIRE(retro_cli_parse(3, argv_invalid_combo, &opt, out, err) ==
                 RETRO_CLI_PARSE_ERROR);
    TEST_REQUIRE(stream_size(out) == 0);
    TEST_REQUIRE(stream_size(err) > 0);
    fclose(out);
    fclose(err);

    char *argv_invalid_combo_reverse[] = {"retrodesk", "--input-backend=curses",
                                          "--render-backend=ansi"};
    TEST_REQUIRE(retro_cli_parse(3, argv_invalid_combo_reverse, &opt, NULL, NULL) ==
                 RETRO_CLI_PARSE_ERROR);

    char *argv_invalid_theme[] = {"retrodesk", "--theme=bad-theme"};
    TEST_REQUIRE(retro_cli_parse(2, argv_invalid_theme, &opt, NULL, NULL) ==
                 RETRO_CLI_PARSE_ERROR);

    char *argv_unknown_flag[] = {"retrodesk", "--wat"};
    TEST_REQUIRE(retro_cli_parse(2, argv_unknown_flag, &opt, NULL, NULL) ==
                 RETRO_CLI_PARSE_ERROR);

    char *argv_help[] = {"retrodesk", "--help"};
    out = tmpfile();
    err = tmpfile();
    TEST_REQUIRE(out != NULL);
    TEST_REQUIRE(err != NULL);
    TEST_REQUIRE(retro_cli_parse(2, argv_help, &opt, out, err) ==
                 RETRO_CLI_SHOWED_USAGE);
    TEST_REQUIRE(stream_size(out) > 0);
    TEST_REQUIRE(stream_size(err) == 0);
    fclose(out);
    fclose(err);

    char *argv_short_help[] = {"retrodesk", "-h"};
    TEST_REQUIRE(retro_cli_parse(2, argv_short_help, &opt, NULL, NULL) ==
                 RETRO_CLI_SHOWED_USAGE);

    TEST_REQUIRE(retro_cli_parse(0, argv_default, &opt, NULL, NULL) ==
                 RETRO_CLI_PARSE_ERROR);
    TEST_REQUIRE(retro_cli_parse(1, NULL, &opt, NULL, NULL) == RETRO_CLI_PARSE_ERROR);
    TEST_REQUIRE(retro_cli_parse(1, argv_default, NULL, NULL, NULL) ==
                 RETRO_CLI_PARSE_ERROR);

    return 0;
}

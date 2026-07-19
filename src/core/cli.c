#include "core/cli.h"

#include <string.h>

void retro_cli_default(RetroCliOptions *out) {
    if (!out) return;
    out->bench_mode = false;
    out->diagnose_mode = false;
    out->version_mode = false;
    out->render_backend = RENDER_BACKEND_AUTO;
    out->input_backend = INPUT_BACKEND_UNKNOWN;
    out->theme_kind = RETRO_THEME_XP;
}

void retro_cli_print_usage(FILE *out, const char *argv0) {
    if (!out) return;
    fprintf(out,
            "usage: %s [--help] [--version] [--diagnose] [--bench-startup] [--render-backend=curses|ansi] [--input-backend=curses|tty] [--theme=xp|hacker|amiga|win31]\n",
            argv0 ? argv0 : "retrodesk");
}

static RetroCliParseResult retro_cli_apply_backend_defaults(RetroCliOptions *opt,
                                                            FILE *err) {
    if (!opt) return RETRO_CLI_PARSE_ERROR;

    if (opt->render_backend == RENDER_BACKEND_ANSI &&
        opt->input_backend == INPUT_BACKEND_UNKNOWN) {
        opt->input_backend = INPUT_BACKEND_TTY_RAW;
    }
    if (opt->input_backend == INPUT_BACKEND_TTY_RAW &&
        opt->render_backend == RENDER_BACKEND_AUTO) {
        opt->render_backend = RENDER_BACKEND_ANSI;
    }
    if (opt->input_backend == INPUT_BACKEND_TTY_RAW &&
        opt->render_backend == RENDER_BACKEND_CURSES) {
        if (err) {
            fputs("invalid backend combination: --input-backend=tty requires --render-backend=ansi\n",
                  err);
        }
        return RETRO_CLI_PARSE_ERROR;
    }
    if (opt->input_backend == INPUT_BACKEND_NCURSES &&
        opt->render_backend == RENDER_BACKEND_ANSI) {
        if (err) {
            fputs("invalid backend combination: --input-backend=curses requires --render-backend=curses\n",
                  err);
        }
        return RETRO_CLI_PARSE_ERROR;
    }
    return RETRO_CLI_OK;
}

RetroCliParseResult retro_cli_parse(int argc, char **argv, RetroCliOptions *out,
                                    FILE *standard_out, FILE *err) {
    if (!out || argc < 1 || !argv) return RETRO_CLI_PARSE_ERROR;
    retro_cli_default(out);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            retro_cli_print_usage(standard_out, argv[0]);
            return RETRO_CLI_SHOWED_USAGE;
        }
        if (strcmp(argv[i], "--bench-startup") == 0) {
            out->bench_mode = true;
            continue;
        }
        if (strcmp(argv[i], "--diagnose") == 0) {
            out->diagnose_mode = true;
            out->bench_mode = true;
            continue;
        }
        if (strcmp(argv[i], "--version") == 0) {
            out->version_mode = true;
            continue;
        }
        if (strcmp(argv[i], "--render-backend=ansi") == 0) {
            out->render_backend = RENDER_BACKEND_ANSI;
            continue;
        }
        if (strcmp(argv[i], "--render-backend=curses") == 0) {
            out->render_backend = RENDER_BACKEND_CURSES;
            continue;
        }
        if (strcmp(argv[i], "--input-backend=tty") == 0) {
            out->input_backend = INPUT_BACKEND_TTY_RAW;
            continue;
        }
        if (strcmp(argv[i], "--input-backend=curses") == 0) {
            out->input_backend = INPUT_BACKEND_NCURSES;
            continue;
        }
        if (strncmp(argv[i], "--theme=", 8) == 0) {
            if (!retro_theme_parse(argv[i] + 8, &out->theme_kind)) {
                if (err) {
                    fprintf(err,
                            "unknown theme '%s' (valid: xp|hacker|amiga|win31)\n",
                            argv[i] + 8);
                }
                return RETRO_CLI_PARSE_ERROR;
            }
            continue;
        }
        if (err) retro_cli_print_usage(err, argv[0]);
        return RETRO_CLI_PARSE_ERROR;
    }

    return retro_cli_apply_backend_defaults(out, err);
}

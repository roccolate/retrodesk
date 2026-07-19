#ifndef RETRODESK_CORE_CLI_H
#define RETRODESK_CORE_CLI_H

#include <stdbool.h>
#include <stdio.h>

#include "platform/platform.h"
#include "render/render.h"
#include "ui/theme.h"

/* Command-line option parser. Distinguishes a successful help request from
   invalid input so callers can return the correct process status. */

typedef enum RetroCliParseResult {
    RETRO_CLI_OK = 0,
    RETRO_CLI_SHOWED_USAGE,
    RETRO_CLI_PARSE_ERROR,
} RetroCliParseResult;

typedef struct RetroCliOptions {
    bool bench_mode;
    bool diagnose_mode;
    bool version_mode;
    RenderBackendKind render_backend;
    InputBackendKind input_backend;
    RetroThemeKind theme_kind;
} RetroCliOptions;

void retro_cli_default(RetroCliOptions *out);
void retro_cli_print_usage(FILE *out, const char *argv0);
RetroCliParseResult retro_cli_parse(int argc, char **argv, RetroCliOptions *out,
                                    FILE *standard_out, FILE *err);

#endif

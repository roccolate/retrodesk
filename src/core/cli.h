#ifndef RETRODESK_CORE_CLI_H
#define RETRODESK_CORE_CLI_H

#include <stdbool.h>
#include <stdio.h>

#include "platform/platform.h"
#include "render/render.h"
#include "ui/theme.h"

typedef struct RetroCliOptions {
    bool bench_mode;
    RenderBackendKind render_backend;
    InputBackendKind input_backend;
    RetroThemeKind theme_kind;
} RetroCliOptions;

void retro_cli_default(RetroCliOptions *out);
void retro_cli_print_usage(FILE *out, const char *argv0);
bool retro_cli_parse(int argc, char **argv, RetroCliOptions *out, FILE *err);

#endif

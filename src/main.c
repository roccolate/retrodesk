#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/cli.h"
#include "core/desktop.h"
#include "platform/platform.h"

#define RETRODESK_VERSION "0.2.0-alpha"

enum { DEFAULT_INPUT_TIMEOUT_MS = 120 };

int main(int argc, char **argv) {
    RetroCliOptions options = {0};
    RetroCliParseResult parse_result =
        retro_cli_parse(argc, argv, &options, stdout, stderr);
    if (parse_result == RETRO_CLI_SHOWED_USAGE) return EXIT_SUCCESS;
    if (parse_result == RETRO_CLI_PARSE_ERROR) return EXIT_FAILURE;
    if (options.version_mode) {
        puts("retrodesk " RETRODESK_VERSION);
        return EXIT_SUCCESS;
    }

    setlocale(LC_ALL, "");

    PlatformConfig platform_cfg = {
        .bench_mode = options.bench_mode,
        .input_timeout_ms = DEFAULT_INPUT_TIMEOUT_MS,
        .requested_input_backend = options.input_backend,
    };
    PlatformBackend *platform = platform_create(&platform_cfg);
    if (!platform) {
        fprintf(stderr,
                "platform initialization failed (terminal/console + selected input backend required)\n");
#if defined(_WIN32)
        fprintf(stderr,
                "Windows tip: run from cmd/PowerShell and ensure PDCurses runtime is available.\n");
#endif
        return EXIT_FAILURE;
    }

    DesktopConfig desktop_cfg = {
        .platform = platform,
        .input_timeout_ms = DEFAULT_INPUT_TIMEOUT_MS,
        .bench_mode = options.bench_mode,
        .render_backend = options.render_backend,
        .theme_kind = options.theme_kind,
    };
    Desktop *desktop = desktop_create(&desktop_cfg);
    if (!desktop) {
        fprintf(stderr, "desktop initialization failed\n");
        platform_destroy(platform);
        return EXIT_FAILURE;
    }

    if (options.bench_mode) {
        const DesktopDiagnostics *diag = desktop_diagnostics(desktop);
        if (diag) {
            printf("input_backend: %s\n", diag->backend_name);
            printf("render_backend: %s\n", diag->render_backend_name);
            printf("theme: %s\n", diag->theme_name);
            printf("mouse_enabled: %s\n", diag->mouse_enabled ? "true" : "false");
            printf("drag_enabled: %s\n", diag->drag_enabled ? "true" : "false");
            printf("resize_enabled: %s\n", diag->resize_enabled ? "true" : "false");
            printf("linux_tty_keyboard_only: %s\n",
                   diag->linux_tty_keyboard_only ? "true" : "false");
        }
    }
    if (options.diagnose_mode) {
        const DesktopDiagnostics *diag = desktop_diagnostics(desktop);
        if (diag) {
            printf("capabilities: mouse=%s drag=%s resize=%s\n",
                   diag->mouse_enabled ? "yes" : "no",
                   diag->drag_enabled ? "yes" : "no",
                   diag->resize_enabled ? "yes" : "no");
        }
        desktop_shutdown(desktop);
        platform_destroy(platform);
        return EXIT_SUCCESS;
    }

    int rc = desktop_run(desktop);
    desktop_shutdown(desktop);
    platform_destroy(platform);
    return rc;
}

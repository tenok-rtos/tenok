#include <stdio.h>
#include <string.h>

#include "quadrotor.h"
#include "shell.h"

int esc_calib(int argc, char *argv[])
{
    if (is_flight_ctrl_running()) {
        shell_puts("command rejected, flight control is running.\n\r");
        return 0;
    }

    /* Confirmation */
    struct shell shell;
    shell_init_minimal(&shell);
    shell_set_prompt(&shell, "confirm esc calibration command [y/n]: ");
    shell_listen(&shell);

    if (strcmp(shell.buf, "y") == 0 || strcmp(shell.buf, "Y") == 0) {
        shell_puts(
            "start esc calibration.\n\r"
            "restart the system after finishing the calibration.\n\r");
        trigger_esc_calibration();
    } else {
        shell_puts("abort.\n\r");
    }

    return 0;
}

HOOK_SHELL_CMD("esc_calib", esc_calib);

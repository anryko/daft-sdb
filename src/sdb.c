#include <stdio.h>
#include <string.h>

#include "utils.c"

int main(int argc, char *argv[]) {
    Proc proc;

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        pid_t pid = (pid_t) strtol(argv[2], NULL, 10);
        if (pid <= 0) {
            fprintf(stderr, "Invalid PID: %s\n", argv[2]);
            return 1;
        }

        if (!proc_attach(&proc, pid)) {
            perror("Failed to attach");
            return 1;
        }

        printf("Attached to PID %d\n", proc.pid);

    } else if (argc == 2) {
        if (!proc_launch(&proc, argv[1])) {
            perror("Failed to launch program");
            return 1;
        }

        printf("Launched PID %d\n", proc.pid);

    } else {
        fprintf(stderr, "Usage: %s [<program>|-p <pid>]\n", argv[0]);
        return 1;
    }

    History h = {0};
    char *line = nullptr;

    while ((line = readline("sdb> ")) != nullptr) {
        const char *cmd = line;

        if (cmd[0] == '\0') {
            if (h.count > 0) {
                cmd = history_last(&h);
            }
        } else {
            char *copy = strdup(cmd);
            if (copy != nullptr) {
                history_append(&h, copy);
            }
        }

        handle_command(&proc, cmd);
        free(line);
    }

    ProcResult result;
    do {
        result = proc_wait(&proc);

        if (result.state == PROCESS_STOPPED) {
            printf("Process stopped by signal %d, resuming...\n", result.info);
            if (proc_resume(&proc) != 0) {
                perror("Failed to resume process");
                break;
            }
        } else if (result.state == PROCESS_TERMINATED) {
            fprintf(stderr, "Error waiting for process: %d\n", result.info);
            break;
        }

        // small sleep to avoid busy-waiting
        usleep(10000); // 10ms
    } while (result.state != PROCESS_EXITED && result.state != PROCESS_TERMINATED);

    printf("Process exited with info %d\n", result.info);

    proc_destroy(&proc);
    history_free(&h);
    return 0;
}

#include <stdio.h>
#include <string.h>

#include "utils.c"

Proc *attach(int argc, char *argv[])
{
    Proc *proc = malloc(sizeof(Proc));

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        pid_t pid = (pid_t) strtol(argv[2], NULL, 10);
        if (pid <= 0) {
            fprintf(stderr, "Invalid PID: %s\n", argv[2]);
            return nullptr;
        }

        if (!proc_attach(proc, pid)) {
            perror("Failed to attach");
            return nullptr;
        }

        printf("Attached to PID %d\n", proc->pid);

    } else if (argc == 2) {
        if (!proc_launch(proc, argv[1])) {
            perror("Failed to launch program");
            return nullptr;
        }

        printf("Launched PID %d\n", proc->pid);

    } else {
        fprintf(stderr, "Usage: %s [<program>|-p <pid>]\n", argv[0]);
        return nullptr;
    }

    return proc;
}

void main_loop(Proc *proc)
{
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

        if (cmd[0] != '\0') {
            handle_command(proc, cmd);
        }

        free(line);
    }

    history_free(&h);
}

int main(int argc, char *argv[]) {

    auto proc = attach(argc, argv);
    if (!proc) return 1;

    main_loop(proc);

    proc_destroy(proc);
    return 0;
}

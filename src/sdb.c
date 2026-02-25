#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "utils.c"

typedef enum {
    PROCESS_RUNNING,
    PROCESS_STOPPED,
    PROCESS_EXITED,
    PROCESS_TERMINATED
} ProcState;

typedef struct {
    ProcState state;
    int info; // exit code or signal
} ProcResult;

typedef struct {
    pid_t pid;
    bool terminate_on_end;
    ProcState state;
} Proc;

static ProcResult proc_stop_reason_from_status(int status) {
    ProcResult result;

    if (WIFEXITED(status)) {
        result.state = PROCESS_EXITED;
        result.info = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.state = PROCESS_TERMINATED;
        result.info = WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        result.state = PROCESS_STOPPED;
        result.info = WSTOPSIG(status);
    } else {
        result.state = PROCESS_TERMINATED;
        result.info = 0;
    }

    return result;
}

// Blocking wait
ProcResult proc_wait(Proc *proc) {
    if (!proc || proc->pid <= 0) {
        ProcResult invalid = { .state = PROCESS_TERMINATED, .info = -1 };
        return invalid;
    }

    int status;
    if (waitpid(proc->pid, &status, 0) < 0) {
        ProcResult fail = { .state = PROCESS_TERMINATED, .info = errno };
        return fail;
    }

    ProcResult reason = proc_stop_reason_from_status(status);
    proc->state = reason.state;
    return reason;
}

bool proc_launch(Proc *proc, const char *path) {
    if (!proc || !path) return false;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) { // Child
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) _exit(errno);
        execlp(path, path, nullptr);
        _exit(errno); // exec failed
    }

    // Parent
    proc->pid = pid;
    proc->terminate_on_end = true;
    proc->state = PROCESS_STOPPED;

    proc_wait(proc); // initial stop after PTRACE_TRACEME
    return true;
}

bool proc_attach(Proc *proc, pid_t pid) {
    if (!proc || pid <= 0) return false;

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) return false;

    proc->pid = pid;
    proc->terminate_on_end = false;
    proc->state = PROCESS_STOPPED;

    proc_wait(proc); // initial stop after attach
    return true;
}

bool proc_resume(Proc *proc) {
    if (!proc || proc->pid <= 0) return false;

    if (ptrace(PTRACE_CONT, proc->pid, nullptr, nullptr) < 0) return false;

    proc->state = PROCESS_RUNNING;
    return true;
}

void proc_destroy(Proc *proc) {
    if (!proc || proc->pid <= 0) return;

    int status;

    if (proc->state == PROCESS_RUNNING) {
        kill(proc->pid, SIGSTOP);
        waitpid(proc->pid, &status, 0);
    }

    ptrace(PTRACE_DETACH, proc->pid, nullptr, nullptr);
    kill(proc->pid, SIGCONT);

    if (proc->terminate_on_end) {
        kill(proc->pid, SIGKILL);
        waitpid(proc->pid, &status, 0);
    }

    proc->pid = 0;
    proc->state = PROCESS_TERMINATED;
}

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

        printf("%s\n", cmd);
        free(line);
        // history_free(&h);
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
    return 0;
}

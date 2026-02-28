#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

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

void proc_result_print(Proc *proc, ProcResult *result)
{
    printf("Process %d ", proc->pid);
    switch (result->state) {
        case PROCESS_EXITED:
            printf("exited with status %d", result->info);
            break;
        case PROCESS_TERMINATED:
            printf("terminated with signal %s", sigabbrev_np(result->info));
            break;
        case PROCESS_STOPPED:
            printf("stoped with signal %s", sigabbrev_np(result->info));
            break;
        case PROCESS_RUNNING:
            printf("is running");
            break;
    }
    printf("\n");
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

    proc_wait(proc);
    return true;
}

bool proc_attach(Proc *proc, pid_t pid) {
    if (!proc || pid <= 0) return false;

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) return false;

    proc->pid = pid;
    proc->terminate_on_end = false;
    proc->state = PROCESS_STOPPED;

    proc_wait(proc);
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

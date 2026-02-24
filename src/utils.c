#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LINE_LEN_MAX 256

char *readline(const char *prompt)
{
    static char line[LINE_LEN_MAX];

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (!fgets(line, sizeof(line), stdin)) return nullptr;

    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\n') {
        line[--n] = '\0';
    }

    char *p = malloc(n + 1);
    if (!p) return nullptr;

    memcpy(p, line, n + 1);
    return p;
}

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define LINE_LEN_MAX 256
#define DA_INIT_CAP 256
#define DECLTYPE_CAST(T)

// Dynamic Array helpers.
#define da_reserve(da, expected_capacity)                                \
    do {                                                                 \
        if ((expected_capacity) > (da)->capacity) {                      \
            if ((da)->capacity == 0) {                                   \
                (da)->capacity = DA_INIT_CAP;                            \
            }                                                            \
            while ((expected_capacity) > (da)->capacity) {               \
                (da)->capacity *= 2;                                     \
            }                                                            \
            (da)->items = DECLTYPE_CAST((da)->items)realloc(             \
                    (da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol");           \
        }                                                                \
    } while (0)

#define da_append(da, item)                  \
    do {                                     \
        da_reserve((da), (da)->count + 1);   \
        (da)->items[(da)->count++] = (item); \
    } while (0)

#define da_foreach(Type, it, da) for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

#define da_free(da) free((da).items)

#define da_free_with(Type, da, dtor) \
    do {                             \
        da_foreach(Type, _it, da) {  \
            dtor(*_it);              \
        }                            \
        free((da)->items);           \
        (da)->items = nullptr;       \
        (da)->count = 0;             \
        (da)->capacity = 0;          \
    } while (0)

// Command History.
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} History;

#define history_append(hp, s) da_append((hp), (s))
#define history_last(hp) ((hp)->items[(hp)->count - 1])
#define history_free(hp) da_free_with(char*, (hp), free)


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

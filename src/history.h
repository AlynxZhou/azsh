#ifndef __HISTORY_H__
#define __HISTORY_H__

#include <stdio.h>
#include "azsh.h"

// One more for end (always unused) and one more for current.
#define HISTORY_SIZE (10 + 2)
#define LINE_SIZE_UNIT 80

typedef struct history *history_t;

history_t azsh_create_history(void);
void azsh_delete_history(history_t h);
// It's safe to free s after this returns;
void azsh_put_history(history_t h, const char *s);
// But shouldn't free return value of this.
char *azsh_get_history(history_t h, int i);
// f can also be stdout!
void azsh_write_history(history_t h, FILE *f);
void azsh_read_history(history_t h, FILE *f);

#endif

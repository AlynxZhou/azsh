#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "history.h"

struct history {
	char *list[HISTORY_SIZE];
	int begin;
	int end;
};

history_t azsh_create_history(void)
{
	struct history *h = malloc(sizeof(*h));
	for (int i = 0; i < HISTORY_SIZE; ++i)
		h->list[i] = NULL;
	h->begin = 0;
	h->end = 0;
	return h;
}

void azsh_delete_history(history_t h)
{
	for (int i = 0; i < HISTORY_SIZE; ++i)
		if (h->list[i] != NULL)
			free(h->list[i]);
	free(h);
}

void azsh_put_history(history_t h, const char *s)
{
	int size = strlen(s) + 1;
	h->begin = (h->begin + 1) % HISTORY_SIZE;
	if (h->begin == h->end) {
		if (h->list[h->begin] != NULL) {
			free(h->list[h->begin]);
			h->list[h->begin] = NULL;
		}
		h->end = (h->end + 1) % HISTORY_SIZE;
	}
	h->list[h->begin] = malloc(sizeof(*s) * size);
	strncpy(h->list[h->begin], s, size);
}

char *azsh_get_history(history_t h, int i)
{
	// i starts from 1, and 0 is always the command current running.
	int index = (h->begin - i) % HISTORY_SIZE;
	if (index < 0)
		index += HISTORY_SIZE;
	return h->list[index];
}

void azsh_write_history(history_t h, FILE *f)
{
	for (int i = (h->end + 1) % HISTORY_SIZE; i != h->begin;) {
		int seq = (h->begin - i) % HISTORY_SIZE;
		if (seq < 0)
			seq += HISTORY_SIZE;
		fprintf(f, "%d %s\n", seq, h->list[i]);
		i = (i + 1) % HISTORY_SIZE;
	}
}

void azsh_read_history(history_t h, FILE *f)
{
	// %d %s\n
	int size = LINE_SIZE_UNIT;
	int rest_len = size;
	char *line = malloc(sizeof(line) * size);
	char *start = line;
	do {
		char *res = fgets(start, rest_len, f);
		// EOF
		if (res == NULL) {
			free(line);
			break;
		}
		int len = strlen(line);
		if (line[len - 1] != '\n') {
			// Long string.
			size += LINE_SIZE_UNIT;
			line = realloc(line, sizeof(*line) * size);
			start = line + len;
			rest_len = size - len;
		} else {
			line[len - 1] = '\0';
			for (int i = 0; i < len - 1; ++i) {
				if (line[i] == ' ') {
					azsh_put_history(h, line + i + 1);
					break;
				}
			}
			free(line);
			line = malloc(sizeof(line) * size);
		}
	} while (true);
}

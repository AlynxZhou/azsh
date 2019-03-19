#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "azsh.h"

void error(const char *s);
int azsh_mainloop(void);
char *azsh_readline(char *prompt);
void azsh_rtrim(char *line);
char **azsh_parse_args(char **line_ptr);
void azsh_print_args(char **argv);
void azsh_run_command(char **argv);

char *HOME = NULL;

int main(int argc, char *argv[])
{
	HOME = getenv("HOME");
	printf("User home: %s\n", HOME);
	return azsh_mainloop();
}

int azsh_mainloop(void)
{
	char *line = NULL;
	while ((line = azsh_readline(DEFAULT_PROMPT)) != NULL) {
		azsh_rtrim(line);
		char **argv = azsh_parse_args(&line);
		if (argv != NULL) {
			// azsh_print_args(argv);
			azsh_run_command(argv);
			free(argv);
		}
		free(line);
	}
	return EXIT_SUCCESS;
}

char *azsh_readline(char *prompt)
{
	int size = LINE_SIZE_UNIT;
	int rest_len = size;
	char *line = malloc(sizeof(*line) * size);
	char *start = line;
	fputs(prompt, stdout);
	fflush(stdout);
	bool stop = true;
	do {
		stop = true;
		char *res = fgets(start, rest_len, stdin);
		// EOF
		if (res == NULL) {
			free(line);
			return NULL;
		}
		int len = strlen(line);
		if (line[len - 1] != '\n') {
			// Long string.
			stop = false;
			size += LINE_SIZE_UNIT;
			line = realloc(line, sizeof(*line) * size);
			start = line + len;
			rest_len = size - len;
		} else if (line[len - 2] == '\\' && line[len - 1] == '\n') {
			int escape_count = 0;
			for (int i = len - 2; i >= 0; --i) {
				if (line[i] != '\\')
					break;
				++escape_count;
			}
			if (escape_count % 2) {
				stop = false;
				// Escape.
				line[len - 2] = '\0';
				len = strlen(line);
				start = line + len;
				rest_len = size - len;
			}
		}
	} while (!stop);
	return line;
}

void azsh_rtrim(char *line)
{
	for (int len = strlen(line) - 1; len >= 0; --len) {
		if (isspace(line[len]))
			line[len] = '\0';
		else
			break;
	}
}

char **azsh_parse_args(char **line_ptr)
{
	char *line = *line_ptr;
	int size = ARGV_SIZE_UNIT;
	char **argv = malloc(sizeof(*argv) * size);
	int argv_len = 0;
	int line_len = strlen(line);
	// Replace env.
	for (int i = 0; i < line_len; ++i) {
		// To simplify we only support `${VARS}`.
		if (line[i] == '$' && i + 1 < line_len && line[i + 1] == '{') {
			int escape_count = 0;
			for (int j = i - 1; j >= 0; --j) {
				if (line[j] != '\\')
					break;
				++escape_count;
			}
			if (escape_count % 2)
				break;
			int start = i + 2;
			int stop = start;
			for (int j = start; j < line_len; ++j) {
				if (line[j] == '}') {
					stop = j;
					break;
				}
			}
			if (stop == start)
				continue;
			int key_size = stop - start + 1;
			char *key = malloc(sizeof(*key) * key_size);
			strncpy(key, line + start, key_size);
			key[key_size - 1] = '\0';
			char *value = getenv(key);
			int value_len = 0;
			if (value == NULL) {
				// `$`, `{`, `}`
				*line_ptr = malloc(
					sizeof(**line_ptr) *
					(line_len + 1 - (key_size - 1 + 3)));
				memcpy(*line_ptr, line, start - 2);
				// Line length + 1 for `\0`!
				memcpy((*line_ptr) + start - 2, line + stop + 1,
				       line_len + 1 - (stop + 1));
			} else {
				value_len = strlen(value);
				// `$`, `{`, `}`
				*line_ptr = malloc(sizeof(**line_ptr) *
						   (line_len + 1 -
						    (key_size - 1 + 3) +
						    value_len));
				memcpy(*line_ptr, line, start - 2);
				memcpy((*line_ptr) + start - 2, value,
				       value_len);
				memcpy((*line_ptr) + start - 2 + value_len,
				       line + stop + 1,
				       line_len + 1 - (stop + 1));
			}
			free(line);
			line = *line_ptr;
			line_len = strlen(line);
			// Must - 1 because it will be add 1 next loop.
			i = start - 2 + value_len - 1;
		}
	}
	bool leading_spaces = true;
	bool prev_space = true;
	bool in_double_quote = false;
	bool in_single_quote = false;
	bool prev_escape = false;
	for (int i = 0; i < line_len; ++i) {
		if (leading_spaces && isspace(line[i]))
			continue;
		else
			leading_spaces = false;
		if (prev_escape)
			prev_escape = false;
		if (line[i] == '\\' && i < line_len - 1 &&
		    !(in_single_quote || in_double_quote)) {
			prev_escape = true;
			switch (line[i + 1]) {
			case 'n':
				line[i + 1] = '\n';
				for (int j = i; j < line_len; ++j)
					line[j] = line[j + 1];
				--line_len;
				break;
			case 't':
				line[i + 1] = '\t';
				for (int j = i; j < line_len; ++j)
					line[j] = line[j + 1];
				--line_len;
				break;
			case '\\':
				line[i + 1] = '\\';
				for (int j = i; j < line_len; ++j)
					line[j] = line[j + 1];
				--line_len;
				break;
			case ' ':
				for (int j = i; j < line_len; ++j)
					line[j] = line[j + 1];
				--line_len;
				break;
			default:
				break;
			}
		}
		if (line[i] == '\'' && !prev_escape) {
			in_single_quote = !in_single_quote;
			for (int j = i; j < line_len; ++j)
				line[j] = line[j + 1];
			--line_len;
		}
		if (line[i] == '"' && !prev_escape) {
			in_double_quote = !in_double_quote;
			for (int j = i; j < line_len; ++j)
				line[j] = line[j + 1];
			--line_len;
		}
		if (isspace(line[i]) &&
		    !(prev_escape || in_double_quote || in_single_quote)) {
			if (!prev_space) {
				line[i] = '\0';
				prev_space = true;
			}
		} else {
			if (prev_space) {
				if (argv_len >= size - 1) {
					size += ARGV_SIZE_UNIT;
					argv = realloc(argv,
						       sizeof(*argv) * size);
				}
				argv[argv_len++] = line + i;
				prev_space = false;
			}
		}
	}
	if (argv_len == 0) {
		free(argv);
		return NULL;
	}
	argv[argv_len] = NULL;
	return argv;
}

void azsh_print_args(char **argv)
{
	putchar('|');
	for (int i = 0; argv[i] != NULL; ++i) {
		fputs(argv[i], stdout);
		putchar('|');
	}
	putchar('\n');
}

void azsh_run_command(char **argv)
{
	pid_t pid = fork();
	if (pid > 0)
		wait(NULL);
	else if (pid < 0)
		error("Fork Error\n");
	else
		exit(execvp(argv[0], argv));
}

void error(const char *s)
{
	perror(s);
	exit(EXIT_FAILURE);
}

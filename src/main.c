#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "azsh.h"

int azsh_mainloop(void);
char *azsh_readline(char *prompt);
void azsh_rtrim(char *line);
char **azsh_parse_args(char *line);
void azsh_run_command(char **argv);

int main(int argc, char *argv[])
{
	return azsh_mainloop();
}

int azsh_mainloop(void)
{
	char *line = NULL;
	while ((line = azsh_readline(DEFAULT_PROMPT)) != NULL) {
		azsh_rtrim(line);
		// fputs(line, stdout);
		char **argv = azsh_parse_args(line);
		if (argv != NULL) {
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
			stop = false;
			// Escape.
			line[len - 2] = '\0';
			len = strlen(line);
			start = line + len;
			rest_len = size - len;
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

char **azsh_parse_args(char *line)
{
	int size = ARGV_SIZE_UNIT;
	char **argv = malloc(sizeof(*argv) * size);
	int argv_len = 0;
	int line_len = strlen(line);
	bool leading_spaces = true;
	bool prev_space = true;
	for (int i = 0; i < line_len; ++i) {
		if (leading_spaces && isspace(line[i]))
			continue;
		else
			leading_spaces = false;
		if (isspace(line[i])) {
			if (!prev_space) {
				line[i] = '\0';
				prev_space = true;
			}
		} else {
			if (prev_space) {
				if (argv_len >= size - 1) {
					size += ARGV_SIZE_UNIT;
					argv = realloc(argv, sizeof(*argv) * size);
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

void azsh_run_command(char **argv)
{
	pid_t pid = fork();
	if (pid > 0)
		wait(NULL);
	else if (pid < 0)
		fputs("Fork Error\n", stdout);
	else
		execvp(argv[0], argv);
}

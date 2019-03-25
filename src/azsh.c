#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "history.h"
#include "azsh.h"

history_t history;

int azsh_mainloop(const char *HOME)
{
	history = azsh_create_history();
	FILE *fp;
	// HOME does not have ending `/`.
	char history_name[] = "/.azhistory";
	int HOME_LEN = strlen(HOME);
	int name_len = strlen(history_name);
	int size = HOME_LEN + name_len + 1;
	char *history_path = malloc(sizeof(*history_path) + size);
	strncpy(history_path, HOME, HOME_LEN);
	strncpy(history_path + HOME_LEN, history_name, name_len + 1);
	// printf("%s\n", history_path);
	if ((fp = fopen(history_path, "r")) != NULL) {
		azsh_read_history(history, fp);
	}
	char *line = NULL;
	while ((line = azsh_readline(DEFAULT_PROMPT)) != NULL) {
		azsh_rtrim(line);
		azsh_put_history(history, line);
		char **argv = azsh_parse_args(&line);
		if (argv != NULL) {
			// azsh_print_args(argv);
			azsh_run_command(argv);
			free(argv);
		}
		free(line);
	}
	if ((fp = fopen(history_path, "w")) != NULL) {
		azsh_write_history(history, fp);
	}
	free(history_path);
	azsh_delete_history(history);
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

void _azsh_replace_history(char **line_ptr)
{
	char *line = *line_ptr;
	int line_len = strlen(line);
	// Replace history.
	int iter_times = 0;
	for (int i = 0; i < line_len; ++i) {
		if (line[i] == '!') {
			int escape_count = 0;
			for (int j = i - 1; j >= 0; --j) {
				if (line[j] != '\\')
					break;
				++escape_count;
			}
			if (escape_count % 2)
				continue;
			// `!!` is the same as `!1`,
			// but `!1` is easier to parse.
			if (i + 1 < line_len && line[i + 1] == '!')
				line[i + 1] = '1';
			int digits_count = 0;
			for (int j = i + 1; j < line_len; ++j) {
				if (!isdigit(line[j]))
					break;
				++digits_count;
			}
			if (!digits_count)
				continue;
			char *buf = malloc(sizeof(*buf) * (digits_count + 1));
			// strncpy() automatically fill rest space with '\0'.
			strncpy(buf, line + i + 1, digits_count + 1);
			int num;
			sscanf(buf, "%d", &num);
			free(buf);
			if (num == 0)
				continue;
			int current_num = num;
			num += iter_times;
			int start = i;
			int stop = i + digits_count + 1;
			char *str = azsh_get_history(history, num);
			if (str == NULL)
				continue;
			int str_len = strlen(str);
			int size = line_len - (escape_count + 1) + str_len;
			*line_ptr = malloc(sizeof(*line) * size);
			strncpy(*line_ptr, line, start);
			strncpy(*line_ptr + start, str, str_len);
			strncpy(*line_ptr + start + str_len, line + stop,
				line_len - stop);
			(*line_ptr)[size - 1] = '\0';
			free(line);
			line = *line_ptr;
			line_len = strlen(line);
			// If we run `ls`, `ls !!`, `ls !!`, when the third
			// command is running, first it will be replaced with
			// `ls ls !!`, however, `!!` here should be `!2`,
			// so we add this each time.
			iter_times += current_num;
			// Must - 1 because it will be add 1 next loop.
			--i;
		}
	}
}

void _azsh_replace_environment(char **line_ptr)
{
	char *line = *line_ptr;
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
				continue;
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
				int size = line_len + 1 - (key_size - 1 + 3);
				// `$`, `{`, `}`
				*line_ptr = malloc(sizeof(**line_ptr) * size);
				strncpy(*line_ptr, line, start - 2);
				// Line length + 1 for `\0`!
				strncpy((*line_ptr) + start - 2,
					line + stop + 1, line_len - (stop + 1));
				(*line_ptr)[size - 1] = '\0';
			} else {
				value_len = strlen(value);
				int size = line_len + 1 - (key_size - 1 + 3) +
					   value_len;
				// `$`, `{`, `}`
				*line_ptr = malloc(sizeof(**line_ptr) * size);
				strncpy(*line_ptr, line, start - 2);
				strncpy((*line_ptr) + start - 2, value,
					value_len);
				strncpy((*line_ptr) + start - 2 + value_len,
					line + stop + 1, line_len - (stop + 1));
				(*line_ptr)[size - 1] = '\0';
			}
			free(line);
			line = *line_ptr;
			line_len = strlen(line);
			// Must - 1 because it will be add 1 next loop.
			i = start - 2 + value_len - 1;
		}
	}
}

char **azsh_parse_args(char **line_ptr)
{
	int size = ARGV_SIZE_UNIT;
	char **argv = malloc(sizeof(*argv) * size);
	int argv_len = 0;
	_azsh_replace_history(line_ptr);
	_azsh_replace_environment(line_ptr);
	char *line = *line_ptr;
	int line_len = strlen(line);
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
			case '$':
				for (int j = i; j < line_len; ++j)
					line[j] = line[j + 1];
				--line_len;
				break;
			case '!':
				for (int j = i; j < line_len; ++j)
					line[j] = line[j + 1];
				--line_len;
				break;
			case '\\':
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

int _azsh_cd(char **argv)
{
	if (argv[1] == NULL)
		error("Too few arguments.\n");
	else if (argv[2] != NULL)
		error("Too many arguments.\n");
	char *apath = realpath(argv[1], NULL);
	if (apath == NULL)
		error("Cannot get realpath");
	int res = chdir(apath);
	free(apath);
	return res;
}

int _azsh_pwd(char **argv)
{
	if (argv[1] != NULL)
		error("Too many arguments.\n");
	char *wd = get_current_dir_name();
	if (wd == NULL)
		error("Cannot get pwd.\n");
	printf("%s\n", wd);
	fflush(stdout);
	free(wd);
	return 0;
}

void azsh_run_command(char **argv)
{
	// cd should not be done in child process.
	if (!strcmp("cd", argv[0])) {
		_azsh_cd(argv);
		return;
	}
	pid_t pid = fork();
	if (pid > 0) {
		wait(NULL);
	} else if (pid < 0) {
		error("Fork Error\n");
	} else {
		if (!strcmp("pwd", argv[0])) {
			exit(_azsh_pwd(argv));
		} else if (!strcmp("history", argv[0])) {
			azsh_write_history(history, stdout);
			exit(EXIT_SUCCESS);
		} else {
			exit(execvp(argv[0], argv));
		}
	}
}

void error(const char *s)
{
	perror(s);
	exit(EXIT_FAILURE);
}

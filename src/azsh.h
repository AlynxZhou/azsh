#ifndef __AZSH_H__
#define __AZSH_H__

#define LINE_SIZE_UNIT 80
#define ARGV_SIZE_UNIT 10
#define DEFAULT_PROMPT "azsh> "
#define PATH_SEP '/'

void error(const char *s);
int azsh_mainloop(const char *HOME);
char *azsh_readline(char *prompt);
void azsh_rtrim(char *line);
char **azsh_parse_args(char **line_ptr);
void azsh_print_args(char **argv);
void azsh_run_command(char **argv);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "azsh.h"
#include "history.h"

char *HOME;

int main(int argc, char *argv[])
{
	HOME = getenv("HOME");
	// printf("User home: %s\n", HOME);
	return azsh_mainloop(HOME);
}

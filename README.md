azsh
====

Not a standard shell, but only a toy for operating system homework.
-------------------------------------------------------------------

# Usage

Make sure you are using GCC and GLIBC because some features request `_GNU_SOURCE`.

```
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./bin/azsh
```

# Features

- Basic fork and exec command.
- Support long line that using `\` before line break.
- Basic escape sequence support.
	Using `\ ` for space, `\\` for `\`, also support single quote and double quote, you can escape them with `\` too.
- Basic environment variable support.
	To simplify code, this shell only support format `${VAR}`, like `${HOME}`, does not support `$HOME` or `$(HOME)`.
- Basic history support.
	`history` to show history, `!!` for last command and `!no` for last no-th command.
- Basic internal command.
	Support `cd` and `pwd`.

# License

Apache-2.0

# Quartz

A highly portable and embeddable scripting language inspired by Lua and JavaScript.
It is written in C (C99 specifically).

Features:
- Co-operate multi-tasking.
- Dynamic typing.
- No dependency on `malloc` or `stdio.h` in the engine by default.
- Automatic memory management via tracing garbage collection.
- Very flexible and stable ABI, reducing the odds of breaking changes.
- Separate `int` (C `intptr_t`), `real` (C `double`) and `complex` (2 C `float`s) types.

It only depends on 5 headers:
- `stddef.h`, for NULL, size_t and ptrdiff_t.
- `stdint.h` for intptr_t.
- `stdbool.h` for `bool`, `true` and `false`.
- `math.h` for trig functions.
- `stdarg.h` for varargs.

When compiled with `-DQUARTZ_USE_LIBC`, it will use `stdlib.h`, `stdio.h` and
POSIX/Windows APIs as a default implementation.

# Compiling

Makefile is WIP

## Manually

The library can be compiled by compiling `src/one.c`. It `#include`s every other source file.
The interperter can be compiled by compiling `src/main.c`. It includes `one.c`, and defines `QUARTZ_USE_LIBC`, thus it does depend on `malloc` and `stdio.h`.
Since `math.h` is used, some compiles may need you to link in `libm` via `-lm`.

An example build command for the library is:
```sh
cc -shared -o libquartz.so -lm -O3 src/one.c
```

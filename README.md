# Quartz

A highly portable and embeddable scripting language inspired by Lua and JavaScript.
It is written in C (C99 specifically).

Features:
- Co-operate multi-tasking.
- Dynamic typing.
- `libc`-free option via a context system, allowing it to run on baremetal targets.
- Automatic memory management via tracing garbage collection.
- Error codes instead of setjmp/longjmp, for deterministic cleanup in C functions.
- Separate `int` (C `intptr_t`), `real` (C `double`) and `complex` (2 C `float`s) types.

It only depends on 5 headers:
- `stddef.h`, for NULL, size_t and ptrdiff_t.
- `stdint.h` for intptr_t.
- `stdbool.h` for `bool`, `true` and `false`.
- `math.h` for trig functions.
- `stdarg.h` for varargs.

When compiled without `-DQUARTZ_NO_LIBC`, it will use `stdlib.h`, `stdio.h` and
POSIX/Windows APIs as a default implementation.

Passing in `-DQUARTZ_NO_LIBC` should make it work on most baremetal machines, though the libm and double
precision floats may cause incompatibilities. In the future, an option to compile out real and complex numbers may also be added.

# Compiling

Makefile is WIP

## Manually

The library can be compiled by compiling `src/one.c`. It `#include`s every other source file.
The interperter can be compiled by compiling `src/main.c`. It includes `one.c`, unless `QUARTZ_EXT_LIB` is defined.
Since `math.h` is used, some compiles may need you to link in `libm` via `-lm`.

An example build command for the library is:
```sh
cc -shared -o libquartz.so -lm -O3 src/one.c
```

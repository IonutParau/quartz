# Quartz

A simple, minimal embeddable scripting language which does not depend on libc (but does need some standard headers and libm).

## Not depending on libc

Quartz uses a context system to allow controlling everything from file-system interaction to memory allocation.
This is to allow extensive sandboxing, but also means that libc need not be used, as you could change the context
to one which makes no use of libc. Compiling Quartz with `-DQUARTZ_NOLIBC` will get rid of the default context implementation,
replacing it with one that does nothing.

## The headers required

Even without libc, you will still need:
- `stdint.h`, for `intptr_t`.
- `math.h`, for trig functions. It is possible to compile out the math module which depends on this with `-DQUARTZ_NOLIBM`,
but this will leave the language incomplete.
- `stddef.h`, for `NULL` and `size_t`.
- `limits.h`, for a bunch of numerical limits to check for overflows.
- `stdbool.h`, for `bool`, `true` and `false`.
- `stdarg.h` for variable arguments.

With libc, you will also need `stdlib.h`, `stdio.h` and, for things such as directories, POSIX or Windows headers.

# The language

The language is somewhat inspired by Lua, C and Go.
See the `examples`.

## Features

- Dynamic typing
- Co-operative multitasking
- Separate int and double types
- Record types
- C API
- Exit codes instead of `setjmp`/`longjmp` in the C API

---

# Summary: Creating C Functions for Little Sheens

This document summarizes how to create C functions for the Little Sheens project, mirroring the style of the [`lib/`](https://github.com/stepherg/littlesheens/tree/master/lib) directory. These functions extend the Duktape JavaScript engine within the Sheens state machine framework.

## Overview
Little Sheens embeds JavaScript via Duktape, and C functions in `lib/` (e.g., `double.c`) provide native functionality callable from JavaScript. Each function file defines Duktape-compatible functions and registers them for dynamic or static linking.

## Steps to Create a C Function

### 1. Define the Function
Use the Duktape signature: `duk_ret_t (*)(duk_context *)`.
- Example: `double_number` doubles a number.
```c
static duk_ret_t double_number(duk_context *ctx) {
    if (duk_get_top(ctx) < 1) return duk_error(ctx, DUK_ERR_TYPE_ERROR, "double_number() needs 1 arg");
    double n = duk_require_number(ctx, 0);
    duk_push_number(ctx, n * 2);
    return 1;
}
```

### 2. Register the Function
Define `register_functions` returning a `DukFunctionRegistration` array:
```c
typedef struct {
    const char *name;
    duk_c_function func;
    int nargs;
} DukFunctionRegistration;

DukFunctionRegistration *register_functions(void) {
    DukFunctionRegistration *funcs = malloc(2 * sizeof(DukFunctionRegistration));
    funcs[0] = (DukFunctionRegistration){"double_number", double_number, 1};
    funcs[1] = (DukFunctionRegistration){NULL, NULL, 0};
    return funcs;
}
```

### 3. Example File: `lib/double.c`
```c
#include "duktape.h"
#include <stdlib.h>

typedef struct { const char *name; duk_c_function func; int nargs; } DukFunctionRegistration;

static duk_ret_t double_number(duk_context *ctx) {
    if (duk_get_top(ctx) < 1) return duk_error(ctx, DUK_ERR_TYPE_ERROR, "double_number() needs 1 arg");
    double n = duk_require_number(ctx, 0);
    duk_push_number(ctx, n * 2);
    return 1;
}

DukFunctionRegistration *register_functions(void) {
    DukFunctionRegistration *funcs = malloc(2 * sizeof(DukFunctionRegistration));
    funcs[0] = (DukFunctionRegistration){"double_number", double_number, 1};
    funcs[1] = (DukFunctionRegistration){NULL, NULL, 0};
    return funcs;
}
```

### 4. Compile
- **Shared Library**:
  ```bash
  gcc -fPIC -shared -Iduktape-2.7.0/src -o lib/double.so lib/double.c
  ```
- **Static Library**:
  ```bash
  gcc -c -Iduktape-2.7.0/src lib/double.c -o lib/double.o
  ar rcs lib/libdouble.a lib/double.o
  ```

### 5. Integration
- **Dynamic**: Loaded via `dlopen`/`dlsym` in the main program (e.g., `sheensio.c`).
- **Static**: Linked at compile time (e.g., `gcc -o sheensio sheensio.c -Llib -ldouble`).
- **JavaScript Usage**: `var result = double_number(5); // Returns 10`.

## Tips
- **Error Handling**: Use `duk_error` for clear JS-side errors.
- **Memory**: Allocate `funcs` with `malloc`; caller frees it.
- **Naming**: Use descriptive names (e.g., `double_number`).

## Building
- **Requirements**: `gcc`, `wget`, Duktape headers.
- **Commands**:
  ```bash
  # Shared
  gcc -fPIC -shared -Iduktape-2.7.0/src -o lib/double.so lib/double.c
  # Static
  gcc -c -Iduktape-2.7.0/src lib/double.c -o lib/double.o
  ar rcs lib/libdouble.a lib/double.o
  ```

## Notes
- Add new functions to `lib/` and update the build system as needed.
- Test with a simple Duktape program before integrating.
- Follow Little Sheens’ licensing for contributions.

--- 

This summary captures the core process for creating and integrating C functions into Little Sheens, focusing on clarity and brevity. Let me know if you’d like to tweak it further!
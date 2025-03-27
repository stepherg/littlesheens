---

# Creating C Functions for Little Sheens

This guide explains how to create C functions for the Little Sheens project, similar to those found in the [`lib/`](https://github.com/stepherg/littlesheens/tree/master/lib) directory. These functions are designed to be registered with the Duktape JavaScript engine and integrated into the Sheens state machine framework.

## Overview

Little Sheens uses Duktape to embed JavaScript within a C-based state machine system. The C functions in `lib/` (e.g., `double.c`, `match.c`) extend the JavaScript environment by providing native implementations callable from JavaScript actions or scripts. Each C file:
- Defines one or more Duktape-compatible functions.
- Registers them via a `register_functions` entry point.
- Is compiled into a shared library (`.so`) for dynamic linking.

## Step-by-Step Guide

### 1. Define the C Function
Each function must follow the Duktape C function signature: `duk_ret_t (*)(duk_context *)`.
- **`duk_context *ctx`**: The Duktape context, used to interact with the JavaScript stack.
- **Return Value**: 
  - `0`: No value returned (void).
  - `1`: One value pushed onto the stack.
  - `DUK_RET_*`: Error codes (e.g., `DUK_RET_TYPE_ERROR`).

#### Example: `double_number`
```c
duk_ret_t double_number(duk_context *ctx) {
    // Check argument count
    if (duk_get_top(ctx) < 1) {
        return duk_error(ctx, DUK_ERR_TYPE_ERROR, "double_number() requires 1 argument");
    }

    // Get the number from the stack (index 0)
    double n = duk_require_number(ctx, 0);

    // Push the doubled value back onto the stack
    duk_push_number(ctx, n * 2);
    return 1; // Return 1 value
}
```

### 2. Create a Registration Function
Define a `register_functions` function that returns an array of `DukFunctionRegistration` structures, listing all functions to expose to JavaScript.
- **Structure**: 
  ```c
  typedef struct {
      const char *name;     // JavaScript function name
      duk_c_function func;  // C function pointer
      int nargs;            // Number of arguments (-1 for variable)
  } DukFunctionRegistration;
  ```
- **Sentinel**: End the array with `{NULL, NULL, 0}`.

#### Example: Registration
```c
DukFunctionRegistration *register_functions(void) {
    DukFunctionRegistration *funcs = malloc(2 * sizeof(DukFunctionRegistration));
    funcs[0] = (DukFunctionRegistration){"double_number", double_number, 1};
    funcs[1] = (DukFunctionRegistration){NULL, NULL, 0}; // Sentinel
    return funcs;
}
```

### 3. Write the Full C File
Combine the function and registration into a single `.c` file in `lib/`.

#### `lib/double.c` (Full Example)
```c
#include <duktape.h>
#include <register.h>
#include <stdlib.h>

typedef struct {
    const char *name;
    duk_c_function func;
    int nargs;
} DukFunctionRegistration;

duk_ret_t double_number(duk_context *ctx) {
    if (duk_get_top(ctx) < 1) {
        return duk_error(ctx, DUK_ERR_TYPE_ERROR, "double_number() requires 1 argument");
    }
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


#### Example Usage in JavaScript
In a Sheens spec or script:
```javascript
var result = double_number(5); // Calls the C function, returns 10
print("Doubled value: " + result);
```

## Tips and Best Practices

- **Error Handling**: Use `duk_error` to report argument errors or runtime issues, improving JavaScript-side debugging.
- **Memory Management**: Allocate memory for `DukFunctionRegistration` with `malloc`; the caller (e.g., `machines.c`) should free it.
- **Naming**: Match function names to their purpose (e.g., `double_number`, `match_string`) for clarity in JavaScript.
- **Dependencies**: Minimize external dependencies beyond Duktape to keep functions lightweight.
- **Testing**: Test each function in isolation with a simple Duktape program before integrating with Sheens.

## Building and Running

The default target in the Makefile will automatically compile the `.c` files into a shared libraries (`.so`).

```bash
[littlesheens]$ make
```

---
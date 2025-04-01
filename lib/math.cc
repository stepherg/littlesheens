#include <duktape.h>
#include <register.h>
#include <stdlib.h>

//
// C function to add two numbers
//
duk_ret_t native_add(duk_context *ctx) {
   if (duk_get_top(ctx) != 2) {
      return duk_error(ctx, DUK_ERR_TYPE_ERROR, "add() requires 2 arguments");
   }
   double a = duk_require_number(ctx, 0);
   double b = duk_require_number(ctx, 1);
   duk_push_number(ctx, a + b);
   return 1;
}

//
// C function to multiply two numbers
//
duk_ret_t native_multiply(duk_context *ctx) {
   if (duk_get_top(ctx) != 2) {
      return duk_error(ctx, DUK_ERR_TYPE_ERROR, "add() requires 2 arguments");
   }
   double a = duk_require_number(ctx, 0);
   double b = duk_require_number(ctx, 1);
   duk_push_number(ctx, a * b);
   return 1;
}

//
// Registration function
//
DukFunctionRegistration *register_functions(void) {
   DukFunctionRegistration *funcs = malloc(3 * sizeof(DukFunctionRegistration));
   funcs[0] = (DukFunctionRegistration){ "add", native_add, 2 };
   funcs[1] = (DukFunctionRegistration){ "multiply", native_multiply, 2 };
   funcs[2] = (DukFunctionRegistration){ NULL, NULL, 0 };
   return funcs;
}

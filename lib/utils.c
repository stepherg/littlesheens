#include <duktape.h>
#include <register.h>
#include <ctype.h>
#include <string.h>

//
// C function to convert string to uppercase
//
duk_ret_t native_uppercase(duk_context *ctx) {
   if (duk_get_top(ctx) != 1) {
      return duk_error(ctx, DUK_ERR_TYPE_ERROR, "uppercase() requires 1 argument");
   }

   const char *str = duk_require_string(ctx, 0);
   size_t slen = strlen(str);
   char *upper = malloc(slen + 1);
   upper[slen] = '\0';
   for (int i = 0; i < slen; i++) {
      upper[i] = toupper(str[i]);
   }
   duk_push_string(ctx, upper);
   free(upper);
   return 1;
}

//
// Registration function
//
DukFunctionRegistration *register_functions(void) {
   DukFunctionRegistration *funcs = malloc(2 * sizeof(DukFunctionRegistration));
   funcs[0] = (DukFunctionRegistration){ "uppercase", native_uppercase, 1 };
   funcs[1] = (DukFunctionRegistration){ NULL, NULL, 0 };
   return funcs;
}

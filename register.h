#ifndef __DUK_REGISTER_H__
#define __DUK_REGISTER_H__

typedef struct {
   const char *name;
   duk_c_function func;
   int nargs;
} DukFunctionRegistration;

// Type for the registration function each module must provide
typedef DukFunctionRegistration *(*RegisterFunc)(void);

#endif

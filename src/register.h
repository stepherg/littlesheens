#ifndef __DUK_REGISTER_H__
#define __DUK_REGISTER_H__

// Type for the registration function each module must provide
typedef duk_idx_t(*RegisterFunc)(duk_context *ctx);
typedef void (*CloseFunc)(duk_context *ctx);

#endif

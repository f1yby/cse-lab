// safe assertions.

#ifndef verify_client_h
#define verify_client_h

#include <assert.h>
#include <stdlib.h>

#ifdef NDEBUG
#define VERIFY(expr)      \
  do {                    \
    if (!(expr)) abort(); \
  } while (0)
#else
#define VERIFY(expr) assert(expr)
#endif

#endif

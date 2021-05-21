/* LzmaLib.h -- LZMA library interface
2009-04-07 : Igor Pavlov : Public domain */

#ifndef __LZMA_LIB_H
#define __LZMA_LIB_H

#undef _WIN32

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MY_STDAPI int MY_STD_CALL

#define LZMA_PROPS_SIZE 5


MY_STDAPI LzmaUncompress(unsigned char *dest, size_t *destLen, const unsigned char *src, SizeT *srcLen,
  const unsigned char *props, size_t propsSize);

#ifdef __cplusplus
}
#endif

#endif

#ifndef __SHA_H__
#define __SHA_H__

#include <gmodule.h>

#if (SIZEOF_INT == 4)
typedef unsigned int uint32;
#elif (SIZEOF_SHORT == 4)
typedef unsigned short uint32;
#else
typedef unsigned int uint32;
#endif /* HAVEUINT32 */
 
G_MODULE_EXPORT int strprintsha(char *dest, int *hashval);
 
typedef struct {
  uint32 H[5];
  uint32 W[80];
  int lenW;
  uint32 sizeHi,sizeLo;
} SHA_CTX;
 
G_MODULE_EXPORT void shaInit(SHA_CTX *ctx);
G_MODULE_EXPORT void shaUpdate(SHA_CTX *ctx, unsigned char *dataIn, int len);
G_MODULE_EXPORT void shaFinal(SHA_CTX *ctx, unsigned char hashout[20]);
G_MODULE_EXPORT void shaBlock(unsigned char *dataIn, int len, unsigned char hashout[20]);
G_MODULE_EXPORT char *shahash(char *str);

#endif

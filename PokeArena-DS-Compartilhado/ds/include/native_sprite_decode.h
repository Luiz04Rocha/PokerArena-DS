#ifndef NATIVE_SPRITE_DECODE_H
#define NATIVE_SPRITE_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Native 64 x 64, four pixels per word. The high bit marks a transparent run. */
static inline bool nativeDecodeSprite(const uint16_t *src,const uint16_t *end,uint16_t *dest) {
    int remaining=1024;
    while(src<end && remaining) {
        unsigned token=*src++;
        int count=token&0x7fff;
        if(!count || count>remaining) break;
        if(token&0x8000) memset(dest,0,count*sizeof(*dest));
        else {
            if(end-src<count) break;
            memcpy(dest,src,count*sizeof(*dest));
            src+=count;
        }
        dest+=count;
        remaining-=count;
    }
    if(remaining) memset(dest,0,remaining*sizeof(*dest));
    return remaining==0;
}

#endif

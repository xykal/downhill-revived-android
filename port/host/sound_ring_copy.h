/* Interleaved stereo ring copy. Source must not alias the ring. */
#ifndef DHD_SOUND_RING_COPY_H
#define DHD_SOUND_RING_COPY_H
#include <stdint.h>
#include <string.h>
static void sound_ring_copy(int16_t *ring,long capacity,long write,const int16_t *stereo,long count)
{
    if(count<=0)return;
    long at=write%capacity;
    while(count>0) {
        long n=count<capacity-at?count:capacity-at;
        memcpy(ring+at*2,stereo,(size_t)n*2*sizeof *stereo);
        stereo+=n*2;count-=n;at=0;
    }
}
#endif

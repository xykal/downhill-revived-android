#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound_ring_copy.h"
enum {CAP=24000};
static int16_t reference[CAP*2],actual[CAP*2],src[CAP*2];
int main(void){
    uint32_t rng=47891;
    for(int k=0;k<1000;k++){
        rng=rng*1664525u+1013904223u;long start=rng%(CAP*10);
        rng=rng*1664525u+1013904223u;long n=rng%(CAP+1);
        for(int i=0;i<CAP*2;i++){rng=rng*1664525u+1013904223u;reference[i]=actual[i]=(int16_t)(rng>>16);rng=rng*1664525u+1013904223u;src[i]=(int16_t)(rng>>16);}
        for(long i=0;i<n;i++){reference[((start+i)%CAP)*2]=src[i*2];reference[((start+i)%CAP)*2+1]=src[i*2+1];}
        sound_ring_copy(actual,CAP,start,src,n);
        if(memcmp(reference,actual,sizeof actual)){fprintf(stderr,"mismatch start=%ld n=%ld\n",start,n);return 1;}
    }
    puts("PASS: stereo ring bulk copy matches reference in 1000 wrap/length fixtures");return 0;
}

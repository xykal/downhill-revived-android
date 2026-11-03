/* Compare the cached correlation search to the former ring-reading implementation. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#define ST_HOP 480
#define ST_SEEK 240
#define RING_SAMPLES 24000
static int16_t ring[RING_SAMPLES * 2];
static float st_mono(long at) { const int16_t *f=&ring[(at % RING_SAMPLES)*2]; return (float)f[0]+f[1]; }
#include "sound_wsola_seek.h"
static long original(long lo,long hi,long ref) {
    static float want[ST_HOP/2]; long best=lo,at; double best_score=-1e300;
    for(int i=0;i<ST_HOP/2;i++)want[i]=st_mono(ref+2*i);
    for(int pass=0;pass<2;pass++){
        long from=pass?best-1:lo,to=pass?best+1:hi,step=pass?1:2;
        if(from<lo)from=lo;
        if(to>hi)to=hi;
        for(at=from;at<=to;at+=step){double dot=0,energy=1;
            for(int i=0;i<ST_HOP/2;i++){float x=st_mono(at+2*i);dot+=x*want[i];energy+=x*x;}
            double score=dot/sqrt(energy);if(score>best_score){best_score=score;best=at;}}
    }return best;
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
    uint32_t rnd=918273; volatile long checksum=0;
    /* Random, periodic, silence, extrema; include ring wrap and short candidate windows. */
    for(int pattern=0;pattern<4;pattern++){
        for(int i=0;i<RING_SAMPLES*2;i++){
            rnd=rnd*1664525u+1013904223u;
            ring[i]=pattern==0?(int16_t)(rnd>>16):pattern==1?(int16_t)(10000*sin(i*.025)):pattern==2?0:(i&1?32767:-32768);
        }
        for(int k=0;k<40;k++){
            rnd=rnd*1664525u+1013904223u;long lo=1000+(rnd%23500),ref=500+(rnd%22000);
            long hi=lo+(k*13)%(2*ST_SEEK+1);
            long a=original(lo,hi,ref),b=st_seek(lo,hi,ref);
            if(a!=b){fprintf(stderr,"mismatch pattern=%d lo=%ld hi=%ld ref=%ld old=%ld new=%ld\n",pattern,lo,hi,ref,a,b);return 1;}
        }
    }
    for(int i=0;i<RING_SAMPLES*2;i++){rnd=rnd*1664525u+1013904223u;ring[i]=(int16_t)(rnd>>16);}
    double a=now();for(int i=0;i<120;i++)checksum+=original(1000,1480,2000+i);double b=now();
    for(int i=0;i<120;i++)checksum+=st_seek(1000,1480,2000+i);
    double c=now();
    printf("PASS wsola exact search: 160 fixtures; host synthetic old %.4fs cached %.4fs checksum %ld\n",b-a,c-b,checksum);
    return 0;
}

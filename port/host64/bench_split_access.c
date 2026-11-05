/* Synthetic guard+load microbenchmark; not the game and not a phone FPS test.
   Compile twice, once with this revision's memory_access.h and once with the
   prior header, e.g. by placing each header and its two dependencies in a
   separate include directory. Run each with modes 0..3. */
#define DHD_EXPERIMENTAL_SPLIT_MEMORY 1
#include "memory_access.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#define N 16384
#define REPS 1800
static uint32_t b[N],mem[65536];
static volatile uint32_t sink;
__attribute__((noinline)) static uint32_t slow(uint32_t a) { return mem[((a ^ 0x10000)>>2)&65535]; }
static inline uint32_t load0(uint32_t x) {return host64_base_ok(x,0) ? mem[(x>>2)&65535] : slow(x);}
static inline uint32_t load4(uint32_t x) {return host64_base_ok(x,4) ? mem[((x+4)>>2)&65535] : slow(x+4);}
static inline uint32_t loadbig(uint32_t x) {return host64_base_ok(x,0x70000000) ? mem[(x>>2)&65535] : slow(x);}
int main(int argc,char **argv) {
 unsigned mode=argc>1?atoi(argv[1]):0; uint32_t r=123456789;
 for (unsigned i=0;i<N;i++){r=1664525*r+1013904223; switch(mode){
 case 0:b[i]=0x08000000|(r&0xffffc); break;
 case 1:b[i]= (i%10==0?0:0x08000000)|(r&0xffffc);break;
 case 2:b[i]=(i%3==0?0:(i%3==1?0x08000000:0x70000000))|(r&0xffffc);break;
 default:b[i]=r;break;} }
 for(unsigned i=0;i<65536;i++)mem[i]=i*7+31;
 struct timespec a,z; clock_gettime(CLOCK_MONOTONIC,&a); uint32_t acc=0;
 for(unsigned rep=0;rep<REPS;rep++) for(unsigned i=0;i<N;i++){uint32_t x=b[i];acc+=load0(x)+load4(x); if ((i&15)==0)acc+=loadbig(x);}
 clock_gettime(CLOCK_MONOTONIC,&z);sink=acc;double t=(z.tv_sec-a.tv_sec)+(z.tv_nsec-a.tv_nsec)/1e9;
 printf("mode=%u sec=%.6f check=%u\n",mode,t,acc);return 0;
}

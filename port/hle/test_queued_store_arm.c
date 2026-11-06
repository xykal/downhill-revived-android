/* Synthetic integration test of real graphics.c + graphics_queue.c + GS; no GPU/game data.
 * Built for 32-bit ARM Linux, run under QEMU. VIF DIRECT is modeled, not production VIF. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "hle.h"
#include "gs_queue.h"
#include "../gs/gs.h"
#include "../runtime/memory.h"
#include "../host/host_api.h"
volatile uint32_t gs_presented_frames;
double vif_gs_seconds;
uint64_t kernel_vsync_count;
uintptr_t guest_base;
static int (*handler_fn)(uint32_t,uint32_t);
static int faults, callbacks, page=4096;
void hle_log(const char *fmt,...){va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);}
void kernel_poll(void) {}
void host_thread_name(const char *n){(void)n;}
double host_current_thread_cpu(void){return 0;}
double host_wall_seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int host_callback_fault_guard(uint32_t f,uint32_t ctx){(void)ctx;handler_fn=(void*)(uintptr_t)f;return 1;}
int host_fault_guard_here(void){return 1;}
int host_mem_protect(uint32_t at,uint32_t size,int protection){
  uintptr_t first=at & ~(uintptr_t)(page-1),end=(at+size+page-1)&~(uintptr_t)(page-1);
  return mprotect((void *)first,end-first,protection==HOST_MEM_NOACCESS ? PROT_NONE : PROT_READ|PROT_WRITE)==0;
}
static void fault(int sig,siginfo_t *info,void *ctx){(void)sig;(void)ctx;faults++; if(!handler_fn || !handler_fn(0,(uint32_t)(uintptr_t)info->si_addr)) _exit(90);}
/* Minimal DIRECT execution: the store packet is one 4-word VIF header and 24 GIF words. */
void vif_feed(int unit,const uint32_t *words,uint32_t count){
 (void)unit;if(count==28 && words[3]==0x50000006) {callbacks++;graphics_gif_feed(2,words+4,24);}
 else if(count==1 && (words[0]==0 || words[0]==0x06000000)) {} else {fprintf(stderr,"unexpected VIF packet n=%u\n",count);abort();}
}
void host_service_vif_feed(int unit,uint32_t words,uint32_t count){vif_feed(unit,(void *)(uintptr_t)words,count);}
extern int sceGsExecStoreImage(uint32_t,uint32_t) RETAIL(func_00244228);
extern void graphics_settle(void);
extern void graphics_shutdown(void);
extern int graphics_threaded(void);
extern uint32_t host_service_gs_store_image(uint32_t,uint32_t,uint32_t);
static void *mapview(int fd,uint32_t at,uint32_t offset,uint32_t size){
 void *p=mmap((void*)(uintptr_t)at,size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_FIXED,fd,offset);
 if(p!=(void*)(uintptr_t)at){perror("mmap");exit(2);}return p;
}
static void setup(void){
 int fd=memfd_create("guest-ram",0);if(fd<0 || ftruncate(fd,PS2_RAM_SIZE)){perror("memfd");exit(2);}
 for(unsigned m=0;m<3;m++){uint32_t mirror=m==1?0x20000000:m==2?0x30000000:0;
  mapview(fd,mirror|PS2_RAM_BASE,0,PS2_RAM_SIZE);
  /* This fixture uses offset 0x10000, below the low mirror threshold. */
 }
 close(fd);void *csr=mmap((void*)0x12000000,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
 if(csr!=(void*)0x12000000){perror("csr");exit(2);}
 struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);sigaction(SIGSEGV,&sa,0);
 gs_reset(&gs);
}
static const uint64_t templ[14]={UINT64_C(0x06008000)<<32,UINT64_C(0x50000006)<<32|0x13000000,
 UINT64_C(1)<<60|0x8005,14,UINT64_C(48)<<24|UINT64_C(8)<<16,GS_BITBLTBUF,
 13|UINT64_C(27)<<16,GS_TRXPOS,4|UINT64_C(4)<<32,GS_TRXREG,0,GS_FINISH,1,GS_TRXDIR};
int main(int argc,char **argv){
 page=argc>1?atoi(argv[1]):4096;if(page!=4096&&page!=16384)return 2;
 setup();uint32_t zero=0;graphics_feed(1,(uint32_t)(uintptr_t)&zero,1);graphics_wait();
 for(unsigned i=0;i<16;i++)gs_write_pixel(&gs,GS_PSMZ32,0,8,13+i%4,27+i/4,i*0x1234567u);
 for(unsigned trial=0;trial<100;trial++){
  /* packet and target share simulated 4K or 16K page: regression for d5073e5 */
  uint64_t *packet=(void*)(uintptr_t)0x08010100; volatile uint32_t *dst=(void*)(uintptr_t)0x08010800;
  memcpy(packet,templ,sizeof templ);for(unsigned i=0;i<17;i++)dst[i]=0xDEADBEEF;
  if(sceGsExecStoreImage((uint32_t)(uintptr_t)packet,(uint32_t)(uintptr_t)dst))return 3;
  for(unsigned i=0;i<16;i++)if(dst[i]!=i*0x1234567u){fprintf(stderr,"DIFF trial=%u i=%u value=%08x\n",trial,i,dst[i]);return 4;}
  if(dst[16]!=0xDEADBEEF)return 5;
 }
 /* Fill all sixteen queue slots, trigger the full-slot fallback, and read through RAM mirrors. */
 for(unsigned j=0;j<17;j++) {
  uint64_t *pkt=(void*)(uintptr_t)(0x08020000+j*0x4000);
  volatile uint32_t *dst=(void*)(uintptr_t)(0x08080000+j*0x4000);
  memcpy(pkt,templ,sizeof templ);for(unsigned i=0;i<17;i++)dst[i]=0xDEADBEEF;
 }
 for(unsigned j=0;j<17;j++) {
  uint32_t pkt=0x08020000+j*0x4000, dst=0x08080000+j*0x4000;
  if(sceGsExecStoreImage(pkt,dst))return 6;
 }
 for(unsigned j=0;j<17;j++) {
  uint32_t mirror=j%3==1?0x20000000:j%3==2?0x30000000:0;
  volatile uint32_t *dst=(void*)(uintptr_t)(mirror | (0x08080000+j*0x4000));
  for(unsigned i=0;i<16;i++) if(dst[i]!=i*0x1234567u){fprintf(stderr,"slots DIFF j=%u i=%u value=%08x\n",j,i,dst[i]);return 7;}
  if(dst[16]!=0xDEADBEEF)return 8;
 }
 graphics_wait();
 printf("MATCH: 100 overlapping and 17 batched store-image reads, mirrors; simulated-page=%d faults=%d packets=%d GS-worker=%s strict=%s\n",page,faults,callbacks,getenv("DHD_GS_WORKER"),getenv("DHD_SYNC_STRICT"));
 graphics_shutdown();return 0;
}

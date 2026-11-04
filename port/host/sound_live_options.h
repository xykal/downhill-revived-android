/* Lock-free requests from Android UI to the single SDL audio callback thread. */
#ifndef DHD_SOUND_LIVE_OPTIONS_H
#define DHD_SOUND_LIVE_OPTIONS_H
#include <stdatomic.h>
typedef struct {
    atomic_int stretch_pending, latency_pending;
    atomic_int stretch_applied, latency_applied;
} SoundLiveOptions;
#define SOUND_LIVE_OPTIONS_INIT {ATOMIC_VAR_INIT(-1),ATOMIC_VAR_INIT(-1),ATOMIC_VAR_INIT(-1),ATOMIC_VAR_INIT(-1)}
static void sound_live_request(SoundLiveOptions *s,int option,int value)
{
    if(option==8 && value>=0 && value<=1)
        atomic_store_explicit(&s->stretch_pending,value,memory_order_release);
    if(option==10 && value>=0 && value<=2) {
        static const int ms[]={70,120,200};
        atomic_store_explicit(&s->latency_pending,ms[value],memory_order_release);
    }
}
/* Only called by the audio callback: use existing transitions in st_fill and cushion logic. */
static void sound_live_apply(SoundLiveOptions *s,int *stretch,unsigned *target,unsigned per_ms)
{
    int value=atomic_exchange_explicit(&s->stretch_pending,-1,memory_order_acq_rel);
    if(value>=0){*stretch=value;atomic_store_explicit(&s->stretch_applied,value,memory_order_release);}
    value=atomic_exchange_explicit(&s->latency_pending,-1,memory_order_acq_rel);
    if(value>=0){*target=(unsigned)value*per_ms;atomic_store_explicit(&s->latency_applied,value,memory_order_release);}
}
static int sound_live_running(SoundLiveOptions *s,int option)
{
    if(option==8)return atomic_load_explicit(&s->stretch_applied,memory_order_acquire);
    if(option==10)return atomic_load_explicit(&s->latency_applied,memory_order_acquire);
    return -1;
}
#endif

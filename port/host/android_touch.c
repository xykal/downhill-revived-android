#include "android_touch.h"
#include <stdatomic.h>
#ifdef __ANDROID__
#include <jni.h>
#endif

/* UI and game thread exchange one coherent snapshot. Critical section only copies six words. */
typedef struct { unsigned buttons, active; int lx,ly,rx,ry; } TouchState;
static atomic_flag guard = ATOMIC_FLAG_INIT;
static TouchState touch = {0,0,128,128,128,128};
static void lock(void) { while (atomic_flag_test_and_set_explicit(&guard,memory_order_acquire)) {} }
static void unlock(void) { atomic_flag_clear_explicit(&guard,memory_order_release); }
static int byte(int n) { return n<0 ? 0 : n>255 ? 255 : n; }
void android_touch_set(unsigned buttons, int lx, int ly, int rx, int ry, unsigned active)
{
    TouchState s={buttons & 0xffffu,active & 3u,byte(lx),byte(ly),byte(rx),byte(ry)};
    lock(); touch=s; unlock();
}
void android_touch_merge(HostPad *out)
{
    static const uint16_t order[12]={PAD_RIGHT,PAD_LEFT,PAD_UP,PAD_DOWN,PAD_TRIANGLE,PAD_CIRCLE,
                                      PAD_CROSS,PAD_SQUARE,PAD_L1,PAD_R1,PAD_L2,PAD_R2};
    TouchState s;
    lock(); s=touch; unlock();
    out->buttons |= (uint16_t)s.buttons;
    if (s.active&1) out->left_x=s.lx, out->left_y=s.ly;
    if (s.active&2) out->right_x=s.rx, out->right_y=s.ry;
    for (unsigned i=0;i<12;i++) if (s.buttons&order[i]) out->pressure[i]=255;
}
/* Whitelist UI settings; never allow the Java UI to overwrite the saved ISO URI. */
int android_touch_option(int option, int value, const char **key, const char **setting)
{
    static const char *const booleans[]={"0","1"};
    static const char *const shapes[]={"stretch","wide","4:3"};
    static const char *const filters[]={"linear","point"};
    if (!key || !setting) return 0;
    switch (option) {
    case 1: if (value<1 || value>3) return 0; *key="scale"; *setting=value==1 ? "1" : value==2 ? "2" : "window"; return 1;
    case 2: if (value<0 || value>1) return 0; *key="perf_counter"; *setting=booleans[value]; return 1;
    case 3: if (value<0 || value>2) return 0; *key="DHD_ASPECT"; *setting=shapes[value]; return 1;
    case 4: if (value<0 || value>1) return 0; *key="widescreen"; *setting=booleans[value]; return 1;
    case 5: if (value<0 || value>1) return 0; *key="DHD_PRESENT_FILTER"; *setting=filters[value]; return 1;
    case 6: if (value<0 || value>1) return 0; *key="skip_movies"; *setting=booleans[value]; return 1;
    case 7: if (value<0 || value>1) return 0; *key="vsync"; *setting=booleans[value]; return 1;
    case 8: if (value<0 || value>1) return 0; *key="DHD_SND_STRETCH"; *setting=booleans[value]; return 1;
    case 9: if (value<0 || value>1) return 0; *key="fps"; *setting=value ? "monitor" : "60"; return 1;
    case 10: {
        static const char *const buffers[]={"70","120","200"};
        if (value<0 || value>2) return 0;
        *key="sound_latency_ms"; *setting=buffers[value]; return 1;
    }
    case 11: if (value<0 || value>1) return 0; *key="DHD_TIMELINE"; *setting=value ? "1" : ""; return 1;
    case 12: {
        static const char *const spins[]={"3000","1000","0"};
        if (value<0 || value>2) return 0;
        *key="DHD_SPIN_US"; *setting=spins[value]; return 1;
    }
    case 13: if (value<0 || value>1) return 0; *key="DHD_RSTAT"; *setting=value ? "1" : ""; return 1;
    case 14: if (value<0 || value>1) return 0; *key="DHD_SND_HOST_TRACE"; *setting=value ? "1" : ""; return 1;
    default: return 0;
    }
}
/* UI thread publishes live choices only after the ini save; game thread consumes them. */
static atomic_int live_counter=ATOMIC_VAR_INIT(-1), live_shape=ATOMIC_VAR_INIT(-1), live_filter=ATOMIC_VAR_INIT(-1);
void android_touch_queue_live(int option, int value)
{
    if (option==2 && value>=0 && value<=1) atomic_store_explicit(&live_counter,value,memory_order_release);
    if (option==3 && value>=0 && value<=2) atomic_store_explicit(&live_shape,value,memory_order_release);
    if (option==5 && value>=0 && value<=1) atomic_store_explicit(&live_filter,value,memory_order_release);
}
void android_touch_take_live(int *counter,int *shape,int *filter)
{
    *counter=atomic_exchange_explicit(&live_counter,-1,memory_order_acq_rel);
    *shape=atomic_exchange_explicit(&live_shape,-1,memory_order_acq_rel);
    *filter=atomic_exchange_explicit(&live_filter,-1,memory_order_acq_rel);
}
#ifdef __ANDROID__
extern int android_settings_ready(void);
JNIEXPORT void JNICALL Java_app_downhillrevived_DhdActivity_nativeTouchState(
    JNIEnv *env, jclass cls, jint buttons, jint lx, jint ly, jint rx, jint ry, jint active)
{
    (void)env;(void)cls;android_touch_set((unsigned)buttons,lx,ly,rx,ry,(unsigned)active);
}
JNIEXPORT jboolean JNICALL Java_app_downhillrevived_DhdActivity_nativeSaveOption(
    JNIEnv *env, jclass cls, jint option, jint value)
{
    (void)env;(void)cls;
    const char *key, *setting;
    if (!android_settings_ready() || !android_touch_option(option,value,&key,&setting)) return JNI_FALSE;
    if (!host_setting_save(key,setting)) return JNI_FALSE;
    android_touch_queue_live(option,value);
    return JNI_TRUE;
}
#endif

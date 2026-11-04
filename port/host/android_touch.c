#include "android_touch.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
/* Immutable startup environment captured on the game thread. The three live options are
   atomic indices updated only after gs_present_options_apply has actually returned. */
static const char *const option_env[15]={NULL,"DHD_SCALE","DHD_PERF_COUNTER","DHD_ASPECT","DHD_WIDESCREEN",
    "DHD_PRESENT_FILTER","DHD_SKIP_MOVIES","DHD_VSYNC","DHD_SND_STRETCH","DHD_FPS","DHD_SND_LATENCY_MS",
    "DHD_TIMELINE","DHD_SPIN_US","DHD_RSTAT","DHD_SND_HOST_TRACE"};
static const char *const option_default[15]={NULL,"1","0","4:3","0","linear","0","0","1","60","70","0","3000","0","0"};
static char startup[15][48];
static atomic_int loaded=ATOMIC_VAR_INIT(0);
static atomic_int active_counter=ATOMIC_VAR_INIT(-1), active_shape=ATOMIC_VAR_INIT(-1), active_filter=ATOMIC_VAR_INIT(-1);
void android_touch_settings_loaded(void)
{
    for (int i=1;i<15;i++) {
        const char *v=getenv(option_env[i]);
        /* These three toggles use presence, not the value, as their switch. */
        if (i==11 || i==13 || i==14) v=v ? "1" : "0";
        snprintf(startup[i],sizeof startup[i],"%s",v && *v ? v : option_default[i]);
    }
    atomic_store_explicit(&loaded,1,memory_order_release);
}
void android_touch_note_live(void)
{
    const char *v=getenv("DHD_PERF_COUNTER");
    atomic_store_explicit(&active_counter,v && atoi(v)!=0,memory_order_release);
    v=getenv("DHD_ASPECT");
    int shape=v && !strcmp(v,"stretch") ? 0 : v && !strcmp(v,"wide") ? 1 :
              v && !strcmp(v,"4:3") ? 2 : v && !strcmp(v,"16:9") ? 3 : v && !strcmp(v,"pixels") ? 4 : -1;
    atomic_store_explicit(&active_shape,shape,memory_order_release);
    v=getenv("DHD_PRESENT_FILTER");
    int filter=v && !strcmp(v,"point") ? 1 : 0;
    atomic_store_explicit(&active_filter,filter,memory_order_release);
}
const char *android_touch_running_value(int option)
{
    if (!atomic_load_explicit(&loaded,memory_order_acquire) || option<1 || option>14) return NULL;
    const char *value=startup[option];
    if (option==2) {int n=atomic_load_explicit(&active_counter,memory_order_acquire);if(n>=0)value=n ? "1" : "0";}
    if (option==3) {int n=atomic_load_explicit(&active_shape,memory_order_acquire);static const char *const s[]={"stretch","wide","4:3","16:9","pixels"};if(n>=0)value=s[n];}
    if (option==5) {int n=atomic_load_explicit(&active_filter,memory_order_acquire);static const char *const s[]={"linear","point"};if(n>=0)value=s[n];}
    return value;
}
#ifdef __ANDROID__
extern int android_settings_ready(void);
extern int host_setting_read(const char *,char *,size_t);
extern int host_setting_locked(const char *);
extern void host_sound_request_option(int,int);
extern int host_sound_applied_option(int);
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
    if (!android_settings_ready() || !atomic_load_explicit(&loaded,memory_order_acquire) ||
        !android_touch_option(option,value,&key,&setting) || host_setting_locked(option_env[option])) return JNI_FALSE;
    if (!host_setting_save(key,setting)) return JNI_FALSE;
    android_touch_queue_live(option,value);
    if(option==8 || option==10) host_sound_request_option(option,value);
    return JNI_TRUE;
}
JNIEXPORT jboolean JNICALL Java_app_downhillrevived_DhdActivity_nativeSettingsLoaded(JNIEnv *env,jclass cls)
{
    (void)env;(void)cls;
    return atomic_load_explicit(&loaded,memory_order_acquire) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_app_downhillrevived_DhdActivity_nativeReadOption(JNIEnv *env,jclass cls,jint option,jboolean saved)
{
    (void)cls;
    const char *key,*ignore,*value;
    char buffer[64];
    if (!atomic_load_explicit(&loaded,memory_order_acquire) || option<1 || option>14 ||
        !android_touch_option(option,option==1 ? 1 : 0,&key,&ignore)) return NULL;
    if (saved) {
        if (!host_setting_read(key,buffer,sizeof buffer)) return NULL;
        value=buffer;
    } else {
        value=android_touch_running_value(option);
        if(option==8) { int n=host_sound_applied_option(option); if(n>=0)value=n ? "1" : "0"; }
        if(option==10) { int n=host_sound_applied_option(option); if(n==70)value="70"; else if(n==120)value="120"; else if(n==200)value="200"; }
    }
    return (*env)->NewStringUTF(env,value);
}
#endif

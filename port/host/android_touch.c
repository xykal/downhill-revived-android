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
    if (!android_settings_ready()) return JNI_FALSE; /* don't race initial ISO/settings writes */
    if (option==1 && (value==1 || value==2)) return host_setting_save("scale", value==1 ? "1" : "2") ? JNI_TRUE : JNI_FALSE;
    if (option==2 && (value==0 || value==1)) return host_setting_save("perf_counter", value ? "1" : "0") ? JNI_TRUE : JNI_FALSE;
    return JNI_FALSE;
}
#endif

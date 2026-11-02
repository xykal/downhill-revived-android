#include "android_touch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static HostPad neutral(void) { HostPad p; memset(&p,0,sizeof p); p.left_x=p.left_y=p.right_x=p.right_y=128; return p; }
int main(void) {
    HostPad p=neutral();p.buttons=PAD_R1;p.left_x=200;p.pressure[9]=128;
    android_touch_set(PAD_CROSS|PAD_LEFT|PAD_L2,255,0,1,254,1); android_touch_merge(&p);
    assert((p.buttons&(PAD_R1|PAD_CROSS|PAD_LEFT|PAD_L2))==(PAD_R1|PAD_CROSS|PAD_LEFT|PAD_L2));
    assert(p.left_x==255 && p.left_y==0 && p.right_x==128 && p.right_y==128);
    assert(p.pressure[1]==255 && p.pressure[6]==255 && p.pressure[10]==255 && p.pressure[9]==128);
    android_touch_set(0,128,128,-99,999,2); p=neutral();p.left_x=99; android_touch_merge(&p);
    assert(p.left_x==99 && p.right_x==0 && p.right_y==255);
    android_touch_set(0,128,128,128,128,0);p=neutral();android_touch_merge(&p);
    assert(p.buttons==0 && p.left_x==128 && p.right_x==128);
    puts("PASS: physical pad merge, analog, pressure, clamp and release");
}

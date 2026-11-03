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
    const char *key, *value;
    assert(android_touch_option(1,1,&key,&value) && !strcmp(key,"scale") && !strcmp(value,"1"));
    assert(android_touch_option(1,2,&key,&value) && !strcmp(value,"2"));
    assert(android_touch_option(1,3,&key,&value) && !strcmp(value,"window"));
    assert(android_touch_option(3,0,&key,&value) && !strcmp(key,"DHD_ASPECT") && !strcmp(value,"stretch"));
    assert(android_touch_option(3,1,&key,&value) && !strcmp(value,"wide"));
    assert(android_touch_option(4,1,&key,&value) && !strcmp(key,"widescreen") && !strcmp(value,"1"));
    assert(android_touch_option(5,1,&key,&value) && !strcmp(key,"DHD_PRESENT_FILTER") && !strcmp(value,"point"));
    assert(!android_touch_option(3,3,&key,&value) && !android_touch_option(0,0,&key,&value));
    assert(!android_touch_option(1,-1,&key,&value) && !android_touch_option(1,1,NULL,&value));
    assert(android_touch_option(8,0,&key,&value) && !strcmp(key,"DHD_SND_STRETCH") && !strcmp(value,"0"));
    assert(android_touch_option(9,0,&key,&value) && !strcmp(key,"fps") && !strcmp(value,"60"));
    assert(android_touch_option(10,1,&key,&value) && !strcmp(key,"sound_latency_ms") && !strcmp(value,"120"));
    assert(!android_touch_option(10,3,&key,&value));
    assert(android_touch_option(11,0,&key,&value) && !strcmp(key,"DHD_TIMELINE") && !strcmp(value,""));
    assert(android_touch_option(11,1,&key,&value) && !strcmp(value,"1"));
    assert(android_touch_option(12,2,&key,&value) && !strcmp(key,"DHD_SPIN_US") && !strcmp(value,"0"));
    assert(android_touch_option(13,1,&key,&value) && !strcmp(key,"DHD_RSTAT"));
    assert(android_touch_option(14,0,&key,&value) && !strcmp(key,"DHD_SND_HOST_TRACE") && !strcmp(value,""));
    int counter,shape,filter;
    android_touch_queue_live(2,1); android_touch_queue_live(3,2); android_touch_queue_live(3,0); android_touch_queue_live(5,1);
    android_touch_take_live(&counter,&shape,&filter); assert(counter==1 && shape==0 && filter==1);
    android_touch_take_live(&counter,&shape,&filter); assert(counter==-1 && shape==-1 && filter==-1);
    android_touch_queue_live(3,9); android_touch_take_live(&counter,&shape,&filter); assert(shape==-1);
    puts("PASS: touch merge, settings whitelist and live-option queue");
}

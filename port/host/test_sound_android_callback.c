/* Exercise the real host_callback's live updates with fake SDL stream methods; no audio device.
   __ANDROID__ enables the production live-option path even in this Linux/QEMU fixture. */
#define __ANDROID__ 1
#include "sound_out_sdl.c"
#include <assert.h>
static int sent;
int SDLCALL SDL_GetAudioStreamQueued(SDL_AudioStream *s){(void)s;return 0;}
bool SDLCALL SDL_PutAudioStreamData(SDL_AudioStream *s,const void *p,int len){(void)s;(void)p;sent+=len;return true;}
double host_wall_seconds(void){return 1.0;}
void hle_log(const char *fmt,...){(void)fmt;}
int main(void){
    out.stretch=1;out.target=70*48;out.speed_at=1;out.speed=1;
    host_sound_request_option(8,0);host_sound_request_option(10,2);
    assert(host_sound_applied_option(8)==-1 && host_sound_applied_option(10)==-1);
    host_callback(NULL,NULL,0,480*4);
    assert(out.stretch==0 && out.target==200*48 && host_sound_applied_option(8)==0 && host_sound_applied_option(10)==200 && sent>0);
    host_sound_request_option(8,1);host_sound_request_option(10,1);host_callback(NULL,NULL,0,480*4);
    assert(out.stretch==1 && out.target==120*48 && host_sound_applied_option(8)==1 && host_sound_applied_option(10)==120);
    /* Transition a populated ring into stretching, leave it, then rejoin on the audio thread. */
    for(int i=0;i<16000;i++){ring[i*2]=(int16_t)((i%200)*100-10000);ring[i*2+1]=ring[i*2];}
    ring_read=0;ring_write=16000;st_read=st_write=0;st.on=0;
    out.running=out.started=1;out.speed=.6;out.speed_at=1;
    host_callback(NULL,NULL,0,480*4);assert(st.on && host_tempo<1);
    host_sound_request_option(8,0);host_callback(NULL,NULL,0,480*4);
    assert(!st.on && host_tempo==1.0 && host_sound_applied_option(8)==0);
    host_sound_request_option(8,1);host_callback(NULL,NULL,0,480*4);
    assert(st.on && host_tempo<1 && host_sound_applied_option(8)==1);
    puts("PASS: real audio callback applies settings and switches stretch safely");return 0;
}

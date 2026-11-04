#include <assert.h>
#include <stdio.h>
#include "sound_live_options.h"
int main(void)
{
    SoundLiveOptions s=SOUND_LIVE_OPTIONS_INIT;int stretch=1;unsigned target=70*48;
    assert(sound_live_running(&s,8)==-1 && sound_live_running(&s,10)==-1);
    sound_live_request(&s,8,0);sound_live_request(&s,10,2);
    assert(stretch==1 && target==70*48 && sound_live_running(&s,8)==-1);
    sound_live_apply(&s,&stretch,&target,48);
    assert(stretch==0 && target==200*48 && sound_live_running(&s,8)==0 && sound_live_running(&s,10)==200);
    sound_live_request(&s,8,1);sound_live_request(&s,8,0);sound_live_request(&s,10,1);
    sound_live_request(&s,10,99);sound_live_request(&s,8,-1);
    sound_live_apply(&s,&stretch,&target,48);
    assert(stretch==0 && target==120*48 && sound_live_running(&s,10)==120);
    sound_live_apply(&s,&stretch,&target,48);assert(stretch==0 && target==120*48);
    puts("PASS: audio callback option queue, coalescing and applied acknowledgements");return 0;
}

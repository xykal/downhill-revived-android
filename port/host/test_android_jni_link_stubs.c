/* Link-only fixture: real implementations are provided by the production Android host. */
#include <stddef.h>
int android_settings_ready(void){return 1;}
int host_setting_save(const char *key,const char *value){(void)key;(void)value;return 1;}
int host_setting_read(const char *key,char *out,size_t size){(void)key;(void)out;(void)size;return 0;}
int host_setting_locked(const char *name){(void)name;return 0;}
void host_sound_request_option(int option,int value){(void)option;(void)value;}
int host_sound_applied_option(int option){(void)option;return -1;}
int SDL_main(int argc,char **argv){(void)argc;(void)argv;return 0;}

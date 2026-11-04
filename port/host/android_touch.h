/* Touch input from Android UI, merged with the existing physical player-one pad. */
#ifndef DHD_ANDROID_TOUCH_H
#define DHD_ANDROID_TOUCH_H
#include "../hle/hle.h"
void android_touch_set(unsigned buttons, int lx, int ly, int rx, int ry, unsigned active);
void android_touch_merge(HostPad *out);
int android_touch_option(int option, int value, const char **key, const char **setting);
void android_touch_queue_live(int option, int value);
void android_touch_take_live(int *counter, int *shape, int *filter);
void android_touch_settings_loaded(void);
void android_touch_note_live(void);
const char *android_touch_running_value(int option);
#endif

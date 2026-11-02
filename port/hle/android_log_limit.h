/* Android's one bounded diagnostic log. The log-copy worker is the sole caller.
   Reserve space for the marker; a fatal signal may append its small direct record. */
#ifndef DHD_ANDROID_LOG_LIMIT_H
#define DHD_ANDROID_LOG_LIMIT_H
#include <stddef.h>
#define DHD_ANDROID_LOG_MAX (2u * 1024u * 1024u)
#define DHD_ANDROID_LOG_NOTICE "[LOG] Size limit reached; further normal lines suppressed. A fatal-signal record may follow."
struct dhd_log_limit { size_t bytes; int warned; };
/* 1: record this line, 2: write the notice instead, 0: suppress. */
static inline int dhd_log_limit_next(struct dhd_log_limit *s, size_t line_bytes)
{
    const size_t reserve = sizeof DHD_ANDROID_LOG_NOTICE; /* newline replaces NUL */
    if (s->warned) return 0;
    if (s->bytes <= DHD_ANDROID_LOG_MAX - reserve &&
        line_bytes <= DHD_ANDROID_LOG_MAX - reserve - s->bytes) {
        s->bytes += line_bytes;
        return 1;
    }
    s->warned = 1;
    s->bytes += reserve;
    return 2;
}
#endif

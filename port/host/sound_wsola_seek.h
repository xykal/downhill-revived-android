/* WSOLA correlation search shared with test_sound_wsola.c.
   Caller supplies ST_HOP, ST_SEEK and st_mono(long). */
#ifndef DHD_SOUND_WSOLA_SEEK_H
#define DHD_SOUND_WSOLA_SEEK_H
#include <math.h>

static long st_seek(long lo, long hi, long ref)
{
    static float want[ST_HOP / 2];
    /* The candidates overlap almost completely. Decode each mono sample once instead of doing
       tens of thousands of ring modulo operations per hop. The score and iteration order are
       unchanged. There is exactly one sound-output callback thread, as for the original want[]. */
    static float candidates[ST_HOP + 2 * ST_SEEK];
    long best = lo, at;
    double best_score = -1e300;
    for (int i = 0; i < ST_HOP / 2; i++) want[i] = st_mono(ref + 2 * i);
    for (long i = 0; i < hi - lo + ST_HOP; i++) candidates[i] = st_mono(lo + i);
    for (int pass = 0; pass < 2; pass++) {
        long from = pass ? best - 1 : lo, to = pass ? best + 1 : hi, step = pass ? 1 : 2;
        if (from < lo) from = lo;
        if (to > hi) to = hi;
        for (at = from; at <= to; at += step) {
            double dot = 0, energy = 1;
            const float *candidate = candidates + at - lo;
            for (int i = 0; i < ST_HOP / 2; i++) {
                float x = candidate[2 * i];
                dot += x * want[i]; energy += x * x;
            }
            double score = dot / sqrt(energy);
            if (score > best_score) { best_score = score; best = at; }
        }
    }
    return best;
}
#endif

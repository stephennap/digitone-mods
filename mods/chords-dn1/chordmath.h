/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS' chord math: pure functions, no firmware, so the same code builds for the Digitone and is
 * copied line for line by tests/test_chords_dn1.py. After the Digitone II's keyboard chord modes:
 * CHORD ROOT, CHORD CENTER and CHORD SPREAD, a SHAPE, and BASS OCT. */
#ifndef CHORDS_CHORDMATH_H
#define CHORDS_CHORDMATH_H

#include <stdint.h>

#define CH_MAX_TONES 8                  /* a step plays the note and up to 7 intervals */
#define CH_SCALES    35                 /* the Digitone's SCALE 0-34; -1 is chromatic */

/* Each scale as a 12-bit mask over the semitones above ROOT (bit 0 the root), in the Digitone's
 * SCALE order: the OS's own scale table (1.43: 0x401d4e68, 13 bytes a scale, each semitone's degree
 * from 1 or 0 for none, then the octave's), which tests/test_chords_dn1.py checks all 35 against. */
#define M(a, b, c, d, e, f, g) ((1u << (a)) | (1u << (b)) | (1u << (c)) | (1u << (d)) | (1u << (e)) | \
                                (1u << (f)) | (1u << (g)))
static const uint16_t ch_scale_mask[CH_SCALES] = {
    M(0, 2, 4, 5, 7, 9, 11),            /*  0 Ionian (Maj) */
    M(0, 2, 3, 5, 7, 9, 10),            /*  1 Dorian */
    M(0, 1, 3, 5, 7, 8, 10),            /*  2 Phrygian */
    M(0, 2, 4, 6, 7, 9, 11),            /*  3 Lydian */
    M(0, 2, 4, 5, 7, 9, 10),            /*  4 Mixolydian */
    M(0, 2, 3, 5, 7, 8, 10),            /*  5 Aeolian (Min) */
    M(0, 1, 3, 5, 6, 8, 10),            /*  6 Locrian */
    M(0, 3, 5, 7, 10, 0, 0),            /*  7 Pentatonic Minor */
    M(0, 2, 4, 7, 9, 0, 0),             /*  8 Pentatonic Major */
    M(0, 2, 3, 5, 7, 9, 11),            /*  9 Melodic Minor */
    M(0, 2, 3, 5, 7, 8, 11),            /* 10 Harmonic Minor */
    M(0, 2, 4, 6, 8, 10, 0),            /* 11 Whole Tone */
    M(0, 3, 5, 6, 7, 10, 0),            /* 12 Blues */
    M(0, 2, 3, 5, 7, 8, 10) | (1u << 11),   /* 13 Combo Minor */
    M(0, 1, 4, 5, 6, 8, 11),            /* 14 Persian */
    M(0, 1, 5, 6, 10, 0, 0),            /* 15 Iwato */
    M(0, 1, 5, 7, 10, 0, 0),            /* 16 In-Sen */
    M(0, 2, 3, 7, 8, 0, 0),             /* 17 Hirajoshi */
    M(0, 1, 3, 7, 8, 0, 0),             /* 18 Pelog */
    M(0, 1, 4, 5, 7, 8, 10),            /* 19 Phrygian Dominant */
    M(0, 2, 3, 5, 6, 8, 9) | (1u << 11),    /* 20 Whole-Half Diminished */
    M(0, 1, 3, 4, 6, 7, 9) | (1u << 10),    /* 21 Half-Whole Diminished */
    M(0, 1, 3, 4, 5, 6, 8) | (1u << 10),    /* 22 Spanish */
    M(0, 2, 4, 5, 6, 8, 10),            /* 23 Major Locrian */
    M(0, 1, 3, 4, 6, 8, 10),            /* 24 Super Locrian */
    M(0, 1, 3, 5, 7, 9, 10),            /* 25 Dorian b2 */
    M(0, 2, 4, 6, 8, 9, 11),            /* 26 Lydian Augmented */
    M(0, 2, 4, 6, 7, 9, 10),            /* 27 Lydian Dominant */
    M(0, 1, 4, 5, 7, 8, 11),            /* 28 Double Harmonic Major */
    M(0, 3, 4, 6, 7, 10, 11),           /* 29 Lydian #2 #6 */
    M(0, 1, 3, 4, 7, 8, 9),             /* 30 Ultraphrygian */
    M(0, 2, 3, 6, 7, 8, 11),            /* 31 Hungarian Minor */
    M(0, 1, 4, 5, 6, 9, 10),            /* 32 Oriental */
    M(0, 3, 4, 5, 8, 9, 11),            /* 33 Ionian #2 #5 */
    M(0, 1, 2, 5, 6, 8, 9),             /* 34 Locrian bb3 bb7 */
};
#undef M

/* SHAPE, 0-7 (TRIG 1-8). In a scale: the scale steps from the played note (0 the note, 2 a third up
 * the scale, ...), so the chord is the scale's own; CH_NINTH is the 9th, the step above the note an
 * octave up, in a scale of any size (in a 7-note scale, 8 steps). With SCALE on chromatic: a chord
 * type, in semitones. -1 ends a list. */
#define CH_SHAPES 8
#define CH_NINTH  9
static const int8_t ch_shape_steps[CH_SHAPES][6] = {
    { 0, 2, 4, -1 },                    /* 1-3-5 */
    { 0, 2, 4, 6, -1 },                 /* 1-3-5-7 */
    { 0, 2, 4, 6, CH_NINTH, -1 },       /* 1-3-5-7-9 */
    { 0, 1, 4, -1 },                    /* 1-2-5 */
    { 0, 3, 4, -1 },                    /* 1-4-5 */
    { 0, 2, 4, 5, -1 },                 /* 1-3-5-6 */
    { 0, 2, 4, CH_NINTH, -1 },          /* 1-3-5-9 */
    { 0, 4, -1 },                       /* 1-5 */
};
static const int8_t ch_shape_semis[CH_SHAPES][6] = {
    { 0, 4, 7, -1 },                    /* MAJ */
    { 0, 3, 7, -1 },                    /* MIN */
    { 0, 4, 7, 10, -1 },                /* 7TH */
    { 0, 3, 7, 10, -1 },                /* MIN7 */
    { 0, 4, 7, 11, -1 },                /* MAJ7 */
    { 0, 3, 6, -1 },                    /* DIM */
    { 0, 2, 7, -1 },                    /* SUS2 */
    { 0, 5, 7, -1 },                    /* SUS4 */
};

/* MODE: how the chord's notes are placed. */
enum { CH_ROOT, CH_CENTER, CH_SPREAD, CH_MODES };

static void ch_sort(int32_t *t, int32_t n)
{
    int32_t i, j, v;
    for (i = 1; i < n; i++) {
        v = t[i];
        for (j = i; j > 0 && t[j - 1] > v; j--)
            t[j] = t[j - 1];
        t[j] = v;
    }
}

/* The chord a single played note makes, as the firmware's notes. note: a MIDI note 0-127; root: the
 * keyboard's ROOT (a MIDI note); scale: -1 chromatic, else 0-34; shape: 0-7; mode: CH_ROOT,
 * CH_CENTER or CH_SPREAD; center: CENTER POSITION, a MIDI note; bass: BASS OCT, 0 none or 1-3
 * octaves below the chord's root. -> how many of notes[] it wrote (notes[0] the lowest, the rest
 * intervals above it, each 1-127), or 0: play as stock (a SCALE out of range, a note outside the
 * scale, or a chord it cannot place in the MIDI range). */
static int32_t ch_chord(int32_t note, int32_t root, int32_t scale, int32_t shape, int32_t mode,
                        int32_t center, int32_t bass, int8_t notes[CH_MAX_TONES])
{
    int32_t tones[CH_MAX_TONES], n = 0, r = 0, i, j;

    if (note < 0 || note > 127 || shape < 0 || shape >= CH_SHAPES || mode < 0 || mode >= CH_MODES)
        return 0;

    /* the chord in root position, tones[0] its root */
    if (scale < 0) {
        for (i = 0; ch_shape_semis[shape][i] >= 0; i++)
            tones[n++] = note + ch_shape_semis[shape][i];
    } else {
        int32_t deg[12], k = 0, d = -1, pc, s;
        uint32_t mask;
        if (scale >= CH_SCALES || !(mask = ch_scale_mask[scale]))
            return 0;
        for (s = 0; s < 12; s++)
            if (mask & (1u << s))
                deg[k++] = s;
        pc = ((note - root) % 12 + 12) % 12;
        for (s = 0; s < k; s++)
            if (deg[s] == pc)
                d = s;
        if (d < 0)
            return 0;
        for (i = 0; ch_shape_steps[shape][i] >= 0; i++) {
            int32_t t, dup = 0;
            j = d + (ch_shape_steps[shape][i] == CH_NINTH ? k + 1 : ch_shape_steps[shape][i]);
            t = note + deg[j % k] - deg[d] + 12 * (j / k);
            for (s = 0; s < n; s++)     /* a small scale's 7th can be its 9th: keep it once */
                if (tones[s] == t)
                    dup = 1;
            if (!dup)
                tones[n++] = t;
        }
    }

    /* CENTER and SPREAD: each note to the octave nearest CENTER POSITION (center-5 .. center+6), so
     * the hand stays put while the chords change; a note class twice over (a small scale's 9th) is
     * kept once. SPREAD then lifts every second note an octave. */
    if (mode != CH_ROOT) {
        int32_t m = 0;
        for (i = 0; i < n; i++) {
            int32_t t = center + (((tones[i] - center) % 12) + 12) % 12, dup = 0;
            if (t > center + 6)
                t -= 12;
            for (j = 0; j < m; j++)
                if (tones[j] == t)
                    dup = 1;
            if (i == 0)
                r = t;                  /* the root, as placed */
            if (!dup)
                tones[m++] = t;
        }
        n = m;
        ch_sort(tones, n);
        if (mode == CH_SPREAD)
            for (i = 1; i < n; i += 2)
                tones[i] += 12;
    } else {
        r = tones[0];
    }
    ch_sort(tones, n);

    /* BASS OCT: the root again, bass octaves below the root as placed (its lowest copy) and under
     * every other note; with more than three notes it takes that root's place. */
    if (bass > 0) {
        int32_t b, at = 0;
        r = ((r % 12) + 12) % 12;
        while (at < n && ((tones[at] % 12) + 12) % 12 != r)
            at++;
        if (at == n)
            return 0;
        b = tones[at] - 12 * bass;
        while (b >= tones[0])
            b -= 12;
        if (b >= 0) {
            if (n > 3) {
                for (i = at; i < n - 1; i++)
                    tones[i] = tones[i + 1];
                n--;
            }
            for (i = n; i > 0; i--)
                tones[i] = tones[i - 1];
            tones[0] = b;
            n++;
        }
    }

    if (n < 2 || n > CH_MAX_TONES)
        return 0;
    while (tones[n - 1] > 127 && tones[0] >= 12)    /* keep every note a MIDI note */
        for (i = 0; i < n; i++)
            tones[i] -= 12;
    if (tones[0] < 0 || tones[n - 1] > 127)
        return 0;
    for (i = 1; i < n; i++)
        if (tones[i] == tones[i - 1])
            return 0;
    notes[0] = (int8_t)tones[0];
    for (i = 1; i < n; i++)
        notes[i] = (int8_t)(tones[i] - tones[0]);
    return n;
}

#endif

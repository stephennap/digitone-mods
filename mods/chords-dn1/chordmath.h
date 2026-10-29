/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS' chord math: pure functions, no firmware, so the same code builds for the Digitone and for
 * the host test (test_chordmath.c). */
#ifndef CHORDS_CHORDMATH_H
#define CHORDS_CHORDMATH_H

#include <stdint.h>

#define CH_MAX_TONES 8                  /* a step plays the note and up to 7 intervals */
#define CH_SCALES    35                 /* the Digitone's SCALE 0-34; -1 is chromatic */

/* Each scale as a 12-bit mask over the semitones above ROOT (bit 0 the root), in the Digitone's
 * SCALE order. The 7-note scales and Whole Tone are the OS's own table (1.43: 0x401d3ed0, 7 bytes a
 * scale); tests/test_chords_dn1.py checks them against the stock file. The other scales are the
 * textbook ones where the name has one meaning; 0 where it does not (those play as stock). */
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
    0,                                  /* 13 Combo Minor: not known */
    0,                                  /* 14 Persian: not known */
    0,                                  /* 15 Iwato: not known */
    0,                                  /* 16 In-Sen: not known */
    0,                                  /* 17 Hirajoshi: not known */
    0,                                  /* 18 Pelog: not known */
    M(0, 1, 4, 5, 7, 8, 10),            /* 19 Phrygian Dominant */
    M(0, 2, 3, 5, 6, 8, 9) | (1u << 11),    /* 20 Whole-Half Diminished */
    M(0, 1, 3, 4, 6, 7, 9) | (1u << 10),    /* 21 Half-Whole Diminished */
    0,                                  /* 22 Spanish: not known */
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

/* Chord types: scale-degree steps from the played note (0 the note, 2 a third up the scale, ...),
 * and the semitones used when SCALE is chromatic (a major-quality chord). */
enum { CH_OFF, CH_TRIAD, CH_SUS2, CH_SUS4, CH_SIXTH, CH_SEVENTH, CH_NINTH, CH_ADD9, CH_TYPES };
static const int8_t ch_degrees[CH_TYPES][6] = {     /* -1 ends a list */
    { -1 },
    { 0, 2, 4, -1 },
    { 0, 1, 4, -1 },
    { 0, 3, 4, -1 },
    { 0, 2, 4, 5, -1 },
    { 0, 2, 4, 6, -1 },
    { 0, 2, 4, 6, 8, -1 },
    { 0, 2, 4, 8, -1 },
};
static const int8_t ch_chromatic[CH_TYPES][6] = {
    { -1 },
    { 0, 4, 7, -1 },
    { 0, 2, 7, -1 },
    { 0, 5, 7, -1 },
    { 0, 4, 7, 9, -1 },
    { 0, 4, 7, 10, -1 },
    { 0, 4, 7, 10, 14, -1 },
    { 0, 4, 7, 14, -1 },
};

/* Voicings: CLOSE as built, INV 1-3 move the lowest 1-3 tones up an octave, OPEN moves the
 * root position's second tone up an octave. */
enum { CH_CLOSE, CH_INV1, CH_INV2, CH_INV3, CH_OPEN, CH_VOICINGS };

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

/* Builds the chord for one played note. note: a MIDI note 0-127; root: the keyboard's ROOT (a MIDI
 * note); scale: -1 chromatic, else 0-34. -> how many of notes[] it wrote (notes[0] the lowest
 * tone, the rest its intervals above it, each 1-127), or 0: play as stock (chord off, a scale with
 * no known notes, or a note outside the scale). */
static int32_t ch_build(int32_t note, int32_t root, int32_t scale, int32_t type, int32_t voicing,
                        int8_t notes[CH_MAX_TONES])
{
    int32_t tones[CH_MAX_TONES], n = 0, i;

    if (note < 0 || note > 127 || type <= CH_OFF || type >= CH_TYPES)
        return 0;
    if (scale < 0) {                    /* chromatic: fixed shapes */
        for (i = 0; ch_chromatic[type][i] >= 0; i++)
            tones[n++] = note + ch_chromatic[type][i];
    } else {
        int32_t deg[12], k = 0, pc, d = -1, s;
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
        if (d < 0)                      /* not in the scale: leave it alone */
            return 0;
        for (i = 0; ch_degrees[type][i] >= 0; i++) {
            int32_t j = d + ch_degrees[type][i];
            tones[n++] = note + deg[j % k] - deg[d] + 12 * (j / k);
        }
    }

    if (voicing >= CH_INV1 && voicing <= CH_INV3) {
        for (i = 0; i < voicing && i < n - 1; i++)
            tones[i] += 12;
    } else if (voicing == CH_OPEN && n >= 3) {
        tones[1] += 12;
    }
    ch_sort(tones, n);

    while (tones[n - 1] > 127 && tones[0] >= 12)    /* keep every tone a MIDI note */
        for (i = 0; i < n; i++)
            tones[i] -= 12;
    if (tones[n - 1] > 127)
        return 0;

    for (i = 1; i < n; i++)             /* distinct, so every interval is 1 or more */
        if (tones[i] == tones[i - 1])
            return 0;
    notes[0] = (int8_t)tones[0];
    for (i = 1; i < n; i++)
        notes[i] = (int8_t)(tones[i] - tones[0]);
    return n;
}

#endif

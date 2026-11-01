/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS: the Digitone II's keyboard chord modes for the Digitone mk1 and Digitone Keys (core-dn1 3.3,
 * seq-events). Three knobs on a third AMP page, Chords, kept in the sound like any stock knob:
 *   MODE  OFF, or CHORD ROOT, CENTER or SPREAD with BASS OCT 0, -1, -2 or -3 (ROOT, RT-1 ... SP-3)
 *   SHAP  the chord's shape, 0-7; TRIG 1-8 pick it while the page shows
 *   CENT  CENTER POSITION: AUTO (the played note) or C2-C7
 * With MODE on, each single-note trig on the track plays the chord from the track's keyboard ROOT and
 * SCALE. Trigs that already hold a chord play as stored. Phase 1: the sequencer's trigs, not yet the
 * TRIG keys played live. */
#include "digitone-mk1/core3.h"
#include "digitone-mk1/seq.h"
#include "chordmath.h"

#define ID_MODE     182                 /* the parameter ids, claimed as param:182 to param:184 */
#define ID_SHAP     183
#define ID_CENT     184
#define SLOT_MODE   26                  /* the sound slots their values live in (no stock use) */
#define SLOT_SHAP   65
#define SLOT_CENT   0                   /* also where an LFO with no destination points: harmless here,
                                         * this mod reads the kit, never the render's words */
#define MODES       13                  /* OFF, then 3 modes x 4 bass octaves */
#define CENT_MAX    61                  /* CENT: 0 AUTO (the played note), 1-61 C2-C7 */
#define CENT_BASE   35                  /* so 1 is C2 (36) */

#define KEY_TRIG1   26
#define EV_KEY(ev)   (*(volatile uint32_t *)((char *)(ev) + 12))
#define EV_FLAGS(ev) (*(volatile uint32_t *)((char *)(ev) + 16))

/* The pattern's lock tables (as fxkit-dn1 reads them): a track's 10319 bytes, a step's row 160, a
 * slot's lock a big-endian s16 at 2 * slot, -1 for none. 10319 is odd, so read them by bytes. */
#define LOCKS        0x1e80
#define TRACK_LOCKS  10319
#define STEP_LOCKS   160

/* A knob's value as this step plays it, 8.8 >> 8: its p-lock, else the sound's. Slot 0's lock is not
 * read: fxkit-dn1 treats slot 0 in a lock row as no slot, and nothing here says otherwise. */
static int32_t ch_value(int32_t track, int32_t step, int32_t slot, int32_t lo, int32_t hi)
{
    uint8_t *pat = fw_pattern, *kit = fw_kit;
    int32_t v = -1;
    if (slot > 0 && pat && step >= 0 && step < SQ_STEPS) {
        const volatile uint8_t *p = pat + LOCKS + TRACK_LOCKS * track + STEP_LOCKS * step + 2 * slot;
        v = (int16_t)(p[0] << 8 | p[1]);
    }
    if (v < 0)
        v = kit ? FW_SOUND_SLOT(kit, track, slot) : lo << 8;
    v >>= 8;
    return v < lo ? lo : v > hi ? hi : v;
}

/* Interrupt level, after every note generator. */
void chords_notes(int32_t track, int32_t step, int8_t notes[SQ_NOTES])
{
    struct sq_track info;
    int8_t out[CH_MAX_TONES];
    int32_t i, n, mode, shape, center;

    if (track < 0 || track >= SQ_AUDIO || notes[0] < 0)
        return;
    mode = ch_value(track, step, SLOT_MODE, 0, MODES - 1);
    if (!mode)
        return;
    for (i = 1; i < SQ_NOTES; i++)      /* a stored chord plays as stored */
        if (notes[i])
            return;
    if (sq_track_info(track, &info, sizeof info) < 0 || info.root == SQ_ROOT_NONE)
        return;
    shape = ch_value(track, step, SLOT_SHAP, 0, CH_SHAPES - 1);
    center = ch_value(track, step, SLOT_CENT, 0, CENT_MAX);
    center = center ? CENT_BASE + center : notes[0];   /* every sound starts at 0: AUTO */
    n = ch_chord(notes[0], info.root, info.scale, shape, (mode - 1) / 4, center, (mode - 1) % 4,
                 out);
    if (n <= 0)
        return;
    for (i = 0; i < SQ_NOTES; i++)
        notes[i] = i < n ? out[i] : 0;
}

/* ---- the knobs and their page ----------------------------------------------------------------- */
static void ch_text(char *buf, const char *s)
{
    while ((*buf++ = *s++))
        ;
}

static const char *const ch_mode_names[MODES] = {
    "OFF", "ROOT", "RT-1", "RT-2", "RT-3", "CNTR", "CN-1", "CN-2", "CN-3", "SPRD", "SP-1", "SP-2", "SP-3"
};
static const char *const ch_step_names[CH_SHAPES] = {
    "135", "1357", "1-9", "125", "145", "1356", "1359", "15"
};
static const char *const ch_semi_names[CH_SHAPES] = {
    "MAJ", "MIN", "7TH", "MIN7", "MAJ7", "DIM", "SUS2", "SUS4"
};
static const char ch_pcs[] = "C C#D D#E F F#G G#A A#B ";

static void ch_fmt_mode(int32_t value, char *buf)
{
    int32_t v = value >> 8;
    ch_text(buf, ch_mode_names[v < 0 ? 0 : v >= MODES ? MODES - 1 : v]);
}

/* SHAP's names follow the active track's SCALE: degrees in a scale, chord types on chromatic. */
static void ch_fmt_shape(int32_t value, char *buf)
{
    int32_t v = value >> 8, t = fw_active_track;
    uint8_t *pat = fw_pattern;
    int chromatic = pat && t >= 0 && t < SQ_AUDIO && FW_TRACK_SCALE(FW_TRACK(pat, t)) < 0;
    v = v < 0 ? 0 : v >= CH_SHAPES ? CH_SHAPES - 1 : v;
    ch_text(buf, chromatic ? ch_semi_names[v] : ch_step_names[v]);
}

/* CENT: AUTO, or a note name, MIDI 60 = C4. */
static void ch_fmt_center(int32_t value, char *buf)
{
    int32_t v = value >> 8, o;
    if (v <= 0 || v > CENT_MAX) {
        ch_text(buf, "AUTO");
        return;
    }
    v += CENT_BASE;
    *buf++ = ch_pcs[2 * (v % 12)];
    if (ch_pcs[2 * (v % 12) + 1] != ' ')
        *buf++ = '#';
    o = v / 12 - 1;
    if (o < 0) {
        *buf++ = '-';
        o = -o;
    }
    *buf++ = (char)('0' + o);
    *buf = 0;
}

const struct core_param chords_mode = {
    ID_MODE, 1, SLOT_MODE, 0, (MODES - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Chord mode", fw_str_amp, "MODE", ch_fmt_mode, fw_str_empty, 0
};
const struct core_param chords_shap = {
    ID_SHAP, 1, SLOT_SHAP, 0, (CH_SHAPES - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Chord shape", fw_str_amp, "SHAP", ch_fmt_shape, fw_str_empty, 0
};
const struct core_param chords_cent = {
    ID_CENT, 1, SLOT_CENT, 0, CENT_MAX << 8, 0, 0, -1, -1, -1, 0,
    "Center position", fw_str_amp, "CENT", ch_fmt_center, fw_str_empty, 0
};

struct core_page chords_page = {        /* AMP's third page; core writes view and pos */
    27, 9, "CHRD", "Chords", { ID_MODE, ID_SHAP, ID_CENT, 0, 0, 0, 0, 0 }, 9, 0, 0
};

/* UI task: while the Chords page shows, TRIG 1-8 pick the active track's shape, as turning SHAP
 * does; their releases are taken too, so the stock never sees half a press. */
int32_t chords_key(void *brain, void *ev)
{
    uint32_t key = EV_KEY(ev), fl = EV_FLAGS(ev);
    int32_t t = fw_active_track;

    if (key < KEY_TRIG1 || key >= KEY_TRIG1 + CH_SHAPES || (fl & 2))   /* FUNC + TRIG stays stock */
        return 0;
    if (t < 0 || t >= SQ_AUDIO || !core_page_shown(brain, &chords_page))
        return 0;
    if ((fl & 1) && !(fl & 8))
        core_sound_set(t, SLOT_SHAP, (int32_t)(key - KEY_TRIG1) << 8);
    return 1;
}

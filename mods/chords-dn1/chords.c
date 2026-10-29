/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS: scale-aware chords for the Digitone mk1 and Digitone Keys (core-dn1 3.3, seq-events).
 * Two knobs on a third AMP page, Chords: CHRD and VOIC, kept in the sound, p-lockable like any stock
 * knob. CHRD KEY makes the chords a trig holds (the Note Add menu's) follow the track's keyboard ROOT
 * and SCALE; TRI to ADD9 do that and also make each single-note trig that chord in the key. */
#include "digitone-mk1/core3.h"
#include "digitone-mk1/seq.h"
#include "chordmath.h"

#define ID_CHRD     182                 /* the parameter ids, claimed as param:182 and param:183 */
#define ID_VOIC     183
#define SLOT_CHRD   26                  /* the sound slots they keep their values in (no stock use) */
#define SLOT_VOIC   65

/* CHRD's values: OFF, KEY, then a chord type each (CH_TRIAD..CH_ADD9 = the value - 1). */
enum { MODE_OFF, MODE_KEY, MODE_TYPES };
#define MODES (MODE_TYPES + CH_TYPES - 2)

/* The pattern's lock tables (as fxkit-dn1 reads them): a track's 10319 bytes, a step's row 160, a
 * slot's lock a big-endian s16 at 2 * slot, -1 for none. 10319 is odd, so read them by bytes. */
#define LOCKS        0x1e80
#define TRACK_LOCKS  10319
#define STEP_LOCKS   160

/* The knob's value as this step plays it: its p-lock, else the sound's. -> 0..count-1 */
static int32_t ch_value(int32_t track, int32_t step, int32_t slot, int32_t count)
{
    uint8_t *pat = fw_pattern, *kit = fw_kit;
    int32_t v = -1;
    if (pat && step >= 0 && step < SQ_STEPS) {
        const volatile uint8_t *p = pat + LOCKS + TRACK_LOCKS * track + STEP_LOCKS * step + 2 * slot;
        v = (int16_t)(p[0] << 8 | p[1]);
    }
    if (v < 0)
        v = kit ? FW_SOUND_SLOT(kit, track, slot) : 0;
    v >>= 8;                            /* 8.8 */
    return v < 0 ? 0 : v >= count ? count - 1 : v;
}

/* Interrupt level, after every note generator. */
void chords_notes(int32_t track, int32_t step, int8_t notes[SQ_NOTES])
{
    struct sq_track info;
    int8_t out[CH_MAX_TONES];
    int32_t i, n, mode, voicing, stored = 0;

    if (track < 0 || track >= SQ_AUDIO || notes[0] < 0)
        return;
    mode = ch_value(track, step, SLOT_CHRD, MODES);
    if (mode == MODE_OFF)
        return;
    if (sq_track_info(track, &info, sizeof info) < 0 || info.root == SQ_ROOT_NONE)
        return;
    voicing = ch_value(track, step, SLOT_VOIC, CH_VOICINGS);
    for (i = 1; i < SQ_NOTES; i++)
        if (notes[i])
            stored = 1;
    if (stored)                         /* a chord the trig holds: into the key */
        n = ch_follow(notes, info.root, info.scale, voicing, out);
    else if (mode >= MODE_TYPES)        /* one note: the chord CHRD names */
        n = ch_build(notes[0], info.root, info.scale, mode - 1, voicing, out);
    else
        n = 0;
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
    "OFF", "KEY", "TRI", "SUS2", "SUS4", "6TH", "7TH", "9TH", "ADD9"
};
static const char *const ch_voicing_names[CH_VOICINGS] = { "CLS", "INV1", "INV2", "INV3", "OPEN" };

static void ch_fmt_mode(int32_t value, char *buf)
{
    int32_t v = value >> 8;
    ch_text(buf, ch_mode_names[v < 0 ? 0 : v >= MODES ? MODES - 1 : v]);
}

static void ch_fmt_voicing(int32_t value, char *buf)
{
    int32_t v = value >> 8;
    ch_text(buf, ch_voicing_names[v < 0 ? 0 : v >= CH_VOICINGS ? CH_VOICINGS - 1 : v]);
}

const struct core_param chords_chrd = {
    ID_CHRD, 1, SLOT_CHRD, 0, (MODES - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Chord", fw_str_amp, "CHRD", ch_fmt_mode, fw_str_empty, 0
};
const struct core_param chords_voic = {
    ID_VOIC, 1, SLOT_VOIC, 0, (CH_VOICINGS - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Voicing", fw_str_amp, "VOIC", ch_fmt_voicing, fw_str_empty, 0
};

struct core_page chords_page = {        /* AMP's third page; core writes view and pos */
    27, 9, "CHRD", "Chords", { ID_CHRD, ID_VOIC, 0, 0, 0, 0, 0, 0 }, 9, 0, 0
};

/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS: scale-aware chords for the Digitone mk1 and Digitone Keys (core-dn1 3.1, seq-events).
 * Hold a synth track's key, pick CHORDS in the Mod Menu, then a chord type and a voicing: from then
 * on each single-note trig on that track plays a chord built from the track's keyboard ROOT and
 * SCALE (a C in C minor plays C, Eb, G). Trigs that already hold a chord play as stored. */
#include "digitone-mk1/core3.h"
#include "digitone-mk1/seq.h"
#include "chordmath.h"

/* Per synth track, written in the UI task, read in the fire: one byte each, so no lock is needed. */
static volatile uint8_t ch_type[SQ_AUDIO];
static volatile uint8_t ch_voicing[SQ_AUDIO];

/* Interrupt level, after every note generator: a single note becomes the track's chord. */
void chords_notes(int32_t track, int32_t step, int8_t notes[SQ_NOTES])
{
    struct sq_track info;
    int8_t out[CH_MAX_TONES];
    int32_t i, n;

    (void)step;
    if (track < 0 || track >= SQ_AUDIO || ch_type[track] == CH_OFF || notes[0] < 0)
        return;
    for (i = 1; i < SQ_NOTES; i++)      /* a stored chord: leave it as it is */
        if (notes[i])
            return;
    if (sq_track_info(track, &info, sizeof info) < 0 || info.root == SQ_ROOT_NONE)
        return;
    n = ch_build(notes[0], info.root, info.scale, ch_type[track], ch_voicing[track], out);
    if (n <= 0)
        return;
    for (i = 0; i < SQ_NOTES; i++)
        notes[i] = i < n ? out[i] : 0;
}

/* The menus. A submenu's entries are picked through core_menu_open's pick, never their open(). */
static void ch_none(void *brain, void *event, int32_t track)
{
    (void)brain, (void)event, (void)track;
}

#define ITEM(label) { label, ch_none, 0, 0 }
static const struct core_menu_item ch_t_off = ITEM("OFF"), ch_t_triad = ITEM("TRIAD"),
    ch_t_sus2 = ITEM("SUS2"), ch_t_sus4 = ITEM("SUS4"), ch_t_6th = ITEM("6TH"),
    ch_t_7th = ITEM("7TH"), ch_t_9th = ITEM("9TH"), ch_t_add9 = ITEM("ADD9");
static const void *const ch_types_list[] = {       /* in CH_OFF..CH_ADD9 order */
    &ch_t_off, &ch_t_triad, &ch_t_sus2, &ch_t_sus4, &ch_t_6th, &ch_t_7th, &ch_t_9th, &ch_t_add9, 0
};
static const struct core_menu_item ch_v_close = ITEM("CLOSE"), ch_v_inv1 = ITEM("INV 1"),
    ch_v_inv2 = ITEM("INV 2"), ch_v_inv3 = ITEM("INV 3"), ch_v_open = ITEM("OPEN");
static const void *const ch_voicings_list[] = {    /* in CH_CLOSE..CH_OPEN order */
    &ch_v_close, &ch_v_inv1, &ch_v_inv2, &ch_v_inv3, &ch_v_open, 0
};
#undef ITEM

static void ch_pick_voicing(void *brain, void *event, int32_t track, int32_t index)
{
    (void)brain, (void)event;
    if (track >= 0 && track < SQ_AUDIO && index >= 0 && index < CH_VOICINGS)
        ch_voicing[track] = (uint8_t)index;
}

static void ch_pick_type(void *brain, void *event, int32_t track, int32_t index)
{
    (void)brain, (void)event;
    if (track < 0 || track >= SQ_AUDIO || index < 0 || index >= CH_TYPES)
        return;
    ch_type[track] = (uint8_t)index;
    if (index != CH_OFF)                /* then the voicing, the current one selected */
        core_menu_open(ch_voicings_list, ch_voicing[track], ch_pick_voicing);
}

static void ch_open(void *brain, void *event, int32_t track)
{
    (void)brain, (void)event;
    if (track >= 0 && track < SQ_AUDIO)
        core_menu_open(ch_types_list, ch_type[track], ch_pick_type);
}

static const uint16_t ch_icon[16] = {   /* three stacked notes on a staff */
    0x0000, 0xffff, 0x0000, 0x0780, 0x0fc0, 0xffff, 0x0780, 0x01e0,
    0x03f0, 0xffff, 0x01e0, 0x0780, 0x0fc0, 0xffff, 0x0780, 0x0000
};

const struct core_menu_item chords_menu = { "CHORDS", ch_open, CORE_MENU_ICON, ch_icon };

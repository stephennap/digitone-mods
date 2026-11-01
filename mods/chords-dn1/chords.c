/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS: the Digitone II's keyboard chord modes for the Digitone mk1 and Digitone Keys (core-dn1 3.3,
 * seq-events), on the TRIG key's second page, Chords, which this mod draws itself after the Digitone
 * II's keyboard setup screen:
 *
 *   +-------------------------+ BASS
 *   | CHORDS T1               | OCT
 *   | [keyboard: the chord,   |  -1
 *   |  the center's arrow]    | (D)
 *   | CNTR C5 (A)    INV (B) |
 *   +-------------------------+
 *    MODE   ROOT   SCALE  SHAPE
 *   CENTER   C5     DOR   1-3-5
 *    (E)    (F)     (G)    (H)
 *
 * A  CENTER POSITION: AUTO (the played note) or C3-C8 (MIDI 36-96); the track's, saved with the project
 * B  CENTER POSITION to the next or the last inversion of the chord (the Digitone II's [+]/[-])
 * D  BASS OCT 0 to -3 (with MODE on: "--" with it off), E  MODE (OFF, CHORD ROOT, CENTER, SPREAD), H  SHAPE (TRIG 1-8 too): the sound's
 * F  ROOT and G  SCALE: the track's keyboard ROOT and SCALE themselves
 *
 * With MODE on, each single-note trig on the track plays the chord from ROOT and SCALE. Trigs that
 * hold a chord play as stored. The sequencer's trigs only for now, not the TRIG keys played live. */
#include "digitone-mk1/core3.h"
#include "digitone-mk1/seq.h"
#include "chordmath.h"

#define ID_MODE     182                 /* the parameter ids, claimed as param:182 to param:184 */
#define ID_SHAP     183
#define ID_BASS     184
#define SLOT_MODE   26                  /* the sound slots their values live in (no stock use) */
#define SLOT_SHAP   65
#define SLOT_BASS   0                   /* also where an LFO with no destination points: harmless here,
                                         * this mod reads the kit, never the render's words */
#define MODES       4                   /* OFF, ROOT, CENTER, SPREAD */
#define BASSES      4                   /* 0, -1, -2, -3 */
#define CENT_MAX    61                  /* CENTER: 0 AUTO (the played note), 1-61 C3-C8 */
#define CENT_BASE   35                  /* so 1 is MIDI 36, C3 as the OS names it */
#define ROOT_MAX    115                 /* the stock setter's limits (1.43: 0x40035274, 0x4003553a) */
#define SCALE_MAX   34

#define KEY_TRIG1   26
#define EV_KEY(ev)   (*(volatile uint32_t *)((char *)(ev) + 12))
#define EV_FLAGS(ev) (*(volatile uint32_t *)((char *)(ev) + 16))
#define EV_DELTA(ev) (*(volatile int32_t *)((char *)(ev) + 16))
#define COUNTS      4                   /* encoder counts a detent */

/* The pattern's lock tables (as fxkit-dn1 reads them): a track's 10319 bytes, a step's row 160, a
 * slot's lock a big-endian s16 at 2 * slot, -1 for none. 10319 is odd, so read them by bytes. */
#define LOCKS        0x1e80
#define TRACK_LOCKS  10319
#define STEP_LOCKS   160

/* ---- state ------------------------------------------------------------------------------------ */
/* per track: CENTER, 0 AUTO; the project data below, which core copies a long word at a time */
static uint8_t ch_center[4] __attribute__((aligned(4)));
/* the last chord each track played, -1 first: none yet (0 would be a note, C-1) */
static volatile int8_t ch_last[SQ_AUDIO][CH_MAX_TONES] = { { -1 }, { -1 }, { -1 }, { -1 } };
static void *ch_brain;                  /* the key dispatcher's, from the first key: ev_draw has none */
static int32_t ch_acc[8];               /* encoder counts short of a detent */
static uint32_t ch_drawn;               /* what the page last showed, to redraw on a change */

static void ch_loaded(int32_t found)
{
    int i;
    if (!found)
        for (i = 0; i < 4; i++)
            ch_center[i] = 0;
}

struct core_projdata chords_projdata = { 0x43485244u /* "CHRD" */, 4, ch_center, ch_loaded };

/* ---- the chords ------------------------------------------------------------------------------- */
/* A knob's value as this step plays it, 8.8 >> 8: its p-lock, else the sound's. Slot 0's lock is not
 * read: fxkit-dn1 treats slot 0 in a lock row as no slot. */
static int32_t ch_value(int32_t track, int32_t step, int32_t slot, int32_t hi)
{
    uint8_t *pat = fw_pattern, *kit = fw_kit;
    int32_t v = -1;
    if (slot > 0 && pat && step >= 0 && step < SQ_STEPS) {
        const volatile uint8_t *p = pat + LOCKS + TRACK_LOCKS * track + STEP_LOCKS * step + 2 * slot;
        v = (int16_t)(p[0] << 8 | p[1]);
    }
    if (v < 0)
        v = kit ? FW_SOUND_SLOT(kit, track, slot) : 0;
    v >>= 8;
    return v < 0 ? 0 : v > hi ? hi : v;
}

/* Interrupt level, after every note generator. */
void chords_notes(int32_t track, int32_t step, int8_t notes[SQ_NOTES])
{
    struct sq_track info;
    int8_t out[CH_MAX_TONES];
    int32_t i, n, mode, center, c;

    if (track < 0 || track >= SQ_AUDIO || notes[0] < 0)
        return;
    mode = ch_value(track, step, SLOT_MODE, MODES - 1);
    n = 0;
    if (mode) {
        for (i = 1; i < SQ_NOTES && !notes[i]; i++)     /* a stored chord plays as stored */
            ;
        if (i == SQ_NOTES && sq_track_info(track, &info, sizeof info) >= 0 &&
            info.root != SQ_ROOT_NONE) {
            c = ch_center[track];
            center = c ? CENT_BASE + c : notes[0];
            n = ch_chord(notes[0], info.root, info.scale, ch_value(track, step, SLOT_SHAP, CH_TYPES - 1),
                         mode - 1, center, ch_value(track, step, SLOT_BASS, BASSES - 1), out);
            if (n > 0)
                for (i = 0; i < SQ_NOTES; i++)
                    notes[i] = i < n ? out[i] : 0;
        }
    }
    for (i = 0; i < CH_MAX_TONES; i++)  /* for the page's keyboard */
        ch_last[track][i] = notes[i];
}

/* ---- the knobs -------------------------------------------------------------------------------- */
static void ch_text(char *buf, const char *s)
{
    while ((*buf++ = *s++))
        ;
}

static const char *const ch_mode_names[MODES] = { "OFF", "ROOT", "CENTER", "SPREAD" };
/* The Digitone II's own names and order (its OS 1.12: 0x402a1348, 0x401d5808). */
static const char *const ch_step_names[CH_SHAPES] = {
    "1-3-5", "1-2-5", "1-3", "1-4", "1-5", "1-4-5", "1-3-5-7", "1-4-7"
};
static const char *const ch_semi_names[CH_TYPES] = {
    "MAJ", "MIN", "7TH", "MIN7", "MAJ7", "DIM", "DIM7", "SUS2", "SUS4", "6", "MIN6", "ADD9"
};
/* The OS's own short scale names (1.43: the name table at 0x40241cd0, entries 36-71), SCALE -1 first. */
static const char *const ch_scale_names[SCALE_MAX + 2] = {
    "CHRO", "ION", "DOR", "PHRY", "LYD", "MIXO", "AEOL", "LOCR", "PNMI", "PNMA", "MLMI", "HRMI",
    "WHTN", "BLUE", "COMB", "PERS", "IWAT", "INSN", "HIRJ", "PELG", "PHYD", "WHDI", "HWDI", "SPAN",
    "MAJL", "SPRL", "DOR2", "LYAU", "LYDO", "DBLH", "LY26", "ULTR", "HUNG", "ORIE", "IO25", "LO37"
};
static const char ch_pcs[] = "C C#D D#E F F#G G#A A#B ";

static int32_t ch_clamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static const char *ch_shape_name(int32_t v, int32_t scale)
{
    v = ch_clamp(v, 0, ch_shape_count(scale) - 1);
    return scale < 0 ? ch_semi_names[v] : ch_step_names[v];
}

/* As the OS names notes (1.43: 0x40116d54, "%s%d" with the octave note / 12): MIDI 60 = C5. note:
 * 0-127, so the octave is 0-10. */
static void ch_note_name(char *buf, int32_t note)
{
    int32_t o = note / 12;
    *buf++ = ch_pcs[2 * (note % 12)];
    if (ch_pcs[2 * (note % 12) + 1] != ' ')
        *buf++ = '#';
    if (o >= 10) {
        *buf++ = '1';
        o -= 10;
    }
    *buf++ = (char)('0' + o);
    *buf = 0;
}

/* The firmware's formatters: at most 4 characters, as the stock values (OFF, 127, -63), since the
 * size of the buffer the firmware gives them is not known. The page draws its own, longer names. */
static const char *const ch_mode_short[MODES] = { "OFF", "ROOT", "CNTR", "SPRD" };
static const char *const ch_step_short[CH_SHAPES] = {
    "135", "125", "13", "14", "15", "145", "1357", "147"
};

static void ch_fmt_mode(int32_t value, char *buf)
{
    ch_text(buf, ch_mode_short[ch_clamp(value >> 8, 0, MODES - 1)]);
}

static void ch_fmt_shape(int32_t value, char *buf)
{
    uint8_t *pat = fw_pattern;
    int32_t t = fw_active_track, v = value >> 8;
    int chromatic = pat && t >= 0 && t < SQ_AUDIO && FW_TRACK_SCALE(FW_TRACK(pat, t)) < 0;
    v = ch_clamp(v, 0, (chromatic ? CH_TYPES : CH_SHAPES) - 1);
    ch_text(buf, chromatic ? ch_semi_names[v] : ch_step_short[v]);
}

static void ch_fmt_bass(int32_t value, char *buf)
{
    int32_t v = ch_clamp(value >> 8, 0, BASSES - 1);
    if (v)
        *buf++ = '-';
    *buf++ = (char)('0' + v);
    *buf = 0;
}

/* The three stay parameters of the sound (saved, copied and sent with it); the page draws them. */
const struct core_param chords_mode = {
    ID_MODE, 1, SLOT_MODE, 0, (MODES - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Chord mode", fw_str_amp, "MODE", ch_fmt_mode, fw_str_empty, 0
};
const struct core_param chords_shap = {
    ID_SHAP, 1, SLOT_SHAP, 0, (CH_TYPES - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Chord shape", fw_str_amp, "SHAP", ch_fmt_shape, fw_str_empty, 0
};
const struct core_param chords_bass = {
    ID_BASS, 1, SLOT_BASS, 0, (BASSES - 1) << 8, 0, 0, -1, -1, -1, 0,
    "Bass octave", fw_str_amp, "BASS", ch_fmt_bass, fw_str_empty, 0
};

/* The TRIG key's second page, after page 1, the synth tracks' Trig Parameters (a kind-9 page like
 * the sound pages: 1.43 builds it at 0x4016ac30). The MIDI tracks' TRIG page, 19, has none. No stock
 * knobs: the mod draws it and turns its knobs. Core writes view and pos. */
struct core_page chords_page = {
    27, 1, "CHRD", "Chords", { 0, 0, 0, 0, 0, 0, 0, 0 }, 9, 0, 0
};

/* ---- the page's state, as it shows ------------------------------------------------------------ */
struct ch_view {
    int32_t track, mode, shape, bass, center, root, scale;
    int8_t last[CH_MAX_TONES];
};

static int ch_shown(void)
{
    return ch_brain && core_page_shown(ch_brain, &chords_page);
}

static int ch_view(struct ch_view *v)
{
    uint8_t *pat = fw_pattern, *kit = fw_kit, *trk;
    int32_t t = fw_active_track, i;
    if (!pat || !kit || t < 0 || t >= SQ_AUDIO)
        return 0;
    trk = FW_TRACK(pat, t);
    v->track = t;
    v->mode = ch_clamp(FW_SOUND_SLOT(kit, t, SLOT_MODE) >> 8, 0, MODES - 1);
    v->scale = FW_TRACK_SCALE(trk);
    v->shape = ch_clamp(FW_SOUND_SLOT(kit, t, SLOT_SHAP) >> 8, 0, ch_shape_count(v->scale) - 1);
    v->bass = ch_clamp(FW_SOUND_SLOT(kit, t, SLOT_BASS) >> 8, 0, BASSES - 1);
    v->center = ch_center[t];
    v->root = ch_clamp(FW_TRACK_ROOT(trk), 0, 127);
    for (i = 0; i < CH_MAX_TONES; i++)
        v->last[i] = ch_last[t][i];
    return 1;
}

/* ---- keys and knobs (the UI task) -------------------------------------------------------------- */
/* While the page shows, TRIG 1-8 pick the active track's shape. A key's later events (its repeat,
 * its release) go where its press went: to the mod only when the mod took the press, so the stock
 * never sees half a press (a trig held on the note page, then TRIG to this page, then let go; or
 * FUNC let go before the trig). Every key also gives us the brain for ev_draw. */
static uint8_t ch_taken;                /* bit k: the mod took TRIG k+1's press */

int32_t chords_key(void *brain, void *ev)
{
    uint32_t key = EV_KEY(ev), fl = EV_FLAGS(ev), bit;
    int32_t t = fw_active_track;

    ch_brain = brain;
    if (key < KEY_TRIG1 || key >= KEY_TRIG1 + CH_SHAPES)
        return 0;
    bit = 1u << (key - KEY_TRIG1);
    if (!(fl & 1) || (fl & 8)) {        /* a release (0x10, or 0 after another key went down) or a
                                           hold (9): as its press went */
        if (!(ch_taken & bit))
            return 0;
        if (!(fl & 1))
            ch_taken &= (uint8_t)~bit;
        return 1;
    }
    if ((fl & 2) || t < 0 || t >= SQ_AUDIO || !core_page_shown(brain, &chords_page)) {
        ch_taken &= (uint8_t)~bit;      /* FUNC + TRIG, and every other page, stay stock */
        return 0;
    }
    ch_taken |= (uint8_t)bit;
    core_sound_set(t, SLOT_SHAP, (int32_t)(key - KEY_TRIG1) << 8);
    return 1;
}

/* The note classes of the chord knob B turns through: the last one the track played, else what its ROOT
 * would play in ROOT mode. 0: none. */
static uint32_t ch_pcs_of(const struct ch_view *v)
{
    int8_t out[CH_MAX_TONES];
    const int8_t *n = v->last;
    uint32_t m = 0;
    int32_t i, k = CH_MAX_TONES;
    if (n[0] < 0) {
        k = ch_chord(v->root, v->root, v->scale, v->shape, CH_ROOT, 0, 0, out);
        n = out;
        if (k <= 0)
            return 1u << (v->root % 12);
    }
    for (i = 0; i < k; i++)
        if (!i || n[i])
            m |= 1u << ((n[0] + (i ? n[i] : 0)) % 12);
    return m;
}

/* While the page shows, its knobs are ours: A CENTER, B its inversion steps, D BASS OCT, E MODE,
 * F ROOT, G SCALE, H SHAPE. */
int32_t chords_enc(void *brain, void *ev)
{
    uint32_t e = EV_KEY(ev);
    uint8_t *trk;
    struct ch_view v;
    int32_t d, i;

    ch_brain = brain;
    if (e < 1 || e > 8 || !core_page_shown(brain, &chords_page) || !ch_view(&v))
        return 0;
    i = (int32_t)e - 1;
    ch_acc[i] += EV_DELTA(ev);
    d = ch_acc[i] / COUNTS;
    ch_acc[i] -= d * COUNTS;
    if (!d)
        return 1;
    trk = FW_TRACK(fw_pattern, v.track);
    switch (e) {
    case 1:
        ch_center[v.track] = (uint8_t)ch_clamp(v.center + d, 0, CENT_MAX);
        break;
    case 2: {                           /* from AUTO, the steps start at ROOT */
        uint32_t pcs = ch_pcs_of(&v);
        int32_t c = v.center ? CENT_BASE + v.center : v.root;
        for (; d > 0; d--)
            c = ch_inversion(pcs, c, 1);
        for (; d < 0; d++)
            c = ch_inversion(pcs, c, -1);
        ch_center[v.track] = (uint8_t)(ch_clamp(c, CENT_BASE + 1, CENT_BASE + CENT_MAX) - CENT_BASE);
        break;
    }
    case 4:
        if (!v.mode)                    /* BASS OCT is the chord modes' */
            break;
        core_sound_set(v.track, SLOT_BASS, ch_clamp(v.bass + d, 0, BASSES - 1) << 8);
        break;
    case 5:
        core_sound_set(v.track, SLOT_MODE, ch_clamp(v.mode + d, 0, MODES - 1) << 8);
        break;
    case 6:
        trk[0x3cb] = (uint8_t)ch_clamp(v.root + d, 0, ROOT_MAX);
        break;
    case 7:
        trk[0x3ce] = (uint8_t)(int8_t)ch_clamp(v.scale + d, -1, SCALE_MAX);
        break;
    case 8:
        core_sound_set(v.track, SLOT_SHAP, ch_clamp(v.shape + d, 0, ch_shape_count(v.scale) - 1) << 8);
        break;
    }
    return 1;
}

/* 30 times a second: redraw when what the page shows has changed. */
void chords_tick(void *ctrl)
{
    struct ch_view v;
    uint32_t s = 0, i;
    if (!ch_shown() || !ch_view(&v))
        return;
    s = (uint32_t)(v.track | v.mode << 2 | v.shape << 4 | v.bass << 7 | v.center << 9 |
                   v.root << 15 | (v.scale + 1) << 22) ^ 0x5a5a5a5au;
    for (i = 0; i < CH_MAX_TONES; i++)
        s = s * 31u + (uint8_t)v.last[i];
    if (s != ch_drawn) {
        ch_drawn = s;
        *((volatile uint8_t *)ctrl + 0x20) = 1;
    }
}

/* ---- the screen: 128 x 64, y = 0 the bottom row ----------------------------------------------- */
#define FONT5     ((const void *)fw_font5)          /* 3 x 5: the stock value boxes' */
#define FONT_LBL  ((const void *)fw_font_label)     /* 4 x 5 capitals: the stock labels' */
#define FONT_TTL  ((const void *)fw_font_title)     /* the stock title bar's */

static void ch_rect(void *bmp, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c)
{
    fw_fillrect(bmp, x0, y0, x1, y1, c);
}

static int32_t ch_len(const char *s)
{
    int32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void ch_small(void *bmp, int32_t cx, int32_t y, const char *s)    /* font5, centred on cx */
{
    fw_textf(bmp, FONT5, cx - (4 * ch_len(s) - 1) / 2, y, -1, "%s", s);
}

static void ch_label(void *bmp, int32_t cx, int32_t y, const char *s)    /* label font, centred */
{
    fw_textf(bmp, FONT_LBL, cx - (5 * ch_len(s) - 1) / 2, y, -1, "%s", s);
}

/* A 3 x 5 font drawn at twice the size for the big values: rows top first, bit 4 the left pixel. */
static const struct { char c; uint8_t r[5]; } ch_glyphs[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 7, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}}, {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}}, {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'C', {3, 4, 4, 4, 3}}, {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}}, {'J', {1, 1, 1, 5, 2}},
    {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}}, {'M', {5, 7, 7, 5, 5}}, {'N', {6, 5, 5, 5, 5}},
    {'O', {2, 5, 5, 5, 2}}, {'P', {6, 5, 6, 4, 4}}, {'Q', {2, 5, 5, 6, 3}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}},
    {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}}, {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
    {'#', {5, 7, 5, 7, 5}}, {'-', {0, 0, 7, 0, 0}},
};

static void ch_big(void *bmp, int32_t cx, int32_t y, const char *s)     /* 6 x 10 a char, centred */
{
    int32_t x = cx - (8 * ch_len(s) - 2) / 2, r, c;
    uint32_t g;
    for (; *s; s++, x += 8) {
        for (g = 0; g < sizeof ch_glyphs / sizeof ch_glyphs[0] && ch_glyphs[g].c != *s; g++)
            ;
        if (g == sizeof ch_glyphs / sizeof ch_glyphs[0])
            continue;                   /* a space, or a char it has no glyph for */
        for (r = 0; r < 5; r++)
            for (c = 0; c < 3; c++)
                if (ch_glyphs[g].r[r] & (4 >> c))
                    ch_rect(bmp, x + 2 * c, y + 2 * (4 - r), x + 2 * c + 1, y + 2 * (4 - r) + 1, 1);
    }
}

/* A keyboard of two octaves from base (a C): 14 white keys 6 wide at x0, from y0 up 15 rows. The
 * chord's notes get a dot; CENTER (a MIDI note, or -1) an arrow under its key. */
static const int8_t ch_white_of[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };

static int32_t ch_key_x(int32_t x0, int32_t base, int32_t note, int32_t *black)
{
    int32_t k = note - base, o = k / 12, pc = k % 12, w = ch_white_of[pc];
    *black = w < 0;
    if (w < 0)                          /* a black key sits on the line after the white below it */
        return x0 + 6 * (7 * o + ch_white_of[pc - 1]) + 6;
    return x0 + 6 * (7 * o + w) + 3;
}

static void ch_keyboard(void *bmp, int32_t x0, int32_t y0, int32_t base, const int8_t *last,
                        int32_t center)
{
    int32_t i, k, x, black, n;
    ch_rect(bmp, x0, y0, x0 + 84, y0 + 14, 1);          /* the white keys, then their gaps */
    for (i = 1; i < 14; i++)
        ch_rect(bmp, x0 + 6 * i, y0, x0 + 6 * i, y0 + 14, 0);
    for (k = 0; k < 24; k++)            /* the black keys: dark, over the top of the white ones */
        if (ch_white_of[k % 12] < 0) {
            x = ch_key_x(x0, base, base + k, &black);
            ch_rect(bmp, x - 2, y0 + 5, x + 2, y0 + 14, 0);
        }
    if (last[0] >= 0) {                 /* the chord's notes */
        for (i = 0; i < CH_MAX_TONES; i++) {
            if (i && !last[i])
                continue;
            n = last[0] + (i ? last[i] : 0);
            if (n < base || n >= base + 24)
                continue;
            x = ch_key_x(x0, base, n, &black);
            if (black)                  /* a lit dot on a dark key, a dark one on a lit key */
                ch_rect(bmp, x - 1, y0 + 7, x, y0 + 8, 1);
            else
                ch_rect(bmp, x - 1, y0 + 1, x, y0 + 2, 0);
        }
    }
    if (center >= base && center < base + 24) {         /* the center's arrow, under its key */
        x = ch_key_x(x0, base, center, &black);
        ch_rect(bmp, x, y0 - 2, x, y0 - 2, 1);
        ch_rect(bmp, x - 1, y0 - 3, x + 1, y0 - 3, 1);
        ch_rect(bmp, x - 2, y0 - 4, x + 2, y0 - 4, 1);
    }
}

void chords_draw(void *bmp, void *ctrl)
{
    struct ch_view v;
    char buf[8];
    int32_t lo, base, i, center;
    static const char letters[] = "EFGH";
    (void)ctrl;
    if (!ch_shown() || !ch_view(&v))
        return;
    ch_rect(bmp, 0, 0, 127, 63, 0);

    /* the keyboard box: its title bar, the keyboard, CENTER */
    fw_framerect(bmp, 0, 24, 88, 63, 1);
    fw_textf(bmp, FONT_TTL, 3, 56, -1, "CHORDS T%d", (int)v.track + 1);
    ch_rect(bmp, 1, 55, 87, 62, -1);
    center = v.center ? CENT_BASE + v.center : -1;
    lo = v.last[0] >= 0 ? v.last[0] : v.root;
    if (center >= 0 && center < lo)
        lo = center;
    base = 12 * (lo / 12);
    if (base > 104)
        base = 104 - 104 % 12;
    ch_keyboard(bmp, 2, 37, base, (const int8_t *)v.last, center);
    if (center >= 0)
        ch_note_name(buf, center);
    else
        ch_text(buf, "AUTO");
    fw_textf(bmp, FONT5, 3, 27, -1, "CNTR %s (A)", buf);
    fw_textf(bmp, FONT5, 59, 27, -1, "INV (B)");

    /* BASS OCT, beside it */
    ch_label(bmp, 108, 56, "BASS");
    ch_label(bmp, 108, 49, "OCT");
    ch_fmt_bass(v.bass << 8, buf);
    ch_big(bmp, 108, 35, v.mode ? buf : "--");
    ch_small(bmp, 108, 27, "(D)");

    /* MODE, ROOT, SCALE, SHAPE */
    ch_label(bmp, 16, 18, "MODE");
    if (v.mode) {
        ch_small(bmp, 16, 12, "CHORD");
        ch_small(bmp, 16, 6, ch_mode_names[v.mode]);
    } else {
        ch_big(bmp, 16, 7, "OFF");
    }
    ch_label(bmp, 48, 18, "ROOT");
    ch_note_name(buf, v.root);
    ch_big(bmp, 48, 7, buf);
    ch_label(bmp, 80, 18, "SCALE");
    ch_big(bmp, 80, 7, ch_scale_names[ch_clamp(v.scale + 1, 0, SCALE_MAX + 1)]);
    ch_label(bmp, 112, 18, "SHAPE");
    ch_small(bmp, 112, 9, ch_shape_name(v.shape, v.scale));
    for (i = 0; i < 4; i++) {
        buf[0] = '(';
        buf[1] = letters[i];
        buf[2] = ')';
        buf[3] = 0;
        ch_small(bmp, 16 + 32 * i, 0, buf);
    }
}

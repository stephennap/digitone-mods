/* SPDX-License-Identifier: GPL-2.0-or-later */
/* chprobe: a probe for live chords on the Digitone mk1 (with CHORDS, chords-dn1). Not for use.
 *
 * At the render's two live-note sites (probe.s) it logs each event, and on every synth track it adds
 * the note's fifth (note + 7) as a second note, to see whether the TRIG keys then play two voices
 * and whether letting a key go releases both. Each block it logs the voices that started (and their
 * notes) and the ones released. While the Chords page shows, its top part is the log (the bottom
 * row, CHORDS' own, stays), newest at the top:
 *   A T1 K0 F00080000 R60 N60 C2   path (A, B), track, kind (+0), flags (+0x28), +0x40, note, count
 *   C T1 K2 F00030601 R100 N60 C2  path C (the keyboard's and MIDI in's event builder): track, the
 *                                  event's +0xc, the request's flags (+0x10), its +8, note, count
 *   V +03 -00 60 67                 voices started (mask) and released (mask), the started notes */
#include "digitone-mk1/core3.h"

extern struct core_page chords_page;    /* CHORDS' page: the log shows there */

#define LINES       6                   /* the log's lines, over CHORDS' keyboard box and BASS OCT */
#define EV_KEY(ev)  (*(volatile uint32_t *)((char *)(ev) + 12))

struct entry {
    uint8_t type, path, count, n;       /* type 0 a note event, 1 voices */
    int8_t track, note;
    uint8_t on, off;
    uint32_t kind, flags;
    int32_t raw;
    int8_t notes[8];
};

static struct entry log_ring[LINES];
static volatile uint32_t log_seq;       /* entries written, ever */
static uint32_t log_drawn;
static void *probe_brain;

static struct entry *log_next(void)     /* interrupt level only */
{
    struct entry *e = &log_ring[log_seq % LINES];
    log_seq++;
    return e;
}

/* The render, interrupt level: the live note is in the event as its only note. */
void chprobe_live(uint8_t *ev, int32_t path)
{
    struct entry *e = log_next();
    int32_t track = *(volatile int32_t *)(ev + 8), note = (int8_t)ev[0x18];
    if (track >= 0 && track < 4 && note >= 0 && note + 7 <= 127) {      /* every synth track */
        ev[0x19] = (uint8_t)(note + 7);
        *(volatile uint32_t *)(ev + 0x14) = 2;
    }
    e->type = 0;
    e->path = (uint8_t)path;
    e->track = (int8_t)track;
    e->note = (int8_t)note;
    e->kind = *(volatile uint32_t *)ev;
    e->flags = *(volatile uint32_t *)(ev + 0x28);
    e->raw = *(volatile int32_t *)(ev + 0x40);
    e->count = (uint8_t)*(volatile uint32_t *)(ev + 0x14);
}

/* The builder of a played note's event (the keyboard, MIDI in): the note is its only one so far. */
void chprobe_req(uint8_t *ev, const uint8_t *req)
{
    struct entry *e;
    int32_t track = *(volatile int32_t *)(ev + 8), note = (int8_t)ev[0x18];
    uint32_t sr;

    if (track >= 0 && track < 4 && note >= 0 && note + 7 <= 127) {      /* every synth track */
        ev[0x19] = (uint8_t)(note + 7);
        *(volatile uint32_t *)(ev + 0x14) = 2;
    }
    __asm__ volatile ("move.w %%sr, %0; move.w #0x2700, %%sr" : "=d" (sr));   /* the render logs too */
    e = log_next();
    e->type = 0;
    e->path = 2;
    e->track = (int8_t)track;
    e->note = (int8_t)note;
    e->kind = *(volatile uint32_t *)(ev + 0xc);
    e->flags = *(const volatile uint32_t *)(req + 0x10);
    e->raw = *(const volatile int32_t *)(req + 8);
    e->count = (uint8_t)*(volatile uint32_t *)(ev + 0x14);
    __asm__ volatile ("move.w %0, %%sr" : : "d" (sr));
}

/* Each audio block, after the render: the voices it started and released. */
void chprobe_out(void)
{
    uint32_t on = fw_gate_on & 0xff, off = fw_gate_off & 0xff, v;
    struct entry *e;
    if (!on && !off)
        return;
    e = log_next();
    e->type = 1;
    e->on = (uint8_t)on;
    e->off = (uint8_t)off;
    e->n = 0;
    for (v = 0; v < 8; v++)
        if (on & (1u << v))
            e->notes[e->n++] = (int8_t)(fw_voice_pitch[v] >> 16);
}

/* ---- the UI task ------------------------------------------------------------------------------ */
int32_t chprobe_key(void *brain, void *ev)
{
    (void)ev;
    probe_brain = brain;
    return 0;
}

static int shown(void)
{
    return probe_brain && core_page_shown(probe_brain, &chords_page);
}

void chprobe_tick(void *ctrl)
{
    uint32_t s = log_seq;
    if (shown() && s != log_drawn) {
        log_drawn = s;
        *((volatile uint8_t *)ctrl + 0x20) = 1;
    }
}

static char *put_hex(char *p, uint32_t v, int digits)
{
    int i;
    for (i = digits - 1; i >= 0; i--)
        *p++ = "0123456789ABCDEF"[(v >> (4 * i)) & 15];
    return p;
}

static char *put_dec(char *p, int32_t v)
{
    char t[12];
    int n = 0;
    if (v < 0) {
        *p++ = '-';
        v = -v;
    }
    do
        t[n++] = (char)('0' + v % 10);
    while ((v /= 10) && n < 11);
    while (n)
        *p++ = t[--n];
    return p;
}

static char *put_str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

static void line(char *p, const struct entry *e)
{
    int i;
    if (e->type == 0) {
        *p++ = (char)('A' + e->path);
        p = put_str(p, " T");
        p = put_dec(p, e->track + 1);
        p = put_str(p, " K");
        p = put_dec(p, (int32_t)e->kind);
        p = put_str(p, " F");
        p = put_hex(p, e->flags, 8);
        p = put_str(p, " R");
        p = put_dec(p, e->raw);
        p = put_str(p, " N");
        p = put_dec(p, e->note);
        p = put_str(p, " C");
        p = put_dec(p, e->count);
    } else {
        p = put_str(p, "V +");
        p = put_hex(p, e->on, 2);
        p = put_str(p, " -");
        p = put_hex(p, e->off, 2);
        for (i = 0; i < e->n && i < 4; i++) {
            *p++ = ' ';
            p = put_dec(p, e->notes[i]);
        }
    }
    *p = 0;
}

void chprobe_draw(void *bmp, void *ctrl)
{
    char buf[40];
    uint32_t s = log_seq, i;
    (void)ctrl;
    if (!shown())
        return;
    fw_fillrect(bmp, 0, 24, 127, 63, 0);    /* the top; CHORDS' MODE, ROOT, SCALE, SHAPE stay */
    fw_fillrect(bmp, 0, 23, 127, 23, 1);
    for (i = 0; i < LINES && i < s; i++) {
        struct entry e;                 /* a copy, by bytes (no memcpy): the render may write more */
        const volatile uint8_t *from = (const volatile uint8_t *)&log_ring[(s - 1 - i) % LINES];
        uint8_t *to = (uint8_t *)&e;
        uint32_t k;
        for (k = 0; k < sizeof e; k++)
            to[k] = from[k];
        line(buf, &e);
        fw_textf(bmp, fw_font5, 1, 58 - 6 * (int32_t)i, -1, "%s", buf);
    }
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CHORDS: RETRIG, the TRIG key's third page (included at the end of chords.c, whose page state, lock
 * rows and drawing it shares).
 *
 *   +---------------------------------+
 *   | RETRIG TRK 1  TRIG 5            |
 *   |  |  |  |  |  |  |  |  |         |   the hits over LEN, as tall as their velocity
 *   +---------------------------------+
 *     RATE    LEN     VEL
 *     1/16     1     -64
 *     (E)     (F)    (G)
 *
 * Per trig, as the Digitakt's retrig: hold a trig and turn E RATE (OFF, 1/1 to 1/80 of a bar), F LEN
 * (1/8 of a step to 32 steps, or INF: until the track's next trig), G VEL (the velocity's fade over
 * LEN: -128 fades out to nothing, 112 fades in from an eighth). The trig plays, then its event (its
 * chord, velocity, length and p-locks) again every 1/RATE of a bar.
 *
 * Storage: no sound slot of its own (the mk1 has none left). A trig's RATE is the low byte of its MODE
 * p-lock, its LEN (low nibble) and VEL (high nibble) the low byte of its BASS p-lock: 8.8 values whose
 * high byte is the knob's, which no stock code and no other part of this mod reads the low byte of.
 * A trig with retrig and no MODE (or BASS) lock gets a high byte of 0x80: a negative value that is
 * not -1 (none), which the lock rows' count (the stock's 0x40023ed2 and ch_lock_set) and ch_value
 * read as no lock, so retrig never fixes a trig's MODE or BASS OCT. Copied, pasted and cleared with
 * the trig, as its locks are.
 *
 * Playback: seq-events' fire tells which track's next queued event is a retrig trig (rt_arm); core's
 * ev_note_queue hands over that event as it is queued, with its time: the mod keeps a copy (and a
 * reference to its p-lock list, as the firmware's own event copy, 0x400ffde8, takes one). The render
 * (ev_render_out) queues a fresh copy for each hit half a step before it plays, with the faded
 * velocity (+0x10, the velocity << 8 in each half), through the firmware's own queue (fw_ev_alloc,
 * fw_ev_queue). Checked on a unit (CH17). A newer note on the track
 * ends a retrig at its time; STOP ends them all. */

#define RT_RATES     18                 /* OFF, then 1/1 .. 1/80 */
#define RT_LENS      16
#define RT_LEN_INF   15
#define RT_LEN_ONE   5                  /* stored as (index - 5) & 15: a fresh lock's 0 is one step */
#define RT_BAR       86400000u          /* 16 steps of 5,400,000 timeline units */
#define RT_EIGHTH    675000u            /* an eighth of a step */
#define RT_AHEAD     2700000            /* queue a hit this far (half a step) before it plays */
#define RT_FAR       172800000          /* two bars: a hit further than this is a jump of the timeline */
#define RT_SLOTS     8                  /* retrigs playing at once, all tracks */
#define RT_PER_BLOCK 4                  /* hits one retrig queues in one render block, at most */
#define RT_NONE_HI   0x80               /* a lock row's high byte: retrig only, no lock of the knob */
#define EV_LONGS     (0x48 / 4)

static const uint8_t rt_rates[RT_RATES] = { 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64, 80 };
/* LEN in eighths of a step; 0 INF */
static const uint16_t rt_lens[RT_LENS] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 256, 0 };
static const char *const rt_len_names[RT_LENS] = {
    "1/8", "1/4", "3/8", "1/2", "3/4", "1", "1.5", "2", "3", "4", "6", "8", "12", "16", "32", "INF"
};

struct rt_trig { int32_t rate, len, vel; };     /* rate 0-17, len 0-15 (the index), vel -8..7 */

/* A lock row's byte pair for a slot of a step, or 0. */
static volatile uint8_t *rt_cell(int32_t track, int32_t step, int32_t slot)
{
    uint8_t *pat = fw_pattern;
    if (!pat || track < 0 || track >= SQ_AUDIO || step < 0 || step >= SQ_STEPS)
        return 0;
    return pat + LOCKS + TRACK_LOCKS * track + STEP_LOCKS * step + 2 * slot;
}

/* The low byte retrig keeps in a slot's lock, 0 with no lock. */
static int32_t rt_lo(int32_t track, int32_t step, int32_t slot)
{
    volatile uint8_t *p = rt_cell(track, step, slot);
    return p && p[0] != 0xff ? p[1] : 0;
}

static void rt_read(int32_t track, int32_t step, struct rt_trig *r)
{
    int32_t m = rt_lo(track, step, SLOT_MODE), b = rt_lo(track, step, SLOT_BASS);
    r->rate = m < RT_RATES ? m : 0;
    r->len = (b + RT_LEN_ONE) & 15;
    r->vel = (int8_t)b >> 4;
}

/* Write the low byte of a slot's lock, keeping its knob's value: a lock gets a new low byte; none
 * gets the high byte RT_NONE_HI (no knob lock, not counted) unless lo is 0; RT_NONE_HI with lo 0 is
 * none again. The rows are by bytes (odd addresses on odd tracks). UI task. */
static void rt_set_lo(int32_t track, int32_t step, int32_t slot, int32_t lo)
{
    volatile uint8_t *p = rt_cell(track, step, slot);
    if (!p)
        return;
    if (p[0] == 0xff) {
        if (!lo)
            return;
        p[1] = (uint8_t)lo;
        p[0] = RT_NONE_HI;
        fw_pattern[LOCKS + TRACK_LOCKS * track + LOCK_USED + slot] = 1;
    } else if (p[0] == RT_NONE_HI && !lo) {
        p[0] = 0xff;
        p[1] = 0xff;
    } else {
        p[1] = (uint8_t)lo;
    }
}

static void rt_write(int32_t track, int32_t step, const struct rt_trig *r)
{
    rt_set_lo(track, step, SLOT_MODE, r->rate);
    rt_set_lo(track, step, SLOT_BASS, ((r->len - RT_LEN_ONE) & 15) | (r->vel & 15) << 4);
}

/* Velocity v0 at a fraction p (0-256) of LEN, by the fade f (-8..7, eighths). */
static int32_t rt_vel(int32_t v0, int32_t f, int32_t p)
{
    int32_t v = f < 0 ? v0 * (2048 + f * p) / 2048 : v0 * (2048 - f * (256 - p)) / 2048;
    return v < 1 ? 1 : v > 127 ? 127 : v;
}

/* ---- playback --------------------------------------------------------------------------------- */
struct rt_slot {
    volatile uint32_t active;           /* the render plays it; set last, cleared last */
    int32_t track;
    uint32_t t0, next, interval, len;   /* timeline units; len 0: INF */
    uint32_t stop;                      /* with cut: no hit at or after this time */
    int32_t cut, f, v0, kind;
    uint8_t *locks;                     /* the copy's p-lock list, one reference the mod's */
    uint32_t ev[EV_LONGS];
};

static struct rt_slot rt_slots[RT_SLOTS];
/* What the fire of each track's step said, for the event ev_note_queue hands over next. */
static struct { volatile int32_t fired, rate, len, f, v0; } rt_arm[SQ_AUDIO];
static volatile int32_t rt_own;          /* the mod is queueing: its own events are not trigs */
static int32_t rt_was_playing;
uint32_t rt_hits, rt_short;              /* hits queued; hits skipped, the pool or the times empty */

static uint32_t rt_mask(void)
{
    uint32_t sr;
    __asm__ volatile("move.w %%sr, %0\n\tmove.w #0x2700, %%sr" : "=d"(sr) : : "memory");
    return sr;
}

static void rt_unmask(uint32_t sr)
{
    __asm__ volatile("move.w %0, %%sr" : : "d"(sr) : "memory");
}

/* Give back a p-lock list's reference, as the firmware's 0x400ffd50 does. Masked. */
static void rt_unref(uint8_t *l)
{
    volatile uint32_t *w = (volatile uint32_t *)l;
    if (!l)
        return;
    if (--w[0] == 0) {
        w[4] = fw_locks_free;
        fw_locks_free = (uint32_t)l;
    }
}

static void rt_finish(struct rt_slot *s)     /* masked */
{
    if (s->active) {
        rt_unref(s->locks);
        s->locks = 0;
        s->active = 0;
    }
}

/* seq-events, the fire (interrupt level): the step's velocity. Fading in, the trig itself plays at
 * the fade's start. */
void retrig_vel(int32_t track, int32_t step, int32_t *vel)
{
    struct rt_trig r;
    if (track < 0 || track >= SQ_AUDIO)
        return;
    rt_read(track, step, &r);
    rt_arm[track].v0 = *vel;
    if (r.rate && r.vel > 0)
        *vel = rt_vel(*vel, r.vel, 0);
}

/* seq-events, the fire, after the chord (order 60): a note trig on the track. */
void retrig_notes(int32_t track, int32_t step, int8_t notes[SQ_NOTES])
{
    struct rt_trig r;
    if (track < 0 || track >= SQ_AUDIO || notes[0] < 0)
        return;
    rt_read(track, step, &r);
    rt_arm[track].rate = r.rate;
    rt_arm[track].len = r.len;
    rt_arm[track].f = r.vel;
    rt_arm[track].fired = 1;
}

/* core 4.1, each event as it is queued (masked, no firmware). A step's note event: it ends the
 * track's retrigs at its time and, a retrig trig, starts its own. */
void retrig_queue(void *event, uint32_t time)
{
    const volatile uint32_t *ev = (const volatile uint32_t *)event;
    uint32_t kind = ev[0];
    int32_t track = (int32_t)ev[2], i, free = -1;
    struct rt_slot *s;

    if (rt_own || track < 0 || track >= SQ_AUDIO || !rt_arm[track].fired)
        return;
    if (kind > 1 || (kind == 0 && !(ev[10] & FW_EVENT_NOTE_TRIG)))
        return;                         /* not a note trig's (a lock trig, the arpeggiator) */
    rt_arm[track].fired = 0;
    for (i = 0; i < RT_SLOTS; i++) {
        s = &rt_slots[i];
        if (s->active && s->track == track && (!s->cut || (int32_t)(time - s->stop) < 0)) {
            s->stop = time;
            s->cut = 1;
        }
        if (!s->active && free < 0)
            free = i;
    }
    if (!rt_arm[track].rate)
        return;
    if (free < 0) {                     /* all busy: the oldest gives way */
        uint32_t best = 0;
        for (i = 0; i < RT_SLOTS; i++)
            if (free < 0 || time - rt_slots[i].t0 > best) {
                best = time - rt_slots[i].t0;
                free = i;
            }
        rt_finish(&rt_slots[free]);
    }
    s = &rt_slots[free];
    for (i = 0; i < EV_LONGS; i++)
        s->ev[i] = ev[i];
    s->ev[0x44 / 4] = 0;                /* the queue's link */
    s->kind = (int32_t)kind;
    s->locks = kind == 0 ? (uint8_t *)ev[0x38 / 4] : 0;
    if (s->locks)
        ++*(volatile uint32_t *)s->locks;
    if (kind == 1) {                    /* the first step's: its hits play as fires */
        s->ev[0] = 0;
        s->ev[0x38 / 4] = 0;
    }
    s->track = track;
    s->t0 = time;
    s->interval = RT_BAR / rt_rates[rt_arm[track].rate];
    s->next = time + s->interval;
    s->len = rt_lens[rt_arm[track].len] * RT_EIGHTH;
    s->f = rt_arm[track].f;
    s->v0 = rt_arm[track].v0 > 0 ? rt_arm[track].v0 : (int32_t)(ev[4] >> 24);
    s->cut = 0;
    s->active = 1;
}

/* One hit of s at s->next. Masked. */
static void rt_hit(struct rt_slot *s)
{
    uint32_t *e, horizon, el, p, v;
    int32_t i;
    if (!fw_nodes_free || !(e = (uint32_t *)fw_ev_alloc())) {
        rt_short++;
        return;
    }
    for (i = 0; i < EV_LONGS; i++)
        e[i] = s->ev[i];
    if (s->locks)
        ++*(volatile uint32_t *)s->locks;
    horizon = s->len ? s->len : 16 * 8 * RT_EIGHTH;     /* INF fades over a bar */
    el = s->next - s->t0;
    p = el >= horizon ? 256 : el / (horizon >> 8);
    v = (uint32_t)rt_vel(s->v0, s->f, (int32_t)(p > 256 ? 256 : p));
    /* +0x10 holds the velocity twice, 8.8 in each half (seen on a unit: 0x64006400 for 100; a copy
     * with the high half 0 plays silent) */
    e[4] = (v << 8 | (s->ev[4] >> 16 & 0xff)) << 16 | v << 8 | (s->ev[4] & 0xff);
    rt_own = 1;
    fw_ev_queue(e, s->next);
    rt_own = 0;
    rt_hits++;
}

/* The render's exit, 1500 times a second: each retrig's hits due within RT_AHEAD. */
void retrig_render(void)
{
    int32_t i, k, d;
    uint32_t sr;
    struct rt_slot *s;

    if (fw_transport != 1) {            /* stopped or paused: what plays ends */
        if (rt_was_playing) {
            sr = rt_mask();
            for (i = 0; i < RT_SLOTS; i++)
                rt_finish(&rt_slots[i]);
            rt_unmask(sr);
        }
        rt_was_playing = 0;
        return;
    }
    rt_was_playing = 1;
    for (i = 0; i < RT_SLOTS; i++) {
        s = &rt_slots[i];
        if (!s->active)
            continue;
        sr = rt_mask();
        for (k = 0; k < RT_PER_BLOCK && s->active; k++) {
            d = (int32_t)(s->next - fw_timeline);
            if (d > RT_FAR || d < -RT_FAR || (s->len && s->next - s->t0 >= s->len) ||
                (s->cut && (int32_t)(s->next - s->stop) >= 0)) {
                rt_finish(s);
                break;
            }
            if (d > RT_AHEAD)
                break;
            if (d >= 0)                 /* a hit already past is skipped, not played late */
                rt_hit(s);
            s->next += s->interval;
        }
        rt_unmask(sr);
    }
}

/* ---- the page ---------------------------------------------------------------------------------- */
struct core_page retrig_page = {
    28, 1, "RTRG", "Retrig", { 0, 0, 0, 0, 0, 0, 0, 0 }, 9, 0, 0
};

static int32_t rt_acc[8];
static uint32_t rt_drawn;

static int rt_shown(void)
{
    return ch_brain && core_page_shown(ch_brain, &retrig_page);
}

/* With a trig held: E RATE, F LEN, G VEL, that trig's. The page's knobs are all the mod's. */
int32_t retrig_enc(void *brain, void *ev)
{
    uint32_t e = EV_KEY(ev);
    struct ch_view v;
    struct rt_trig r;
    int32_t d, i;

    ch_brain = brain;
    if (e < 1 || e > 8 || !core_page_shown(brain, &retrig_page))
        return 0;
    i = (int32_t)e - 1;
    rt_acc[i] += EV_DELTA(ev);
    d = rt_acc[i] / COUNTS;
    rt_acc[i] -= d * COUNTS;
    if (!d || ch_held_key < 0 || !ch_view(&v) || v.step < 0)
        return 1;
    rt_read(v.track, v.step, &r);
    if (e == 5)
        r.rate = ch_clamp(r.rate + d, 0, RT_RATES - 1);
    else if (e == 6)
        r.len = ch_clamp(r.len + d, 0, RT_LENS - 1);
    else if (e == 7)
        r.vel = ch_clamp(r.vel + d, -8, 7);
    else
        return 1;
    rt_write(v.track, v.step, &r);
    return 1;
}

void retrig_tick(void *ctrl)
{
    struct ch_view v;
    struct rt_trig r = { 0, RT_LEN_ONE, 0 };
    uint32_t s;
    if (!rt_shown() || !ch_view(&v))
        return;
    if (v.step >= 0)
        rt_read(v.track, v.step, &r);
    s = (uint32_t)(v.track | (v.step + 1) << 2 | (ch_held_key + 1) << 9 | r.rate << 14 | r.len << 19 |
                   (r.vel + 8) << 23);
    if (s != rt_drawn) {
        rt_drawn = s;
        *((volatile uint8_t *)ctrl + 0x20) = 1;
    }
}

static void rt_rate_name(char *buf, int32_t rate)
{
    int32_t n = rt_rates[rate];
    if (!rate) {
        ch_text(buf, "OFF");
        return;
    }
    *buf++ = '1';
    *buf++ = '/';
    if (n >= 10)
        *buf++ = (char)('0' + n / 10);
    *buf++ = (char)('0' + n % 10);
    *buf = 0;
}

static void rt_num(char *buf, int32_t n)        /* -128..127 */
{
    if (n < 0) {
        *buf++ = '-';
        n = -n;
    }
    if (n >= 100)
        *buf++ = (char)('0' + n / 100);
    if (n >= 10)
        *buf++ = (char)('0' + n / 10 % 10);
    *buf++ = (char)('0' + n % 10);
    *buf = 0;
}

void retrig_draw(void *bmp, void *ctrl)
{
    struct ch_view v;
    struct rt_trig r = { 0, RT_LEN_ONE, 0 };
    char buf[8];
    static const char letters[] = "EFG";
    int32_t held, i, n, x, h;
    uint32_t span, iv, t;
    (void)ctrl;
    if (!rt_shown() || !ch_view(&v))
        return;
    held = ch_held_key >= 0 && v.step >= 0;
    if (held)
        rt_read(v.track, v.step, &r);
    ch_rect(bmp, 0, 0, 127, 63, 0);

    fw_framerect(bmp, 0, 24, 127, 63, 1);
    if (held)
        fw_textf(bmp, FONT_TTL, 3, 56, -1, "RETRIG TRK %d  TRIG %d", (int)v.track + 1, (int)v.step + 1);
    else if (ch_held_key >= 0)
        fw_textf(bmp, FONT_TTL, 3, 56, -1, "RETRIG TRK %d  TRIG ?", (int)v.track + 1);
    else
        fw_textf(bmp, FONT_TTL, 3, 56, -1, "RETRIG TRK %d", (int)v.track + 1);
    ch_rect(bmp, 1, 55, 126, 62, -1);

    if (!held) {
        ch_small(bmp, 64, 38, "HOLD A TRIG");
    } else if (!r.rate) {
        ch_small(bmp, 64, 38, "RETRIG OFF");
    } else {                            /* the hits over LEN (INF: a bar), as tall as their velocity */
        span = rt_lens[r.len] ? rt_lens[r.len] * RT_EIGHTH : 16 * 8 * RT_EIGHTH;
        iv = RT_BAR / rt_rates[r.rate];
        for (n = 0, t = 0; t < span && n < 64; t += iv, n++) {
            x = 4 + (int32_t)(t / (span / 119u));
            h = rt_vel(127, r.vel, (int32_t)(t / (span >> 8))) * 22 / 127;
            if (h < 1)
                h = 1;
            ch_rect(bmp, x, 28, x, 28 + h, 1);
        }
        if (!rt_lens[r.len])
            ch_small(bmp, 116, 46, "...");
    }

    ch_label(bmp, 16, 18, "RATE");
    ch_label(bmp, 48, 18, "LEN");
    ch_label(bmp, 80, 18, "VEL");
    if (held) {
        rt_rate_name(buf, r.rate);
        ch_big(bmp, 16, 7, buf);
        ch_big(bmp, 48, 7, rt_len_names[r.len]);
        rt_num(buf, r.vel * 16);
        ch_big(bmp, 80, 7, buf);
    } else {
        for (i = 0; i < 3; i++)
            ch_big(bmp, 16 + 32 * i, 7, "--");
    }
    for (i = 0; i < 3; i++) {
        buf[0] = '(';
        buf[1] = letters[i];
        buf[2] = ')';
        buf[3] = 0;
        ch_small(bmp, 16 + 32 * i, 0, buf);
    }
}

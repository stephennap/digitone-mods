# CHORDS (Digitone mk1)

The Digitone II's keyboard chord modes for the Digitone mk1 and Digitone
Keys, OS 1.43 and 1.44: CHORD ROOT, CHORD CENTER and CHORD SPREAD, with a
shape, a center position and BASS OCT, on a page drawn after the Digitone
II's keyboard setup screen. The chords are built from each track's keyboard
ROOT and SCALE.

A single-note trig on a track with MODE on plays the chord, and so does a
note played live: on the TRIG keys in keyboard mode, or over MIDI in on the
track's channel. Played live, the chord is held as long as the key, so the
**arpeggiator** steps through it. (The mk1's arpeggiator plays only the
first note of a chord a trig plays from the sequencer, the Note Add menu's
chords too: to arpeggiate a chord, play it.)

The TRIG key's third page, **Retrig**, repeats a trig as the Digitakt's
retrig does: per trig, a RATE, a LEN and a velocity fade (below).

| | |
|---|---|
| needs | core-dn1 4.1 or newer (parameter slots, mod pages, project data, `fw_pattern`, `ev_note_queue`), seq-events |
| sites | one, the played note's: 0x400ff1d6 (1.44 0x400ff44a), in the builder of a played note's event; and `ev_fire_notes`, `ev_fire_vel`, `ev_note_queue`, `ev_render_out`, `ev_key`, `ev_enc`, `ev_tick`, `ev_draw`, three `core_params`, two `core_pages` and a `core_projdata` entry |
| claims | `param:182`-`184`, `page:27` and `page:28` (after page 1, the TRIG key's Trig Parameters: Chords, then Retrig), `projdata:CHRD`, and sound slots 26, 65 and 0 (so not with digitables or machines) |
| RAM | 15.3 KB code, 6 KB data (0.14) |
| sources | [chords.c](chords.c) (the page, its knobs, the handlers), [chordmath.h](chordmath.h) (the chord math, no firmware), [live.s](live.s) (the played note's site), [retrig.inc](retrig.inc) (RETRIG, included by chords.c) |
| tests | [tests/test_chords_dn1.py](../../tests/test_chords_dn1.py) |

## Using it

On a synth track, press TRIG (the note page) again: the Chords page.

```
+-------------------------+ BASS
| CHORDS TRK 1            | OCT
| [keyboard: the chord,   |  -1
|  the center's arrow]    | (D)
| CNTR C5 (A)    INV (B) |
+-------------------------+
 MODE   ROOT   SCALE  SHAPE
CENTER   C5     DOR   1-3-5
 (E)    (F)     (G)    (H)
```

| knob | | values | kept in |
|---|---|---|---|
| A | CENTER | AUTO (centers on the played note), C3-C8; an arrow under the keyboard | the project, per track |
| B | INV | moves CENTER to the next (right) or last (left) inversion of the chord, as the Digitone II's [+]/[-] on their own; from AUTO it starts at ROOT | the project, per track |
| D | BASS OCT | 0, -1, -2, -3; "--" while MODE is OFF | the sound, or a trig's p-lock |
| E | MODE | OFF, CHORD ROOT, CHORD CENTER, CHORD SPREAD | the sound, or a trig's p-lock |
| F | ROOT | the track's keyboard ROOT, a note | the pattern (the stock setting itself) |
| G | SCALE | the track's keyboard SCALE, by the OS's short names (CHRO, ION, DOR ...) | the pattern (the stock setting itself) |
| H | SHAPE / TYPE | the Digitone II's, in its order: in a scale 8 shapes, 1-3-5, 1-2-5, 1-3, 1-4, 1-5, 1-4-5, 1-3-5-7, 1-4-7; on chromatic 12 types, MAJ, MIN, 7TH, MIN7, MAJ7, DIM, DIM7, SUS2, SUS4, 6, MIN6, ADD9 | the sound, or a trig's p-lock |

**A chord a trig.** Turned on their own, D, E and H set the sound, so every
trig; turned while a trig is held, they p-lock that trig, as any knob does. So each trig can have its own MODE,
SHAPE and BASS OCT. While a trig is held the title shows it (`TRK 1  TRIG 5`)
and the page shows that trig's values, on any of the 64 steps: the step is
the one the Digitone's own grid marks as held.

With a trig held, F is that trig's **NOTE**, its chord's root (the trig's own
note, the one the note page shows), so each trig picks its chord in the key;
with none held F is the track's ROOT, the key itself.

With a trig held, A and B set **that trig's own CENTER** (A by semitones, B
by inversions of the trig's chord), so each trig can be voiced on its own;
the page marks it `CNTR C5*`. A trig with none follows the track's CENTER,
which A and B set with no trig held. A trig's CENTER lives in the low byte
of its SHAPE p-lock (8.8: the shape is the high byte; this slot is no stock
sound parameter, so nothing else reads that byte), so it is saved, copied
and pasted with the trig. SCALE (G) is the track's: with a trig held it does
nothing, so a turn meant for the trig cannot change every chord.

The keyboard shows two octaves with a dot on each note of the last chord
the track played. Notes are named as the Digitone names them: MIDI 60 is C5.

- **CHORD ROOT**: the chord stacks up from the played note. With ROOT C and
  SCALE Ionian, 1-3-5: C plays C E G, D plays D F A; 1-3-5-7 on C: C E G B.
- **CHORD CENTER**: the same notes, each in the octave from CENTER-5 to
  CENTER+6, so the chords change by inversion around one place. CENTER C5:
  C plays G4 C5 E5, F plays A4 C5 F5, G plays G4 B4 D5.
- **CHORD SPREAD**: CENTER's chord with every second note an octave up (C:
  G4 E5 C6).
- **INV** (knob B): CENTER places each note from CENTER-5 to CENTER+6,
  so a step up moves CENTER to the lowest note + 6, which lifts exactly
  that note an octave, and a step down to the highest note - 7, which
  drops exactly that one: the next inversion, whatever the chord. It
  steps through the last chord the track played, or ROOT's chord before
  any has.
- **BASS OCT** (-1 to -3): the root again, that many octaves below the
  chord's root and under every note. With more than three notes it replaces
  the root: C4 on 1-3-5-7 at -1 plays C3 E4 G4 B4.

In a scale the chords are the scale's own: its notes stacked from the played
one, so in a major key 1-3-5-7 gives Imaj7, ii m7, iii m7, IVmaj7, V7, vi m7
and vii m7b5, and in harmonic minor the tonic is m(maj7) and the leading tone
dim7. In a scale of 5 or 6 notes there is no 7th step: 1-3-5-7 runs on into
the next octave. With SCALE on chromatic the 12 types are the Digitone's own
Note Add chords (the mk1's table and the Digitone II's are the same bytes).
SHAPE keeps its value when SCALE changes: a type past the 8th is 1-4-7 in a
scale.

It plays as stock when it cannot say: a trig that already holds a chord, a
note outside the scale, or a chord that would leave the MIDI range.

## Retrig

Press TRIG a third time: the Retrig page. Hold a trig and turn:

| knob | | |
|---|---|---|
| E | RATE | OFF, 1/1 to 1/80: hits a bar (1/16 is one a step), the Digitakt's rates |
| F | LEN | how long it repeats: 1/8 of a step to 32 steps, or INF (until the track's next trig, or STOP) |
| G | VEL | the velocity over LEN: -128 fades out to nothing, 0 keeps it, 112 fades in from an eighth (the trig itself too) |

The page draws the hits over LEN (INF: a bar), each as tall as its
velocity. Each hit is the trig's whole event again: its chord, its length
and its p-locks. With no trig held the page shows nothing to set: RETRIG is
a trig's, not a track's.

**Storage.** The mk1 has no sound slot left, so a trig's RATE is the low
byte of its MODE p-lock, and LEN (low nibble, stored as the index - 5, so 0
is one step) and VEL (high nibble, -8..7 sixteenths of 128) the low byte of
its BASS OCT p-lock. D and E keep that byte when they set the lock. A trig
with no MODE or BASS OCT lock gets the high byte 0x80: a negative value
that is not -1, which `ch_value` and the rows' lock count (the stock's,
0x40023ed2, tests bit 7) read as no lock, so RETRIG never fixes a trig's
MODE or BASS OCT. Setting both back to their defaults (OFF; LEN 1, VEL 0)
on such a trig writes -1 again.

**Playback.** On a retrig trig's fire `retrig_notes` (`ev_fire_notes`)
marks the track; `retrig_queue` (core 4.1's `ev_note_queue`, masked) takes
the event the fire then queues, with its time, and keeps a copy of it,
with a reference to its p-lock list (the list's count at +0; the
firmware's own event copy, 0x400ffde8, takes one the same way, and its
release, 0x400ffd50, puts a list back on `fw_locks_free` at 0). The
render's exit (`retrig_render`, `ev_render_out`) queues a fresh event
(`fw_ev_alloc`, a copy, +0x44 the queue's link cleared, its own reference)
for each hit half a step before its time, with the faded velocity at
+0x10 (velocity << 8), through `fw_ev_queue`, masked, and only with a free
queue time (`fw_nodes_free`). A hit whose event or time is not to be had
is skipped. Any newer note trig on the track ends the retrig at its time;
STOP or pause ends all; a first step's event (kind 1) plays its hits as
fires (kind 0) with no p-lock list. Eight retrigs play at once, the oldest
giving way. With a fade in, `retrig_vel` (`ev_fire_vel`) lowers the trig's
own velocity to the fade's start.

## How it works

`chords_notes` runs on `ev_fire_notes` (seq-events, after every note
generator), on the synth tracks. It reads MODE and SHAPE as the step plays
them: the step's p-lock from the active pattern's lock table (`+0x1e80`,
10319 bytes a track, 160 a step, a big-endian s16 a slot, -1 none:
fxkit-dn1's reading), else the sound's slot in `fw_kit`; BASS OCT's too,
slot 0 (a lock row holds every slot: fxkit-dn1 counts slot 0 in a row's
locks). CENTER is the project data `CHRD`, a byte a track. ROOT and SCALE
come from `sq_track_info`. The notes it writes are the lowest note and
intervals above it, each 1-127, at most 8: seq-events passes them to the
firmware unchecked. It keeps each track's last notes for the keyboard.

The page is a mod page whose D, E and H are the parameter slots 184, 182
and 183 (BASS OCT, MODE, SHAPE), so with no trig held the stock turns them,
setting the sound; the others are empty. The TRIG key's pages do not p-lock
sound slots with a trig held (seen on a unit), so then `chords_enc` turns D,
E and H itself and `ch_lock_set` writes the trig's p-lock as fxkit-dn1 does:
the value in the step's row, the row's count (`+0x9e`), the track's used
flag (`+0x2800`) and a lock trig on a step with no trig. While
`core_page_shown` says the page is up, `chords_draw` (`ev_draw`) clears the
screen and draws it, and `chords_enc` (`ev_enc`) takes knobs A, B, C, F and
G, and D, E and H with a trig held. `chords_key` (`ev_key`) takes no key: it
watches which TRIG key is held, and takes its step from the grid's own
held-steps mask: a 64-bit mask, bit n step n, at `+0x158` (steps 32-63) and
`+0x15c` (steps 0-31) of the grid's state, a singleton whose pointer is at
`0x419efdd8` (1.44: `0x419f0dd8`; its getter 0x40149dec, 1.44 0x4014a060; the
mask's test 0x400adb50, 1.44 0x400adbe0; its setter 0x400ae3cc, from the
grid's key press), given to the mod as `CH_SEQ_UI` in `mod.json`'s `cflags`.
The held step is the set bit on the held key's column (step % 16 = key).
(`+0x190` of the same state, 0-7, is the pattern bank, not the page of
steps: an earlier reading of 0x400b02cc took it for one.) Until the stock
sets the bit, and should the pointer be unset, the step is the key's on a
track of up to 16 steps, and on a longer one `chords_tick` finds it as the
step whose lock of MODE, SHAPE or BASS OCT changes while the key is held. `ev_draw` has no
brain to ask, so the mod keeps the key dispatcher's from its first key or
knob event. `chords_tick` redraws when what the page shows changes. ROOT and
SCALE are
written to the active pattern's track bytes (`+0x3cb`, `+0x3ce`) within the
stock setters' limits (1.43: 0x40035274 clamps ROOT to 0-115, 0x4003553a
SCALE to -1..34). The stock setters also post a change notification, which
this does not.

**Played notes.** The keyboard's play-note (0x400ea940, from KeyboardView
at 0x4003b2ea) and MIDI in reach one builder of a played note's event
(0x400ff178), for a key's press and its release alike; at 0x400ff1d6 it has
written count 1 (`+0x14`) and writes the note (`+0x18`). `live.s` jsr's
there, does that move and calls `chords_played(event, request)`: on a
synth track with MODE on, a press (the request's flags not 0) gets the
chord of the sound's MODE, SHAPE and BASS OCT and the track's ROOT, SCALE
and CENTER, as absolute notes from `+0x18`, and the track and note keep it;
a release (flags 0) gets the same notes back. Each of the event's notes
gets a voice, and a release that carries them frees them all: seen on a
unit with a probe (`examples/chords-probe-dn1`). The builder needs d1
after the site, so `live.s` keeps d0-d1/a0-a1. Lint lists the site under
`core_candidates`: an event for a played note belongs in the core.

The SCALE byte is -1 for chromatic and 0-34 for the OS's scales, in the
keyboard menu's order. In 1.43 the names are pointers at `0x40241cd0`
(Chromatic first, so name k+1 is SCALE k; the short names are entries
36-71); every scale is a 13-byte record at `0x401d4e68` (each semitone's
degree from 1, or 0, then the octave's); the 7-note ones are also 7-byte
records at `0x401d3ed0` (`0xff` first for the others); the Note Add menu's
12 chord shapes are 4-byte records at `0x401d3fc5`; and notes are named by
0x40116d54, "%s%d" with the names at `0x401d40b8` and the octave note / 12.
The test checks all 35 scale masks and the chromatic shapes against them.

**From 0.3 or 0.4:** those kept MODE and BASS OCT combined (0-12) in slot
26 and CENT in slot 0. On a sound edited with them, 0.5 reads a stale MODE
or BASS OCT: set both once on the page. CENTER starts at AUTO.

## Not checked yet

- Not yet run on a unit: RETRIG (the hits' timing, the velocity fade, INF
  and its end at the next trig, STOP, a pattern change, a retrig on the
  first step, voices at 1/80 with a long note: a hit keeps the trig's
  length, so long notes overlap); chords on the TRIG keys and over MIDI, with and
  without the arpeggiator; p-locking D, E and H with a trig held, BASS OCT's
  p-locks (slot 0) as played, and the held trig's values on the page, on
  steps 17-64 too (the grid's page read from its state).
- SHAPE's stock knob runs to 12 whatever SCALE: in a scale the values past
  the 8th show and play as 1-4-7.
- ROOT and SCALE written here: whether the keyboard follows at once without
  the stock setter's notification, and what the keyboard's GLOBAL setting
  (the note setup menu) does with a track's own bytes.
- Slot 0: it is also where an LFO with no destination points; the LFO's
  destination list may show "Bass octave" for it.
- A chord uses as many voices as it has notes (up to 6 here), so a track's
  VOICE setting and the other tracks limit how many sound.

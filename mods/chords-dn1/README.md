# CHORDS (Digitone mk1)

The Digitone II's keyboard chord modes for the Digitone mk1 and Digitone
Keys, OS 1.43 and 1.44: CHORD ROOT, CHORD CENTER and CHORD SPREAD, with a
shape, a center position and BASS OCT, on a page drawn after the Digitone
II's keyboard setup screen. The chords are built from each track's keyboard
ROOT and SCALE.

**Phase 1: sequenced trigs.** A single-note trig on a track with MODE on
plays the chord. The TRIG keys played live still play single notes: that
needs a new hook in the keyboard's note path (phase 2).

| | |
|---|---|
| needs | core-dn1 3.3 or newer (parameter slots, mod pages, project data, `fw_pattern`), seq-events |
| sites | none: `ev_fire_notes`, `ev_key`, `ev_enc`, `ev_tick`, `ev_draw`, three `core_params`, a `core_pages` and a `core_projdata` entry, so its 1.44 port is empty |
| claims | `param:182`-`184`, `page:27` (after page 1, the TRIG key's Trig Parameters), `projdata:CHRD`, and sound slots 26, 65 and 0 (so not with digitables or machines) |
| RAM | 7.4 KB code |
| sources | [chords.c](chords.c) (the page, its knobs, the handlers), [chordmath.h](chordmath.h) (the chord math, no firmware) |
| tests | [tests/test_chords_dn1.py](../../tests/test_chords_dn1.py) |

## Using it

On a synth track, press TRIG (the note page) again: the Chords page.

```
+-------------------------+ BASS
| CHORDS T1               | OCT
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

**A chord a trig.** D, E and H are stock knobs under the drawn page: turned
on their own they set the sound, so every trig; turned while a trig is held,
they p-lock that trig, as any knob does. So each trig can have its own MODE,
SHAPE and BASS OCT. While a trig is held the title shows it (`T1  TRIG 5`)
and the page shows that trig's values. On a track longer than 16 steps the
page cannot see which page of steps is shown: it shows `TRIG ?` until a knob
is turned, then the step that turn locked.

With a trig held, F is that trig's **NOTE**, its chord's root (the trig's own
note, the one the note page shows), so each trig picks its chord in the key;
with none held F is the track's ROOT, the key itself. CENTER, INV and SCALE
(A, B, G) are the track's, not a trig's, as on the Digitone II: with a trig
held they do nothing, so a turn meant for the trig cannot change every
chord.

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
and 183 (BASS OCT, MODE, SHAPE), so the stock turns and p-locks them; the
others are empty. While `core_page_shown` says it is up, `chords_draw`
(`ev_draw`) clears the screen and draws it, and `chords_enc` (`ev_enc`)
takes knobs A, B, C, F and G. `chords_key` (`ev_key`) takes no key: it
watches which TRIG key is held, whose step is the key's on a track of up to
16 steps; on a longer one `chords_tick` finds it as the step whose lock of
MODE, SHAPE or BASS OCT changes while the key is held. `ev_draw` has no
brain to ask, so the mod keeps the key dispatcher's from its first key or
knob event. `chords_tick` redraws when what the page shows changes. ROOT and
SCALE are
written to the active pattern's track bytes (`+0x3cb`, `+0x3ce`) within the
stock setters' limits (1.43: 0x40035274 clamps ROOT to 0-115, 0x4003553a
SCALE to -1..34). The stock setters also post a change notification, which
this does not.

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

- Not yet run on a unit: p-locking D, E and H with a trig held, BASS OCT's
  p-locks (slot 0) as played, and the held trig's values on the page.
- SHAPE's stock knob runs to 12 whatever SCALE: in a scale the values past
  the 8th show and play as 1-4-7.
- ROOT and SCALE written here: whether the keyboard follows at once without
  the stock setter's notification, and what the keyboard's GLOBAL setting
  (the note setup menu) does with a track's own bytes.
- Slot 0: it is also where an LFO with no destination points; the LFO's
  destination list may show "Bass octave" for it.
- A chord uses as many voices as it has notes (up to 6 here), so a track's
  VOICE setting and the other tracks limit how many sound.

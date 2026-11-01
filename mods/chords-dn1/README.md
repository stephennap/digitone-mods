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
| CENTER C4          (A)  |
+-------------------------+
 MODE   ROOT   SCALE  SHAPE
CENTER   C4     DOR   1-3-5
 (E)    (F)     (G)    (H)
```

| knob | | values | kept in |
|---|---|---|---|
| A | CENTER | AUTO (centers on the played note), C2-C7; an arrow under the keyboard | the project, per track |
| D | BASS OCT | 0, -1, -2, -3 | the sound |
| E | MODE | OFF, CHORD ROOT, CHORD CENTER, CHORD SPREAD | the sound |
| F | ROOT | the track's keyboard ROOT, a note | the pattern (the stock setting itself) |
| G | SCALE | the track's keyboard SCALE, by the OS's short names (CHRO, ION, DOR ...) | the pattern (the stock setting itself) |
| H | SHAPE | 8 shapes, **TRIG 1-8** too: in a scale 1-3-5, 1-3-5-7, 13579, 1-2-5, 1-4-5, 1-3-5-6, 1-3-5-9, 1-5; on chromatic MAJ, MIN, 7TH, MIN7, MAJ7, DIM, SUS2, SUS4 | the sound |

The keyboard shows two octaves with a dot on each note of the last chord
the track played.

- **CHORD ROOT**: the chord stacks up from the played note. With ROOT C and
  SCALE Ionian, 1-3-5: C plays C E G, D plays D F A; 1-3-5-7 on C: C E G B.
- **CHORD CENTER**: the same notes, each in the octave from CENTER-5 to
  CENTER+6, so the chords change by inversion around one place. CENTER C4:
  C plays G3 C4 E4, F plays A3 C4 F4, G plays G3 B3 D4.
- **CHORD SPREAD**: CENTER's chord with every second note an octave up (C:
  G3 E4 C5).
- **BASS OCT** (-1 to -3): the root again, that many octaves below the
  chord's root and under every note. With more than three notes it replaces
  the root: C3 on 1-3-5-7 at -1 plays C2 E3 G3 B3.

It plays as stock when it cannot say: a trig that already holds a chord, a
note outside the scale, a scale whose notes it does not know (Combo Minor,
Persian, Iwato, In-Sen, Hirajoshi, Pelog, Spanish), or a chord that would
leave the MIDI range.

## How it works

`chords_notes` runs on `ev_fire_notes` (seq-events, after every note
generator), on the synth tracks. It reads MODE and SHAPE as the step plays
them: the step's p-lock from the active pattern's lock table (`+0x1e80`,
10319 bytes a track, 160 a step, a big-endian s16 a slot, -1 none:
fxkit-dn1's reading), else the sound's slot in `fw_kit`. BASS OCT is slot 0,
whose lock it does not read (fxkit-dn1 treats slot 0 in a lock row as no
slot). CENTER is the project data `CHRD`, a byte a track. ROOT and SCALE
come from `sq_track_info`. The notes it writes are the lowest note and
intervals above it, each 1-127, at most 8: seq-events passes them to the
firmware unchecked. It keeps each track's last notes for the keyboard.

The page is a mod page with no stock knobs (all eight ids 0). While
`core_page_shown` says it is up, `chords_draw` (`ev_draw`) clears the
screen and draws it, `chords_enc` (`ev_enc`) takes all eight knobs (B and C
do nothing), and `chords_key` (`ev_key`) takes TRIG 1-8, press and release;
FUNC + TRIG stays stock. `ev_draw` has no brain to ask, so the mod keeps
the key dispatcher's from its first key or knob event. `chords_tick`
redraws when what the page shows changes. MODE, SHAPE and BASS OCT are set
with `core_sound_set`, as a stock knob sets a sound; ROOT and SCALE are
written to the active pattern's track bytes (`+0x3cb`, `+0x3ce`) within the
stock setters' limits (1.43: 0x40035274 clamps ROOT to 0-115, 0x4003553a
SCALE to -1..34). The stock setters also post a change notification, which
this does not.

The SCALE byte is -1 for chromatic and 0-34 for the OS's scales, in the
keyboard menu's order. In 1.43 the names are pointers at `0x40241cd0`
(Chromatic first, so name k+1 is SCALE k; the short names are entries
36-71), 23 of the 35 scales are 7-byte records at `0x401d3ed0` (`0xff`
first for the others, kept elsewhere, not found), and the Note Add menu's
12 chord shapes are 4-byte records at `0x401d3fc5`. The test checks the
scale masks and the chromatic shapes against them.

## Not checked yet

- Not yet run on a unit: the page as drawn, its knobs, TRIG 1-8, the chords
  as played. The layout was previewed on a PC with stand-in fonts.
- Turning a knob on this page while holding a trig sets the sound, not a
  p-lock (MODE and SHAPE p-locks set elsewhere still play).
- ROOT and SCALE written here: whether the keyboard follows at once without
  the stock setter's notification, and what the keyboard's GLOBAL setting
  (the note setup menu) does with a track's own bytes.
- Slot 0: it is also where an LFO with no destination points; the LFO's
  destination list may show "Bass octave" for it.
- Note names assume MIDI 60 = C4; the Digitone's own may differ.
- A chord uses as many voices as it has notes (up to 6 here), so a track's
  VOICE setting and the other tracks limit how many sound.
- The textbook notes for Pentatonic Minor and Major, Blues and the two
  diminished scales are not checked against the OS.

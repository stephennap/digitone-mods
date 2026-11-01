# CHORDS (Digitone mk1)

The Digitone II's keyboard chord modes for the Digitone mk1 and Digitone
Keys, OS 1.43 and 1.44: CHORD ROOT, CHORD CENTER and CHORD SPREAD, with a
shape, a center position and BASS OCT. The chords are built from each
track's keyboard ROOT and SCALE.

**Phase 1: sequenced trigs.** A single-note trig on a track with MODE on
plays the chord. The TRIG keys played live still play single notes: that
needs a new hook in the keyboard's note path (phase 2).

| | |
|---|---|
| needs | core-dn1 3.3 or newer (parameter slots, mod pages, `fw_pattern`), seq-events |
| sites | none: `ev_fire_notes`, `ev_key`, three `core_params` and a `core_pages` entry, so its 1.44 port is empty |
| claims | `param:182`-`184`, `page:27`, and sound slots 26, 65 and 0 (so not with digitables or machines) |
| RAM | 2.8 KB code |
| sources | [chords.c](chords.c) (the knobs, the page, the handlers), [chordmath.h](chordmath.h) (the chord math, no firmware) |
| tests | [tests/test_chords_dn1.py](../../tests/test_chords_dn1.py) |

## Using it

Press AMP until **Chords (3/3)** shows. Its knobs are the sound's, like any
stock knob: saved with the sound, copied with it, and MODE and SHAP
p-lockable per step.

| knob | values | |
|---|---|---|
| MODE | OFF; ROOT, CNTR, SPRD, each with BASS OCT: ROOT, RT-1, RT-2, RT-3, CNTR, CN-1 ... SP-3 | the mode and the bass octave on one knob: the core has three parameter slots |
| SHAP | 8 shapes; **TRIG 1-8** pick them while this page shows | in a scale: 135, 1357, 1-9 (1-3-5-7-9), 125, 145, 1356, 1359, 15; on chromatic: MAJ, MIN, 7TH, MIN7, MAJ7, DIM, SUS2, SUS4 |
| CENT | AUTO, C2-C7 | CENTER POSITION; AUTO centers on the played note. Every sound starts at AUTO |

- **ROOT**: the chord stacks up from the played note. With ROOT C and
  SCALE Ionian, 135: C plays C E G, D plays D F A; 1357 on C: C E G B.
- **CNTR**: the same notes, each in the octave from CENT-5 to CENT+6, so
  the chords change by inversion around one place. CENT C4: C plays
  G3 C4 E4, F plays A3 C4 F4, G plays G3 B3 D4.
- **SPRD**: CNTR's chord with every second note an octave up (C: G3 E4 C5).
- **BASS OCT** (-1 to -3): the root again, that many octaves below the
  chord's root and under every note. With more than three notes it
  replaces the root: C3 on 1357 at RT-1 plays C2 E3 G3 B3.

It plays as stock when it cannot say: a trig that already holds a chord, a
note outside the scale, a scale whose notes it does not know (Combo Minor,
Persian, Iwato, In-Sen, Hirajoshi, Pelog, Spanish), or a chord that would
leave the MIDI range.

## How it works

`chords_notes` runs on `ev_fire_notes` (seq-events, after every note
generator), on the synth tracks. It reads MODE and SHAP as the step plays
them: the step's p-lock from the active pattern's lock table (`+0x1e80`,
10319 bytes a track, 160 a step, a big-endian s16 a slot, -1 none:
fxkit-dn1's reading), else the sound's slot in `fw_kit`. CENT is slot 0,
whose lock it does not read (fxkit-dn1 treats slot 0 in a lock row as no
slot), so CENT follows the sound only. ROOT and SCALE come from
`sq_track_info`. The notes it writes are the lowest note and intervals
above it, each 1-127, at most 8: seq-events passes them to the firmware
unchecked.

`chords_key` (`ev_key`) takes TRIG 1-8, press and release, while
`core_page_shown` says the page is up, and sets the active track's SHAP
with `core_sound_set`, as a turn of the knob does. FUNC + TRIG stays stock.

The SCALE byte is -1 for chromatic and 0-34 for the OS's scales, in the
keyboard menu's order. In 1.43 the names are pointers at `0x40241cd0`
(Chromatic first, so name k+1 is SCALE k), 23 of the 35 scales are 7-byte
records at `0x401d3ed0` (`0xff` first for the others, kept elsewhere, not
found), and the Note Add menu's 12 chord shapes are 4-byte records at
`0x401d3fc5`. The test checks the scale masks and the chromatic shapes
against them.

## Not checked yet

- Not yet run on a unit: the page and knobs, TRIG 1-8, the chords as played.
- Slot 0 as a knob: it is also where an LFO with no destination points; the
  LFO's destination list may show "Center position" for it.
- CENT's note names assume MIDI 60 = C4; the Digitone's own may differ.
- A chord uses as many voices as it has notes (up to 6 here), so a track's
  VOICE setting and the other tracks limit how many sound.
- The textbook notes for Pentatonic Minor and Major, Blues and the two
  diminished scales are not checked against the OS.

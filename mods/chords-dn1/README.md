# CHORDS (Digitone mk1)

Scale-aware chords for the Digitone mk1 and Digitone Keys, OS 1.43 and 1.44.
A single-note trig on a synth track plays a chord built from the track's
keyboard ROOT and SCALE: in C minor, a C plays C, Eb, G; in C major, a D
plays D, F, A.

| | |
|---|---|
| needs | core-dn1 3.1 or newer (the Mod Menu's submenus), seq-events |
| sites | none: `ev_fire_notes` and a `core_menu` entry, so its 1.44 port is empty |
| RAM | 1.7 KB code, 8 bytes data |
| sources | [chords.c](chords.c) (the menu and the handler), [chordmath.h](chordmath.h) (the chord math, no firmware) |
| tests | [tests/test_chords_dn1.py](../../tests/test_chords_dn1.py) |

## Using it

Hold T1-T4 for the Mod Menu and pick **CHORDS**, then a type and a voicing.
Each synth track has its own; they last until power-off.

| type | scale degrees | voicing | |
|---|---|---|---|
| OFF | stock | CLOSE | as built |
| TRIAD | 1 3 5 | INV 1 | the lowest tone up an octave |
| SUS2 | 1 2 5 | INV 2 | the lowest two up |
| SUS4 | 1 4 5 | INV 3 | the lowest three up (a 4-note chord's last inversion) |
| 6TH | 1 3 5 6 | OPEN | the root position's second tone up an octave |
| 7TH | 1 3 5 7 | | |
| 9TH | 1 3 5 7 9 | | |
| ADD9 | 1 3 5 9 | | |

The degrees are the scale's own, so a 5-note scale stacks every other note of
its five (C minor pentatonic: C, F, Bb). With SCALE on chromatic the types
are fixed major-quality shapes (TRIAD 0 4 7, 7TH 0 4 7 10, ...).

It plays as stock when it cannot say: a trig that already holds a chord, a
note outside the scale, a scale whose notes it does not know (Combo Minor,
Persian, Iwato, In-Sen, Hirajoshi, Pelog, Spanish), or a chord that would
leave the MIDI range.

## How it works

`chords_notes` runs on `ev_fire_notes` (seq-events, after every note
generator), on the synth tracks only. It reads ROOT and SCALE with
`sq_track_info`, and `ch_build` writes the lowest tone as the note and the
others as intervals above it, each 1-127, at most 8 notes: seq-events passes
notes to the firmware unchecked.

The SCALE byte is -1 for chromatic and 0-34 for the OS's scales, in the
keyboard menu's order. In 1.43, the names are pointers at `0x40241cd0`
(Chromatic first, so name k+1 is SCALE k) and 23 of the 35 scales are 7-byte
records at `0x401d3ed0` (`0xff` first for the others, which the OS keeps
elsewhere, not found). The test checks the header's masks against those 23.

## Not checked yet

- Not yet run on a unit: the menu, the chords as played, the voice count.
- A chord uses as many voices as it has notes, so a track's VOICE setting
  and the other tracks limit how many sound.
- The textbook notes for Pentatonic Minor and Major, Blues and the two
  diminished scales are not checked against the OS.
- Chords played live on the keyboard go another way and are not changed.

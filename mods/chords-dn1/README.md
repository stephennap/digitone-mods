# CHORDS (Digitone mk1)

Scale-aware chords for the Digitone mk1 and Digitone Keys, OS 1.43 and 1.44,
on two knobs of a third AMP page, **Chords**. The chords follow each track's
keyboard ROOT and SCALE: chords entered with the stock Note Add menu move
into the key, and single notes can become chords of the key.

| | |
|---|---|
| needs | core-dn1 3.3 or newer (parameter slots, mod pages, `fw_pattern`), seq-events |
| sites | none: `ev_fire_notes`, two `core_params` and a `core_pages` entry, so its 1.44 port is empty |
| claims | `param:182`, `param:183`, `page:27`, and the sound slots 26 and 65 (so not with digitables or machines) |
| RAM | 2 KB code |
| sources | [chords.c](chords.c) (the knobs, the page and the handler), [chordmath.h](chordmath.h) (the chord math, no firmware) |
| tests | [tests/test_chords_dn1.py](../../tests/test_chords_dn1.py) |

## Using it

Press AMP until **Chords (3/3)** shows. The two knobs are a sound's, like
any stock knob: saved with the sound, p-lockable per step (hold a trig and
turn), copied and pasted with it.

| CHRD | a trig that holds a chord | a single-note trig |
|---|---|---|
| OFF | stock | stock |
| KEY | follows the key | stock |
| TRI | follows the key | 1 3 5 of the scale |
| SUS2, SUS4 | follows the key | 1 2 5, 1 4 5 |
| 6TH, 7TH, 9TH | follows the key | 1 3 5 6, 1 3 5 7, 1 3 5 7 9 |
| ADD9 | follows the key | 1 3 5 9 |

**Follows the key**: each of the chord's intervals moves to the same scale
step above its note (a 2nd, 3rd, 4th, 5th, 6th or 7th), its octave kept. So
the Note Add menu's MAJOR, placed on D in C major, plays D F A; in C minor,
on C, it plays C Eb G. The trig still shows the shape it holds; the knob
changes what it plays.

**VOIC**: CLS as built (a stored chord: its own spacing), INV1-INV3 the
lowest one to three tones up an octave, OPEN the second tone up an octave.

Single notes on a 5- or 6-note scale stack its own degrees (C minor
pentatonic, TRI: C F Bb); with SCALE on chromatic the types are fixed
major-quality shapes (TRI 0 4 7, 7TH 0 4 7 10, ...). Stored chords follow
only 7-note scales.

It plays as stock when it cannot say: a note outside the scale, a scale
whose notes it does not know (Combo Minor, Persian, Iwato, In-Sen,
Hirajoshi, Pelog, Spanish), a chord with a downward interval, or one that
would leave the MIDI range.

## How it works

`chords_notes` runs on `ev_fire_notes` (seq-events, after every note
generator), on the synth tracks only. It reads the step's CHRD and VOIC:
the step's p-lock from the active pattern's lock table (`+0x1e80`, 10319
bytes a track, 160 a step, a big-endian s16 a slot, -1 none: fxkit-dn1's
reading), else the sound's slot in `fw_kit`. ROOT and SCALE come from
`sq_track_info`. The notes it writes are the lowest tone and intervals
above it, each 1-127, at most 8: seq-events passes notes to the firmware
unchecked.

The SCALE byte is -1 for chromatic and 0-34 for the OS's scales, in the
keyboard menu's order. In 1.43 the names are pointers at `0x40241cd0`
(Chromatic first, so name k+1 is SCALE k), 23 of the 35 scales are 7-byte
records at `0x401d3ed0` (`0xff` first for the others, kept elsewhere, not
found), and the Note Add menu's 12 chord shapes are 4-byte records at
`0x401d3fc5`. The test checks the scale masks and the shapes against them.

## Not checked yet

- Not yet run on a unit: the page and knobs, their p-locks, the chords as played.
- The lock table's layout is fxkit-dn1's, which runs on units; this mod reads it at the step's fire.
- A chord uses as many voices as it has notes, so a track's VOICE setting
  and the other tracks limit how many sound.
- The textbook notes for Pentatonic Minor and Major, Blues and the two
  diminished scales are not checked against the OS.
- Chords played live on the keyboard go another way and are not changed.

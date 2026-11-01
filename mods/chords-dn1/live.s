| SPDX-License-Identifier: GPL-2.0-or-later
| CHORDS: the played note's site (Digitone mk1). ColdFire V4; assemble with -mcpu=54455.
|
| 0x400ff1d6 (1.44 0x400ff44a) is in the builder of a played note's event (0x400ff178), which the
| keyboard's play-note (0x400ea940, from KeyboardView at 0x4003b2ea) and MIDI in reach, for a key's
| press and its release alike. It has just written count 1 (+0x14) and writes the note:
|   move.b 7(a2),0x18(a4)               (a4 the event, a2 the request: +7 the note, +8 the velocity,
|                                        +0x10 the flags, 0 for a release)
| The event's notes are absolute, +0x18 on, +0x14 of them; each gets a voice, and a release that
| carries the same notes frees them all (seen on a unit). The jsr here does the same move, then
| chords_played(event, request), which may write a chord. The builder still needs d1 after this,
| so d0-d1/a0-a1 are kept.
        .section .run, "ax"
        .globl  chords_site_played
chords_site_played:
        move.b  7(%a2), 0x18(%a4)       | the instruction the jsr replaced
        lea     -16(%sp), %sp
        movem.l %d0-%d1/%a0-%a1, (%sp)
        move.l  %a2, -(%sp)             | the request
        move.l  %a4, -(%sp)             | the event
        jsr     chords_played
        addq.l  #8, %sp
        movem.l (%sp), %d0-%d1/%a0-%a1
        lea     16(%sp), %sp
        rts

| SPDX-License-Identifier: GPL-2.0-or-later
| chprobe: the render's two live-note sites (Digitone mk1). ColdFire V4; assemble with
| -mcpu=54455.
|
| For an event with no notes of its own (a live note: the keyboard, MIDI in or the arpeggiator,
| so far as is known), the render works the note out, then writes it as the event's only note:
|   A  0x4009e826 (1.44 0x4009e846), the note from the event's +0x40 (its flag 0x80000)
|   B  0x4009e760 (1.44 0x4009e780), the note from d3
| both: move.b d0,0x18(a3); moveq #1,d0; move.l d0,0x14(a3)   (a3 the event, d0 the note)
| and go on to the note-on loop, which starts a voice for each of the event's +0x14 notes from
| +0x18. Each site jsr's here: do the same, then chprobe_live(event, path), which may add notes.
| After both sites d0, d1, a0 and a1 are written before they are read, so C may use them.
|
| C  0x400ff1d6 (1.44 0x400ff44a), in the builder of a played note's event (0x400ff178), which the
|    keyboard's play-note (0x400ea940, from KeyboardView at 0x4003b2ea) and MIDI in reach: it has just
|    written count 1 (+0x14) and is writing the note: move.b 7(a2),0x18(a4) (a4 the event, a2 the
|    request). The builder still needs d1 after it, so this one keeps d0-d1/a0-a1, and calls
|    chprobe_req(event, request).
| (0.4: no sites use these; kept for when live chords come back.)
        .section .run, "ax"
        .globl  chprobe_site_a, chprobe_site_b, chprobe_site_c
chprobe_site_a:
        moveq   #0, %d1
        bra.s   1f
chprobe_site_b:
        moveq   #1, %d1
1:      move.b  %d0, 0x18(%a3)          | the instructions the jsr replaced
        moveq   #1, %d0
        move.l  %d0, 0x14(%a3)
        move.l  %d1, -(%sp)             | the path
        move.l  %a3, -(%sp)             | the event
        jsr     chprobe_live
        addq.l  #8, %sp
        rts

chprobe_site_c:
        move.b  7(%a2), 0x18(%a4)       | the instruction the jsr replaced
        lea     -16(%sp), %sp
        movem.l %d0-%d1/%a0-%a1, (%sp)
        move.l  %a2, -(%sp)             | the request
        move.l  %a4, -(%sp)             | the event
        jsr     chprobe_req
        addq.l  #8, %sp
        movem.l (%sp), %d0-%d1/%a0-%a1
        lea     16(%sp), %sp
        rts

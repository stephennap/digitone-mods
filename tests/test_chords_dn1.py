"""CHORDS (mods/chords-dn1): its scale table against the Digitone's own, and its chord rules.

    python tests/test_chords_dn1.py                                  # the chord rules
    ELEKLOADER_DN_SYX=Digitone_..._OS1.43.syx python tests/test_chords_dn1.py   # and the OS tables

The chord rules run on a Python copy of chordmath.h's ch_chord (this machine may have no host C
compiler); keep the two in step. The scale check reads the masks from chordmath.h itself.
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from elekloader import devices, formats  # noqa: E402

HERE = os.path.dirname(__file__)
HDR = os.path.join(HERE, '..', 'mods', 'chords-dn1', 'chordmath.h')
SYX = os.environ.get('ELEKLOADER_DN_SYX', '')
LOAD = 0x40000400
SCALES_143 = 0x401d4e68                 # 35 records of 13 bytes: each semitone's degree from 1, or 0
MODES_143 = 0x401d3ed0                  # 35 records of 7 bytes; 0xff first: not a 7-note scale
SHAPES_143 = 0x401d3fc5                 # the Note Add menu's chord shapes, 4 bytes each, 0xff unused


class Skip(Exception):
    pass


def masks():
    """ch_scale_mask from the header, evaluated."""
    src = open(HDR).read()
    body = src[src.index('ch_scale_mask[CH_SCALES] = {'):]
    body = body[body.index('{') + 1:body.index('};')]
    out = []
    for line in body.splitlines():
        line = re.sub(r'/\*.*?\*/', '', line).strip().rstrip(',')
        if not line:
            continue
        m = 0
        for part in (p.strip() for p in line.split('|')):
            g = re.fullmatch(r'M\(([\d,\s]+)\)', part)
            h = re.fullmatch(r'\(1u << (\d+)\)', part)
            if g:
                for n in g.group(1).split(','):
                    m |= 1 << int(n)
            elif h:
                m |= 1 << int(h.group(1))
            elif part != '0':
                raise AssertionError('cannot read %r' % part)
        out.append(m)
    assert len(out) == 35, len(out)
    return out


# --- a copy of chordmath.h's ch_chord ------------------------------------------------------------
NINTH = 9                               # the step above the note, an octave up, in any scale
STEPS = [[0, 2, 4], [0, 2, 4, 6], [0, 2, 4, 6, NINTH], [0, 1, 4], [0, 3, 4], [0, 2, 4, 5],
         [0, 2, 4, NINTH], [0, 4]]
SEMIS = [[0, 4, 7], [0, 3, 7], [0, 4, 7, 10], [0, 3, 7, 10], [0, 4, 7, 11], [0, 3, 6], [0, 2, 7],
         [0, 5, 7]]
ROOT, CENTER, SPREAD = range(3)


def chord(note, root, scale, shape, mode, center, bass, table):
    if not 0 <= note <= 127 or not 0 <= shape < 8 or not 0 <= mode < 3:
        return None
    if scale < 0:
        tones = [note + s for s in SEMIS[shape]]
    else:
        if scale >= 35 or not table[scale]:
            return None
        deg = [s for s in range(12) if table[scale] >> s & 1]
        k = len(deg)
        pc = (note - root) % 12
        if pc not in deg:
            return None
        d = deg.index(pc)
        tones = []
        for o in STEPS[shape]:
            j = d + (k + 1 if o == NINTH else o)
            t = note + deg[j % k] - deg[d] + 12 * (j // k)
            if t not in tones:
                tones.append(t)
    if mode != ROOT:
        placed, r = [], None
        for i, t in enumerate(tones):
            t = center + (t - center) % 12
            if t > center + 6:
                t -= 12
            if i == 0:
                r = t
            if t not in placed:
                placed.append(t)
        tones = sorted(placed)
        if mode == SPREAD:
            tones = [t + 12 if i % 2 else t for i, t in enumerate(tones)]
    else:
        r = tones[0]
    tones.sort()
    if bass > 0:
        at = next((i for i, t in enumerate(tones) if t % 12 == r % 12), None)
        if at is None:
            return None
        b = tones[at] - 12 * bass
        while b >= tones[0]:
            b -= 12
        if b >= 0:
            if len(tones) > 3:
                del tones[at]
            tones = [b] + tones
    if not 2 <= len(tones) <= 8:
        return None
    while tones[-1] > 127 and tones[0] >= 12:
        tones = [t - 12 for t in tones]
    if tones[0] < 0 or tones[-1] > 127 or len(set(tones)) != len(tones):
        return None
    return [tones[0]] + [t - tones[0] for t in tones[1:]]


def inversion(pcs, center, d):
    """chordmath.h's ch_inversion: pcs a set of note classes."""
    placed = []
    for p in pcs:
        t = center + (p - center) % 12
        if t > center + 6:
            t -= 12
        placed.append(t)
    if not placed:
        return center
    return min(placed) + 6 if d > 0 else max(placed) - 7
# ------------------------------------------------------------------------------------------------


def play(note, shape, mode=ROOT, center=60, bass=0, root=60, scale=0):
    r = chord(note, root, scale, shape, mode, center, bass, masks())
    return None if r is None else [r[0]] + [r[0] + i for i in r[1:]]


C2, C3, D3, E3, G3, A3, B3 = 36, 48, 50, 52, 55, 57, 59
C4, D4, E4, F4, G4, A4, B4 = 60, 62, 64, 65, 67, 69, 71
ION, AEOL, PMIN, WTONE, COMBO, HWDIM, CHROM = 0, 5, 7, 11, 13, 21, -1
S135, S1357, S13579, S125, S145, S1356, S1359, S15 = range(8)
MAJ, MIN, SEV, MIN7, MAJ7, DIM, SUS2, SUS4 = range(8)


def test_the_manuals_examples():
    """ROOT C, SCALE Ionian: C on 1-3-5 is C E G, D is D F A, C on 1-3-5-7 is C E G B."""
    assert play(C4, S135) == [C4, E4, G4]
    assert play(D4, S135) == [D4, F4, A4]
    assert play(C4, S1357) == [C4, E4, G4, B4]


def test_bass_oct_adds_a_root_below_or_replaces_it():
    """C3 on 1-3-5, BASS -1: C2 C3 E3 G3. C3 on 1-3-5-7, BASS -1: C2 E3 G3 B3."""
    assert play(C3, S135, bass=1, root=C3) == [C2, C3, E3, G3]
    assert play(C3, S1357, bass=1, root=C3) == [C2, E3, G3, B3]
    assert play(C3, S135, bass=2, root=C3) == [C3 - 24, C3, E3, G3]


def test_every_shape_in_a_scale():
    assert play(C4, S13579) == [C4, E4, G4, B4, D4 + 12]
    assert play(C4, S125) == [C4, D4, G4]
    assert play(C4, S145) == [C4, F4, G4]
    assert play(C4, S1356) == [C4, E4, G4, A4]
    assert play(C4, S1359) == [C4, E4, G4, D4 + 12]
    assert play(C4, S15) == [C4, G4]
    assert play(C4, S135, scale=AEOL) == [C4, 63, G4]           # C minor
    assert play(B4, S135) == [B4, D4 + 12, F4 + 12]             # B diminished


def test_chromatic_shapes_are_chord_types():
    assert play(61, MAJ, scale=CHROM) == [61, 65, 68]
    assert play(C4, MIN7, scale=CHROM) == [C4, 63, G4, 70]
    assert play(C4, SUS4, scale=CHROM) == [C4, F4, G4]


def test_center_keeps_the_hand_in_place():
    """With CENTER at C4 each note takes the octave within C4-5 .. C4+6, so chords change by
    inversion around C4 rather than climbing the keys."""
    assert play(C4, S135, CENTER, C4) == [G3, C4, E4]           # C as G C E
    assert play(G4, S135, CENTER, C4) == [G3, B3, D4]           # G in root position
    assert play(A4, S135, CENTER, C4) == [A3, C4, E4]           # Am as A C E
    assert play(F4, S135, CENTER, C4) == [A3, C4, F4]           # F as A C F
    assert play(F4 - 24, S135, CENTER, C4) == [A3, C4, F4]      # the played octave does not matter
    every = [play(n, S135, CENTER, C4) for n in (C4, D4, E4, F4, G4, A4, B4)]
    assert all(C4 - 5 <= t <= C4 + 6 for ch in every for t in ch)


def test_inversion_steps_move_the_center_one_inversion():
    """The Digitone II's [+]/[-]: knob B. C major around C4 (MIDI 60): G C E, then C E G, E G C."""
    cmaj = {0, 4, 7}
    up = inversion(cmaj, C4, 1)
    assert play(C4, S135, CENTER, up) == [C4, E4, G4]
    up2 = inversion(cmaj, up, 1)
    assert play(C4, S135, CENTER, up2) == [E4, G4, C4 + 12]
    assert play(C4, S135, CENTER, inversion(cmaj, C4, -1)) == [E3, G3, C4]


def test_each_inversion_step_moves_exactly_one_note_an_octave():
    t = masks()
    for scale in (0, 5, 10, 7, 20, -1):
        for note in range(48, 72):
            for shape in range(8):
                for c in range(48, 84, 5):
                    r = chord(note, 60, scale, shape, CENTER, c, 0, t)
                    if r is None:
                        continue
                    now = [r[0] + (i and r[i]) for i in range(len(r))]
                    pcs = {x % 12 for x in now}
                    for d in (1, -1):
                        c2 = inversion(pcs, c, d)
                        r2 = chord(note, 60, scale, shape, CENTER, c2, 0, t)
                        nxt = [r2[0] + (i and r2[i]) for i in range(len(r2))]
                        moved = sorted(set(nxt) - set(now))
                        gone = sorted(set(now) - set(nxt))
                        if d > 0:
                            assert gone == [min(now)] and moved == [min(now) + 12], (now, nxt)
                        else:
                            assert gone == [max(now)] and moved == [max(now) - 12], (now, nxt)


def test_spread_lifts_every_second_note():
    # CENTER's G3 C4 E4 with the second note up: G3 E4 C5
    assert play(C4, S135, SPREAD, C4) == [G3, E4, C4 + 12]
    # CENTER's G3 B3 C4 E4: G3 C4 B4 E5
    assert play(C4, S1357, SPREAD, C4) == [G3, C4, B4, E4 + 12]


def test_bass_under_center_and_spread():
    # G on 1-3-5 centered at C4 is G3 B3 D4; BASS -1 adds G2 under it
    assert play(G4, S135, CENTER, C4, bass=1) == [G3 - 12, G3, B3, D4]
    # G7 is 4 notes, so the bass takes the root's place: G2 B3 D4 F4
    assert play(G4, S1357, CENTER, C4, bass=1) == [G3 - 12, B3, D4, F4]
    # SPREAD: the bass goes under the lowest note, whatever got lifted
    assert play(C4, S135, SPREAD, C4, bass=1) == [C3, G3, E4, C4 + 12]


def test_stock_when_it_cannot_say():
    assert play(61, S135) is None                               # C# is not in C major
    assert play(C4, S135, scale=35) is None                     # no such SCALE


def test_every_scale_makes_chords():
    t = masks()
    for scale in range(35):
        assert t[scale], scale
        assert play(C4, S135, scale=scale) is not None, scale


def test_the_ninth_is_the_second_an_octave_up_in_any_scale():
    # 7 notes: Cmaj9's D5
    assert play(C4, S13579)[-1] == D4 + 12
    # 8 notes (Combo Minor C D Eb F G Ab Bb B): the 9th is D, not the octave C
    assert play(C4, S1359, scale=COMBO) == [C4, 63, G4, D4 + 12]
    # Half-Whole Diminished (C Db Eb E F# G A Bb): C dim7 and its b9, Db
    assert play(C4, S13579, scale=HWDIM) == [C4, 63, 66, A4, 61 + 12]
    # 6 notes (Whole Tone C D E F# G# Bb): the 9th is D, not E again
    assert play(C4, S1359, scale=WTONE) == [C4, E4, 68, D4 + 12]
    # 5 notes: a 7th and 9th that land on the same note are kept once
    r = play(C4, S13579, scale=PMIN)
    assert r is not None and len(r) == len(set(r)) == 4


def test_small_scales_keep_each_note_once():
    for mode in (CENTER, SPREAD):
        r = play(C4, S13579, mode, C4, scale=PMIN)
        assert r is not None and len(set(t % 12 for t in r)) == len(r)


def test_every_chord_is_safe_for_the_firmware():
    """Every result: 2-8 notes, notes 0-127, intervals 1-127, for every input."""
    t = masks()
    for scale in range(-1, 35):
        for note in range(0, 128, 1):
            for shape in range(8):
                for mode in range(3):
                    for center in (36, 60, 96) if mode else (60,):
                        for bass in range(4):
                            r = chord(note, 60, scale, shape, mode, center, bass, t)
                            if r is None:
                                continue
                            assert 2 <= len(r) <= 8
                            assert all(1 <= i <= 127 for i in r[1:])
                            assert 0 <= r[0] and r[0] + max(r[1:]) <= 127


def test_header_masks_match_the_os_scale_table():
    """All 35 scales are the OS's own: its table of each semitone's degree."""
    img = stock_image()
    t = masks()
    for k in range(35):
        rec = img[SCALES_143 - LOAD + 13 * k:SCALES_143 - LOAD + 13 * k + 13]
        notes = [s for s in range(12) if rec[s]]
        assert [rec[s] for s in notes] == list(range(1, len(notes) + 1)) and rec[12] == len(notes) + 1
        want = sum(1 << s for s in notes)
        assert t[k] == want, 'scale %d: header %03x, OS %03x' % (k, t[k], want)


def test_header_masks_match_the_os_mode_table():
    img = stock_image()
    t = masks()
    checked = 0
    for k in range(35):
        rec = img[MODES_143 - LOAD + 7 * k:MODES_143 - LOAD + 7 * k + 7]
        if rec[0] == 0xff:              # the OS keeps this scale elsewhere
            continue
        want = 0
        for b in rec:
            if b < 12:
                want |= 1 << b
        assert t[k] == want, 'scale %d: header %03x, OS %03x' % (k, t[k], want)
        checked += 1
    assert checked == 23, checked       # 1.43 keeps 23 of its 35 scales in this table


def test_chromatic_types_are_the_os_chord_shapes():
    """MAJ ... SUS4 are the Note Add menu's own shapes 0-5, 7 and 8."""
    img = stock_image()
    os_shapes = [[b for b in img[SHAPES_143 - LOAD + 4 * k:SHAPES_143 - LOAD + 4 * k + 4] if b != 0xff]
                 for k in range(12)]
    assert SEMIS == [os_shapes[k] for k in (0, 1, 2, 3, 4, 5, 7, 8)], os_shapes


def stock_image():
    if not SYX or not os.path.exists(SYX):
        raise Skip('missing ELEKLOADER_DN_SYX')
    dev = [d for d in devices.devices() if d.key == 'digitone-mk1'][0]
    st, d, rel = formats.load(open(SYX, 'rb').read())
    if rel != dev.releases['1.43']:
        raise Skip('the table addresses are 1.43\'s')
    return formats.main_image(st, dev)


if __name__ == '__main__':
    ok = bad = skip = 0
    for n in sorted(k for k in list(globals()) if k.startswith('test_')):
        try:
            globals()[n]()
            print('ok     ', n)
            ok += 1
        except Skip as e:
            print('skip   ', n, '(%s)' % e)
            skip += 1
        except Exception as e:      # noqa: BLE001
            print('FAIL   ', n, '%s: %s' % (type(e).__name__, e))
            bad += 1
    print('%d passed, %d failed, %d skipped' % (ok, bad, skip))
    sys.exit(1 if bad else 0)

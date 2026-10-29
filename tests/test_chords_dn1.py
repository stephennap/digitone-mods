"""CHORDS (mods/chords-dn1): its scale table against the Digitone's own, and its chord rules.

    python tests/test_chords_dn1.py                                  # the chord rules
    ELEKLOADER_DN_SYX=Digitone_..._OS1.43.syx python tests/test_chords_dn1.py   # and the scales

The chord rules run on a Python copy of chordmath.h's ch_build (this machine may have no host C
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
SCALES_143 = 0x401d3ed0                 # 35 records of 7 bytes; -1 first: not a 7-note scale
LOAD = 0x40000400


class Skip(Exception):
    pass


def masks():
    """ch_scale_mask from the header, evaluated."""
    src = open(HDR).read()
    body = src[src.index('ch_scale_mask[CH_SCALES] = {'):]
    body = body[body.index('{') + 1:body.index('};')]
    out = []
    for line in body.splitlines():
        line = re.sub(r'/\*.*?\*/', '', line).strip()
        if not line:
            continue
        line = line.rstrip(',')
        m = 0
        for part in line.split('|'):
            part = part.strip()
            g = re.fullmatch(r'M\(([\d,\s]+)\)', part)
            if g:
                for n in g.group(1).split(','):
                    m |= 1 << int(n)
            elif re.fullmatch(r'\(1u << (\d+)\)', part):
                m |= 1 << int(re.fullmatch(r'\(1u << (\d+)\)', part).group(1))
            elif part == '0':
                pass
            else:
                raise AssertionError('cannot read %r' % part)
        out.append(m)
    assert len(out) == 35, len(out)
    return out


# --- a copy of chordmath.h ---------------------------------------------------------------------
DEGREES = [None, [0, 2, 4], [0, 1, 4], [0, 3, 4], [0, 2, 4, 5], [0, 2, 4, 6], [0, 2, 4, 6, 8],
           [0, 2, 4, 8]]
CHROMATIC = [None, [0, 4, 7], [0, 2, 7], [0, 5, 7], [0, 4, 7, 9], [0, 4, 7, 10], [0, 4, 7, 10, 14],
             [0, 4, 7, 14]]
OFF, TRIAD, SUS2, SUS4, SIXTH, SEVENTH, NINTH, ADD9 = range(8)
CLOSE, INV1, INV2, INV3, OPEN = range(5)


def build(note, root, scale, typ, voicing, table):
    if not 0 <= note <= 127 or not OFF < typ < 8:
        return None
    if scale < 0:
        tones = [note + s for s in CHROMATIC[typ]]
    else:
        if scale >= 35 or not table[scale]:
            return None
        deg = [s for s in range(12) if table[scale] >> s & 1]
        k = len(deg)
        pc = (note - root) % 12
        if pc not in deg:
            return None
        d = deg.index(pc)
        tones = [note + deg[(d + o) % k] - deg[d] + 12 * ((d + o) // k) for o in DEGREES[typ]]
    if INV1 <= voicing <= INV3:
        for i in range(min(voicing, len(tones) - 1)):
            tones[i] += 12
    elif voicing == OPEN and len(tones) >= 3:
        tones[1] += 12
    tones.sort()
    while tones[-1] > 127 and tones[0] >= 12:
        tones = [t - 12 for t in tones]
    if tones[-1] > 127 or len(set(tones)) != len(tones):
        return None
    return [tones[0]] + [t - tones[0] for t in tones[1:]]
# ------------------------------------------------------------------------------------------------


def names(r):
    """notes[] -> absolute MIDI notes."""
    return [r[0]] + [r[0] + i for i in r[1:]]


C4, D4, E4, G4, B4 = 60, 62, 64, 67, 71
ION, AEOL, PMIN, BLUES, COMBO = 0, 5, 7, 12, 13


def test_major_key_triads_follow_the_scale():
    t = masks()
    assert names(build(C4, C4, ION, TRIAD, CLOSE, t)) == [60, 64, 67]     # C E G
    assert names(build(D4, C4, ION, TRIAD, CLOSE, t)) == [62, 65, 69]     # D F A: minor
    assert names(build(B4, C4, ION, TRIAD, CLOSE, t)) == [71, 74, 77]     # B D F: diminished


def test_minor_key_and_sevenths():
    t = masks()
    assert names(build(C4, C4, AEOL, TRIAD, CLOSE, t)) == [60, 63, 67]    # C Eb G
    assert names(build(G4, C4, ION, SEVENTH, CLOSE, t)) == [67, 71, 74, 77]   # G7
    assert names(build(C4, C4, ION, NINTH, CLOSE, t)) == [60, 64, 67, 71, 74]  # Cmaj9


def test_root_in_another_octave_and_key():
    t = masks()
    # D major (ROOT D3 = 50): E plays E G B, a minor chord
    assert names(build(E4, 50, ION, TRIAD, CLOSE, t)) == [64, 67, 71]


def test_voicings():
    t = masks()
    assert names(build(C4, C4, ION, TRIAD, INV1, t)) == [64, 67, 72]      # E G C
    assert names(build(C4, C4, ION, TRIAD, INV2, t)) == [67, 72, 76]      # G C E
    assert names(build(C4, C4, ION, TRIAD, INV3, t)) == [67, 72, 76]      # a triad stops at INV 2
    assert names(build(C4, C4, ION, SEVENTH, INV3, t)) == [71, 72, 76, 79]
    assert names(build(C4, C4, ION, TRIAD, OPEN, t)) == [60, 67, 76]      # C G E


def test_five_note_scale_stacks_its_own_degrees():
    t = masks()
    # C minor pentatonic C Eb F G Bb: every other degree from C is C F Bb
    assert names(build(C4, C4, PMIN, TRIAD, CLOSE, t)) == [60, 65, 70]


def test_stock_when_it_cannot_say():
    t = masks()
    assert build(61, C4, ION, TRIAD, CLOSE, t) is None                    # C# is not in C major
    assert build(C4, C4, COMBO, TRIAD, CLOSE, t) is None                  # a scale it does not know
    assert build(C4, C4, ION, OFF, CLOSE, t) is None


def test_chromatic_uses_fixed_shapes():
    t = masks()
    assert names(build(61, C4, -1, TRIAD, CLOSE, t)) == [61, 65, 68]      # C# major
    assert names(build(C4, C4, -1, SEVENTH, CLOSE, t)) == [60, 64, 67, 70]


def test_every_chord_is_safe_for_the_firmware():
    """Every result: notes 0-127, intervals 1-127, at most 8 notes, for every input."""
    t = masks()
    for scale in range(-1, 35):
        for root in (0, 60, 127):
            for note in range(128):
                for typ in range(1, 8):
                    for v in range(5):
                        r = build(note, root, scale, typ, v, t)
                        if r is None:
                            continue
                        assert 1 < len(r) <= 8
                        assert all(1 <= i <= 127 for i in r[1:])
                        assert all(0 <= n <= 127 for n in names(r))


def test_header_masks_match_the_os_scale_table():
    if not SYX or not os.path.exists(SYX):
        raise Skip('missing ELEKLOADER_DN_SYX')
    dev = [d for d in devices.devices() if d.key == 'digitone-mk1'][0]
    st, d, rel = formats.load(open(SYX, 'rb').read())
    assert d is dev and rel == dev.releases['1.43'], 'the table address is 1.43\'s'
    img = formats.main_image(st, dev)
    t = masks()
    checked = 0
    for k in range(35):
        rec = img[SCALES_143 - LOAD + 7 * k:SCALES_143 - LOAD + 7 * k + 7]
        if rec[0] == 0xff:              # the OS keeps this scale elsewhere
            continue
        want = 0
        for b in rec:
            if b < 12:
                want |= 1 << b
        assert t[k] == want, 'scale %d: header %03x, OS %03x' % (k, t[k], want)
        checked += 1
    assert checked == 23, checked       # 1.43 keeps 23 of its 35 scales in this table


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

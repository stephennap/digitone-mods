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
# The SHAPEs (in a scale: steps) and TYPEs (on chromatic: semitones), in the page's order
STEPS = [[0, 2, 4], [0, 1, 4], [0, 2], [0, 3], [0, 4], [0, 3, 4], [0, 2, 4, 6], [0, 3, 6]]
SEMIS = [[0, 4, 7], [0, 3, 7], [0, 4, 7, 10], [0, 3, 7, 10], [0, 4, 7, 11], [0, 3, 6], [0, 3, 6, 9],
         [0, 2, 7], [0, 5, 7], [0, 4, 7, 9], [0, 3, 7, 9], [0, 2, 4, 7]]
ROOT, CENTER, SPREAD = range(3)


def chord(note, root, scale, shape, mode, center, bass, table):
    if not 0 <= note <= 127 or shape < 0 or not 0 <= mode < 3:
        return None
    shape = min(shape, (12 if scale < 0 else 8) - 1)
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
            j = d + o
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
S135, S125, S13, S14, S15, S145, S1357, S147 = range(8)
MAJ, MIN, SEV, MIN7, MAJ7, DIM, DIM7, SUS2, SUS4, SIX, MIN6, ADD9 = range(12)


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
    """The eight, on C in C major."""
    assert play(C4, S125) == [C4, D4, G4]
    assert play(C4, S13) == [C4, E4]
    assert play(C4, S14) == [C4, F4]
    assert play(C4, S15) == [C4, G4]
    assert play(C4, S145) == [C4, F4, G4]
    assert play(C4, S147) == [C4, F4, B4]                       # stacked fourths
    assert play(D4, S147) == [D4, G4, C4 + 12]
    assert play(C4, S135, scale=AEOL) == [C4, 63, G4]           # C minor
    assert play(B4, S135) == [B4, D4 + 12, F4 + 12]             # B diminished


def test_chromatic_shapes_are_chord_types():
    assert play(61, MAJ, scale=CHROM) == [61, 65, 68]
    assert play(C4, MIN7, scale=CHROM) == [C4, 63, G4, 70]
    assert play(C4, SUS4, scale=CHROM) == [C4, F4, G4]
    assert play(C4, DIM7, scale=CHROM) == [C4, 63, 66, A4]
    assert play(C4, SIX, scale=CHROM) == [C4, E4, G4, A4]
    assert play(C4, MIN6, scale=CHROM) == [C4, 63, G4, A4]
    assert play(C4, ADD9, scale=CHROM) == [C4, D4, E4, G4]


def test_a_type_past_a_scales_shapes_is_its_last_shape():
    """SHAPE keeps the knob's value when SCALE changes: a TYPE 9-12 is 1-4-7 in a scale."""
    assert play(C4, ADD9) == play(C4, S147)


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
    """Knob B. C major around C4 (MIDI 60): G C E, then C E G, E G C."""
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


def test_other_scale_sizes():
    # Half-Whole Diminished (C Db Eb E F# G A Bb): 1-3-5-7 is C dim7
    assert play(C4, S1357, scale=HWDIM) == [C4, 63, 66, A4]
    # Whole Tone (C D E F# G# Bb): 1-3-5 is C augmented
    assert play(C4, S135, scale=WTONE) == [C4, E4, 68]
    # Pentatonic Minor (C Eb F G Bb): 1-3-5-7 stacks its own steps, the 7th the 2nd an octave up
    assert play(C4, S1357, scale=PMIN) == [C4, F4, 70, 63 + 12]


def test_small_scales_keep_each_note_once():
    for mode in (CENTER, SPREAD):
        r = play(C4, S1357, mode, C4, scale=PMIN)
        assert r is not None and len(set(t % 12 for t in r)) == len(r)


def test_every_chord_is_safe_for_the_firmware():
    """Every result: 2-8 notes, notes 0-127, intervals 1-127, for every input."""
    t = masks()
    for scale in range(-1, 35):
        for note in range(0, 128, 1):
            for shape in range(12):
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
    """The 12 TYPEs, MAJ ... ADD9, are the mk1's Note Add shapes in order."""
    img = stock_image()
    os_shapes = [[b for b in img[SHAPES_143 - LOAD + 4 * k:SHAPES_143 - LOAD + 4 * k + 4] if b != 0xff]
                 for k in range(12)]
    assert SEMIS == os_shapes, os_shapes


def test_header_tables_match_this_copy():
    """chordmath.h's shape and type tables are the ones this file tests."""
    src = open(HDR).read()
    def table(name):
        body = src[src.index(name):]
        body = body[body.index('{') + 1:body.index('};')]
        rows = re.findall(r'\{([^}]*)\}', body)
        return [[int(x) for x in r.split(',') if x.strip() and int(x) >= 0] for r in rows]
    assert table('ch_shape_steps[CH_SHAPES]') == STEPS
    assert table('ch_type_semis[CH_TYPES]') == SEMIS


def stock_image():
    if not SYX or not os.path.exists(SYX):
        raise Skip('missing ELEKLOADER_DN_SYX')
    dev = [d for d in devices.devices() if d.key == 'digitone-mk1'][0]
    st, d, rel = formats.load(open(SYX, 'rb').read())
    if rel != dev.releases['1.43']:
        raise Skip('the table addresses are 1.43\'s')
    return formats.main_image(st, dev)


# ---- RETRIG (retrig.inc): its tables, its storage in the MODE and BASS locks' low bytes, its fade

def retrig_source():
    with open(os.path.join(HERE, '..', 'mods', 'chords-dn1', 'retrig.inc')) as f:
        return f.read()


def retrig_table(name):
    m = re.search(r'%s\[[A-Z_]+\] = \{([^}]*)\}' % name, retrig_source())
    return [x.strip() for x in m.group(1).split(',')]


RT_BAR = 86400000
RT_EIGHTH = 675000


def rt_store(rate, length, vel):          # retrig.inc's rt_write, as the two low bytes
    return rate, ((length - 5) & 15) | (vel & 15) << 4


def rt_load(mode_lo, bass_lo):            # its rt_read
    v = (bass_lo ^ 0x80) - 0x80
    return (mode_lo if mode_lo < 18 else 0), (bass_lo + 5) & 15, v >> 4


def rt_vel(v0, f, p):                     # its rt_vel (C division truncates toward 0)
    n = v0 * (2048 + f * p) if f < 0 else v0 * (2048 - f * (256 - p))
    v = int(n / 2048)
    return max(1, min(127, v))


def test_retrig_rates_are_whole_intervals():
    rates = [int(x) for x in retrig_table('rt_rates')]
    assert rates[0] == 0 and len(rates) == 18 and rates[1:] == sorted(rates[1:])
    assert rates[1:] == [1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64, 80]    # the Digitakt's
    for n in rates[1:]:
        assert RT_BAR % n == 0, n


def test_retrig_lens_and_names_agree():
    lens = [int(x) for x in retrig_table('rt_lens')]
    names = [x.strip('"') for x in retrig_table('rt_len_names')]
    assert len(lens) == len(names) == 16 and lens[15] == 0 and names[15] == 'INF'
    for e, name in zip(lens[:15], names[:15]):
        if '/' in name:
            a, b = name.split('/')
            assert e * int(b) == 8 * int(a), name
        else:
            assert e == 8 * float(name), name
    assert lens[5] == 8                     # a fresh lock's 0 is one step


def test_retrig_storage_round_trips():
    for rate in range(18):
        for length in range(16):
            for vel in range(-8, 8):
                mode_lo, bass_lo = rt_store(rate, length, vel)
                assert 0 <= mode_lo < 256 and 0 <= bass_lo < 256
                assert rt_load(mode_lo, bass_lo) == (rate, length, vel)
    assert rt_store(0, 5, 0) == (0, 0)      # retrig off, defaults: no lock is written


RT_ALONE_MODE, RT_ALONE_BASS = 0x0080, 0x4000      # chords.c's RT_ALONE


def rt_word(knob, hix, lo, alone):        # retrig.inc's rt_set: a knob's lock (knob 0-3) or retrig's alone
    if knob is not None:
        return (knob | hix) << 8 | lo
    return (hix << 8 | lo) | alone if hix or lo else -1


def knob_locked(w, alone):                # chords.c's ch_knob_locked
    return w >= 0 and not w & alone


def saved(w):                             # the pattern's save and reload (1.44: 0x40008ee6): w >= 0 kept
    return w if w >= 0 else -1


def test_retrig_alone_survives_save_and_never_reads_as_a_knob_lock():
    for rate in range(18):
        for pitch in range(29):
            w = rt_word(None, pitch << 2, rate, RT_ALONE_MODE)
            if w == -1:
                continue
            assert saved(w) == w and not knob_locked(w, RT_ALONE_MODE)
            assert (w >> 8) & 0x7c == pitch << 2 and w & 0x7f == rate     # rt_hix, rt_lo
    for gate in range(8):
        for lo in range(256):
            w = rt_word(None, gate << 2, lo, RT_ALONE_BASS)
            if w == -1:
                continue
            assert saved(w) == w and not knob_locked(w, RT_ALONE_BASS)
            assert (w >> 8) & 0x1c == gate << 2 and w & 0xff == lo


def test_a_knob_lock_with_retrig_stays_a_knob_lock():
    for knob in range(4):
        for rate in range(18):
            for pitch in range(29):
                w = rt_word(knob, pitch << 2, rate, RT_ALONE_MODE)
                assert saved(w) == w and knob_locked(w, RT_ALONE_MODE) and (w >> 8) & 3 == knob
        for gate in range(8):
            for lo in range(256):
                w = rt_word(knob, gate << 2, lo, RT_ALONE_BASS)
                assert saved(w) == w and knob_locked(w, RT_ALONE_BASS) and (w >> 8) & 3 == knob


def test_old_retrig_alone_reads_as_no_knob_lock():
    for lo in range(1, 256):              # 0.14-0.19: the high byte 0x80
        w = (0x80 << 8 | lo) - 0x10000
        assert not knob_locked(w, RT_ALONE_MODE) and not knob_locked(w, RT_ALONE_BASS)


def test_retrig_fade():
    assert rt_vel(100, 0, 0) == rt_vel(100, 0, 256) == 100
    assert rt_vel(100, -8, 0) == 100 and rt_vel(100, -8, 256) == 1      # fades out to nothing
    assert rt_vel(100, -4, 256) == 50
    assert rt_vel(120, 7, 0) == 15 and rt_vel(120, 7, 256) == 120       # fades in from an eighth
    for f in range(-8, 8):
        seq = [rt_vel(127, f, p) for p in range(0, 257, 16)]
        assert seq == (sorted(seq) if f > 0 else sorted(seq, reverse=True)), f


def test_retrig_page_glyphs():
    # every character the page draws big has a glyph in chords.c's font
    with open(os.path.join(HERE, '..', 'mods', 'chords-dn1', 'chords.c')) as f:
        glyphs = set(re.findall(r"\{'(.)', \{", f.read()))
    names = [x.strip('"') for x in retrig_table('rt_len_names')]
    gates = [x.strip('"') for x in retrig_table('rt_gate_names')]
    drawn = set(''.join(names + gates)) | set('OFF1/80') | set('-0123456789')
    assert drawn <= glyphs, drawn - glyphs


# ---- RETRIG's GATE and PITCH (retrig.inc 0.18): bits 2-6 of the MODE and BASS locks' high bytes

def rt_pitch_step(code):
    return code if 1 <= code <= 12 else 12 - code if 13 <= code <= 24 else 0


def rt_pitch_pos(code):
    return code if code >= 25 else 12 + rt_pitch_step(code)


def rt_pitch_code(pos):
    return pos if pos >= 25 else pos - 12 if pos > 12 else 24 - pos if pos < 12 else 0


def rt_degree(note, d, root, mask):
    if not mask:
        return note + d
    while d > 0:
        note += 1
        if mask >> ((note - root) % 12) & 1:
            d -= 1
    while d < 0:
        note -= 1
        if mask >> ((note - root) % 12) & 1:
            d += 1
    return note


def c_mod(a, b):                          # C's %: the sign of the dividend
    return a - b * int(a / b)


def rt_arp(code, k, n):
    if code == 25:
        return k % n
    if code == 26:
        return n - 1 - k % n
    period = 2 * n - 2 if n > 1 else 1
    i = k % period
    return period - i if i >= n else i


def test_retrig_pitch_knob_order():
    codes = [rt_pitch_code(pos) for pos in range(29)]
    assert sorted(codes) == list(range(29))                 # every code once
    assert [rt_pitch_step(c) for c in codes[:25]] == list(range(-12, 13))
    assert codes[12] == 0 and codes[25:] == [25, 26, 27, 28]
    for c in range(29):
        assert rt_pitch_code(rt_pitch_pos(c)) == c


def test_retrig_pitch_steps_walk_the_scale_and_wrap():
    ionian = 0b101010110101                 # C major from root C
    seq = [rt_degree(60, c_mod(1 * k, 14), 60, ionian) for k in range(16)]
    assert seq[:8] == [60, 62, 64, 65, 67, 69, 71, 72]
    assert seq[14] == 60 and seq[15] == 62  # two octaves, then from the start again
    down = [rt_degree(60, c_mod(-2 * k, 14), 60, ionian) for k in range(4)]
    assert down == [60, 57, 53, 50]         # down two degrees a hit: A, F, D
    assert [rt_degree(60, k, 60, 0) for k in range(3)] == [60, 61, 62]     # chromatic: semitones


def test_retrig_arp_orders():
    n = 3
    assert [rt_arp(25, k, n) for k in range(6)] == [0, 1, 2, 0, 1, 2]
    assert [rt_arp(26, k, n) for k in range(6)] == [2, 1, 0, 2, 1, 0]
    assert [rt_arp(27, k, n) for k in range(6)] == [0, 1, 2, 1, 0, 1]
    assert [rt_arp(27, k, 1) for k in range(3)] == [0, 0, 0]


def test_retrig_high_bits_leave_the_knobs():
    for knob in range(4):
        for pitch in range(29):
            hi = knob | pitch << 2
            assert hi < 0x80 and hi & 3 == knob          # ch_value's mask gives the knob back
    for gate in range(8):
        assert (3 | gate << 2) & 3 == 3 and not (3 | gate << 2) & 0x40       # BASS OCT's RT_ALONE bit free


def test_retrig_tables_match_the_source():
    src = retrig_source()
    assert 'rt_gates[RT_GATES] = { 0, 1, 2, 3, 4, 5, 6, 8 }' in src
    assert '#define RT_HI_BITS   0x7c' in src
    with open(os.path.join(HERE, '..', 'mods', 'chords-dn1', 'chords.c')) as f:
        c = f.read()
    assert 'v &= 3;' in c
    assert '#define RT_ALONE(slot) ((slot) == SLOT_MODE ? 0x0080 : 0x4000)' in c
    assert 'return slot == SLOT_MODE ? RT_HI_BITS : 0x1c;' in src

# --- the scale lock: copies of chordmath.h's ch_snap and chords.c's ch_snap_notes -----------------
def snap(note, root, scale, d, table):
    if scale < 0 or not 0 <= note <= 127:
        return note
    m = table[scale]

    def inside(n):
        return m >> ((n - root) % 12) & 1
    if inside(note):
        return note
    up = note + 1
    while up <= 127 and not inside(up):
        up += 1
    dn = note - 1
    while dn >= 0 and not inside(dn):
        dn -= 1
    if up > 127:
        return dn
    if dn < 0 or d > 0:
        return up
    if d < 0:
        return dn
    return up if up - note < note - dn else dn


def snap_notes(notes, root, scale, table):
    if scale < 0:
        return list(notes)
    b = notes[0]
    nb = snap(b, root, scale, 0, table)
    a = [0] * len(notes)
    for i in range(1, len(notes)):
        if not notes[i]:
            continue
        t = snap(b + notes[i], root, scale, 0, table)
        if any(a[j] and nb + a[j] == t for j in range(1, i)):
            t = nb
        if t <= nb:
            t = b + notes[i]
        a[i] = t - nb if nb < t <= nb + 127 else 0
    return [nb] + a[1:]


ION = 0                                 # SCALE 0, Ionian (-1 is chromatic)


def test_scale_lock_leaves_scale_notes_and_chromatic_alone():
    t = masks()
    for n in (60, 62, 64, 65, 67, 69, 71, 72):
        assert snap(n, 60, ION, 0, t) == n
    for n in range(128):
        assert snap(n, 60, -1, 1, t) == n


def test_scale_lock_rounds_to_the_nearest_lower_on_a_tie_or_the_way_it_went():
    t = masks()
    assert snap(61, 60, ION, 0, t) == 60          # C#: C and D tie, the lower
    assert snap(61, 60, ION, 1, t) == 62          # turned up: D
    assert snap(61, 60, ION, -1, t) == 60         # turned down: C
    assert snap(66, 60, ION, 0, t) == 65          # F#: F
    assert snap(66, 62, ION, 0, t) == 66          # in D major F# is the scale's own


def test_scale_lock_lands_in_every_scale_nearest_and_in_range():
    t = masks()
    for scale in range(35):
        for root in range(60, 72):
            inside = [n for n in range(128) if t[scale] >> ((n - root) % 12) & 1]
            for n in range(128):
                for d in (-1, 0, 1):
                    s = snap(n, root, scale, d, t)
                    assert s in inside, (scale, root, n, d, s)
                    if d > 0 and s != n and any(x > n for x in inside):
                        assert s > n
                    if d < 0 and s != n and any(x < n for x in inside):
                        assert s < n
                if n not in inside:
                    s = snap(n, root, scale, 0, t)
                    assert abs(s - n) == min(abs(x - n) for x in inside), (scale, root, n)


def test_knob_f_walks_the_scale_a_note_a_detent():
    t = masks()
    note, out = 60, [60]
    for _ in range(7):                  # chords_enc's F with a trig held: note + 1, snapped up
        note = snap(min(note + 1, 127), 60, ION, 1, t)
        out.append(note)
    assert out == [60, 62, 64, 65, 67, 69, 71, 72]
    note = 61                           # a stored note outside the scale: the next one the way it went
    assert snap(note + 1, 60, ION, 1, t) == 62 and snap(note - 1, 60, ION, -1, t) == 60


def test_scale_lock_on_a_stored_chord():
    t = masks()
    assert snap_notes([61, 3, 7, 0, 0, 0, 0, 0], 60, ION, t) == [60, 4, 7, 0, 0, 0, 0, 0]
    assert snap_notes([60, 1, 0, 0, 0, 0, 0, 0], 60, ION, t) == [60, 1, 0, 0, 0, 0, 0, 0]   # C# onto C: kept
    assert snap_notes([60, 4, 5, 0, 0, 0, 0, 0], 60, ION, t) == [60, 4, 5, 0, 0, 0, 0, 0]
    assert snap_notes([61, 0, 0, 0, 0, 0, 0, 0], 60, -1, t) == [61, 0, 0, 0, 0, 0, 0, 0]


def test_scale_lock_copies_match_the_source():
    with open(HDR) as f:
        src = f.read()
    assert 'return up - note < note - dn ? up : dn;' in src
    assert 'if (dn < 0 || dir > 0)' in src
    with open(os.path.join(HERE, '..', 'mods', 'chords-dn1', 'chords.c')) as f:
        c = f.read()
    assert 'a[i] = t > nb && t - nb <= 127 ? t - nb : 0;' in c
    assert 'ch_snap(ch_clamp(note + s, 0, 127), v.root, v.scale, s)' in c


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

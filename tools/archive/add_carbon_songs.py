#!/usr/bin/env python3
# add_carbon_songs.py - step 3 of the song archive: adds Carbon's 30 songs to the archive of step 2 (add_ug1_songs.py) = the final sdat.viv.
# Reads only its inputs. Every check runs before the first output byte is written; the output is the same bytes on every run.
#  1. MusicSFx.mus = step 2's file unchanged (UG2's 43 waves + MW's 26 + UG1's 27 at their offsets) + Carbon's 30 song waves appended in
#     Carbon's own wave order: cb_mus_0 wave w (2..31) -> UG2 wave 95 + w (97..126). Carbon's wave 1 (a 30.5-s piece with no song row) is not
#     taken. Each wave gets the same header tag set as the MW streams ('06 01 65 | FD | 80 01 02 | 85 0N <count> | 82 01 02 | 84 02 8C A0 | FF':
#     Carbon's rate 36,000 kept, in 2 bytes; N = Carbon's own count length, 3 up to 0x7FFFFF, 4 above = EA's rule) + Carbon's 'SCCl'..'SCEl'
#     bytes unchanged + zero padding to 0x80. Dropped: 0x13 = 0x7F, 0x9C = 0xF05A, 0x9D = 0x105A (Carbon-only tags, in neither UG2's own
#     waves nor the MW streams; check_decode_carbon.sh proves the PCM unchanged).
#  2. MusicSFx.mpf = step 2's 7,264 B with ONE word changed ([0x195C] 0x718 -> 0x754 = the new table end / 4) + 30 entries
#     {offset / 0x80, floor(count * 1000 / 36000)} = 7,504 B, which must equal the expected map (data/MusicSFx.mpf, 34cba302..) byte for byte.
#     No node is changed.
#  3. sdat.viv = the BIG4 archive rebuilt (the same 10 names in the same order, 0x40-aligned, header 0x140); the other 8 entries byte-identical;
#     893,398,656 B.
#  4. streams/cb_NN.bin (the 30 re-headered streams = the bytes appended; NN = the Carbon wave) + streams/cbo_NN.bin (the 30 Carbon spans as
#     they are in cb_mus_0.mus) for check_decode_carbon.sh.
# usage: add_carbon_songs.py <step 2 sdat.viv> <cb_mus_0.mpf> <cb_mus_0.mus> <expected MusicSFx.mpf> <data/carbon_streams_sha256.txt> OUTDIR [--check]
#   --check = every check, nothing written.
import struct, sys, os, re, hashlib
viv, cmpf, cmus, model, shalist, out = sys.argv[1:7]
CHECK = len(sys.argv) > 7 and sys.argv[7] == '--check'
LOG = []
def L(s): print(s); LOG.append(s)
def sha(b): return hashlib.sha256(b).hexdigest()
# --- step 2's archive
vd = open(viv, 'rb').read()
assert sha(vd) == '6f185d8e7459f5af918f23e5c19a4d855d893df76f4102228eb50c6e92014d96', "the input sdat.viv is not step 2's archive (6f185d8e..)"
assert len(vd) == 631243008
assert vd[:4] == b'BIG4'
total, cnt, dirlen = struct.unpack('>III', vd[4:16])
assert cnt == 10 and dirlen == 0x140 and total == len(vd), (cnt, dirlen, total)
o = 16; ents = []
for i in range(cnt):
    off, sz = struct.unpack('>II', vd[o:o + 8]); o += 8; e = vd.index(b'\0', o); ents.append([off, sz, vd[o:e]]); o = e + 1
L("viv %s: %d entries, header 0x%X, total %d = file length, SHA-256 %s" % (viv, cnt, dirlen, total, sha(vd)))
E = {e[2].decode(): e for e in ents}
mpf0 = vd[E['pfdata/MusicSFx.mpf'][0]:E['pfdata/MusicSFx.mpf'][0] + E['pfdata/MusicSFx.mpf'][1]]
mus = vd[E['pfdata/MusicSFx.mus'][0]:E['pfdata/MusicSFx.mus'][0] + E['pfdata/MusicSFx.mus'][1]]
assert len(mpf0) == 7264 and len(mus) == 497631488, (len(mpf0), len(mus))
assert sha(mpf0) == '86a9d10f94026aa0a01f955ace1879b9272088f6adb84769c891bdb2ffa968be' and sha(mus) == '0a50791dff4a061897572efcabe5a49dbe4b40ade72c15ba500343a89eef3b3f'
assert mpf0[:4] == b'xDFP' and mpf0[4] == 4
nn = struct.unpack_from('<H', mpf0, 0x12)[0]; nev = mpf0[0xF]
assert nn == 162 and nev == 79, (nn, nev)
w54, w58, w5c = struct.unpack_from('<III', mpf0, 0x1954)
assert (w54, w58, w5c * 4) == (0x656, 0x658, 7264), (hex(w54), hex(w58), hex(w5c))
L("step 2 MusicSFx.mpf %d B (xDFP v4, %d nodes, %d events; [0x1954] 0x%X [0x1958] 0x%X [0x195C] 0x%X = 0x718), MusicSFx.mus %d B" % (len(mpf0), nn, nev, w54, w58, w5c, len(mus)))
# --- step 2's sample table: 96 entries at 0x1960, each * 0x80 = a SCHl
schl = [m.start() for m in re.finditer(b'SCHl', mus)]
tab = 0x1960
assert struct.unpack_from('<I', mpf0, tab - 8)[0] * 4 == tab and struct.unpack_from('<I', mpf0, tab - 4)[0] * 4 == len(mpf0), "the two words before the table"
nold = (len(mpf0) - tab) // 8
assert nold == 96, nold
oldent = [struct.unpack_from('<II', mpf0, tab + 8 * i) for i in range(nold)]
for i in range(nold): assert mus[oldent[i][0] * 0x80:oldent[i][0] * 0x80 + 4] == b'SCHl', i
assert len(schl) == 96 and [e[0] * 0x80 for e in oldent] == schl, (len(schl))
assert oldent[-1][0] * 0x80 == 0x1D4F1000
L("step 2 sample table at mpf+0x%X: %d entries = the 96 SCHl positions of the .mus (UG2 1..43 + MW 44..69 + UG1 70..96)" % (tab, nold))
# --- Carbon: cb_mus_0.mpf / .mus
M = open(cmpf, 'rb').read(); C = open(cmus, 'rb').read()
assert sha(M) == 'cbd36297722eff76af682f59ca2f0ddb976f4bf3dd58c74cd30dec03e0dccb93' and len(M) == 17168, "cb_mus_0.mpf is not Carbon's own (cbd36297..)"
assert sha(C) == '066ca2cb2732da3531b4a0d5b688ef8a379306f4fa3650d17c3db4aab6ca1ba4' and len(C) == 363904128, "cb_mus_0.mus is not Carbon's own (066ca2cb..)"
assert M[:6] == b'xDFP\x05\x02'
so, end = struct.unpack_from('<II', M, 0x34); assert end == len(M), (hex(so), hex(end))              # the table end = the file end
ctab = [struct.unpack_from('<II', M, so + 8 * i) for i in range(len(M[so:]) // 8)]; assert len(ctab) == 303, len(ctab)
assert all(C[e[0] * 0x80:e[0] * 0x80 + 4] == b'SCHl' for e in ctab)
L("Carbon cb_mus_0.mpf %d B ('xDFP' 5.2, the sample table at 0x%X .. the file end = %d entries, every entry a SCHl), cb_mus_0.mus %d B" % (len(M), so, len(ctab), len(C)))
# --- the known result per stream (data/carbon_streams_sha256.txt: 30 lines
#     'Carbon wave  w -> UG2 wave u: count c (N-byte 0x85), ms ms, size B (Carbon span s, +d), SHA-256 h')
want = {}
for line in open(shalist):
    m = re.match(r'Carbon wave\s+(\d+) -> UG2 wave (\d+): count (\d+) \((\d)-byte 0x85\), (\d+) ms, (\d+) B \(Carbon span (\d+), ([+-]\d+)\), SHA-256 ([0-9a-f]{64})', line.strip())
    assert m, line
    want[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)), int(m.group(7)), int(m.group(8)), m.group(9))
assert sorted(want) == list(range(2, 32)), sorted(want)
def tags(b, p):
    hsz = struct.unpack_from('<I', b, p + 4)[0]; q = p + 12; d = {}; ln85 = 0; order = []
    while q < p + hsz:
        t = b[q]; q += 1
        if t == 0xFF: break
        if t in (0xFC, 0xFD, 0xFE): order.append(t); continue
        n = b[q]; q += 1; d[t] = int.from_bytes(b[q:q + n], 'big'); q += n; order.append(t)
        if t == 0x85: ln85 = n
    return hsz, d, ln85, order
RATE = 36000; cbw = list(range(2, 32))
streams = []; spans = []; counts = []; mss = []
for w in cbw:
    p = ctab[w - 1][0] * 0x80; nxt = ctab[w][0] * 0x80; span = C[p:nxt]
    assert span[:4] == b'SCHl' and C[p + 8:p + 12] == b'PT\0\0'
    hsz, d, ln, order = tags(C, p); cntv = d[0x85]
    assert hsz in (44, 48), (w, hsz)
    assert order == [0x06, 0x13, 0xFD, 0x80, 0x85, 0x82, 0x84, 0x9C, 0x9D], (w, order)                                    # Carbon's one tag set
    assert d[0x06] == 0x65 and d[0x13] == 0x7F and d[0x80] == 2 and d[0x82] == 2 and d[0x84] == RATE and d[0x9C] == 0xF05A and d[0x9D] == 0x105A and 0xA0 not in d, (w, d)
    assert (ln == 3) == (cntv <= 0x7FFFFF) and ln in (3, 4), (w, ln, cntv)                                                 # EA's length rule
    assert ctab[w - 1][1] == cntv * 1000 // RATE, (w, ctab[w - 1][1], cntv)                                                 # Carbon's map ms = floor(count x 1000 / 36000) = UG2's rule
    q = p + hsz; assert C[q:q + 4] == b'SCCl' and struct.unpack_from('<I', C, q + 4)[0] == 12
    nblk = struct.unpack_from('<I', C, q + 8)[0]
    e = q + 12; got = 0; nb = 0; first = True
    while True:
        tg = C[e:e + 4]; bs = struct.unpack_from('<I', C, e + 4)[0]
        if tg == b'SCEl': assert bs == 8; e += 8; break
        assert tg == b'SCDl' and 144 <= bs <= 2876, (w, e, tg, bs)
        ns, o0, o1 = struct.unpack_from('<III', C, e + 8)
        if first: assert (bs, ns, o0, o1) == (2876, 2408, 0, 1428) and C[e + 0x14 + o0] == 0xEE and C[e + 0x14 + o1] == 0xEE, (w, bs, ns, o0, o1); first = False   # both first frames 0xEE
        got += ns; nb += 1; e += bs
    assert nb == nblk and got == cntv, (w, nb, nblk, got, cntv)                                                             # SCCl n = SCDl count; samples sum = tag 0x85
    body = C[q:e]                                                                                                          # 'SCCl' .. 'SCEl' + 8, unchanged
    assert all(x == 0 for x in C[e:nxt]) and 0 <= nxt - e < 0x80, (w, nxt - e)                                             # the span's end padding zero and < 0x80
    tagbytes = bytes([0x06, 0x01, 0x65, 0xFD, 0x80, 0x01, 0x02, 0x85, ln]) + cntv.to_bytes(ln, 'big') + bytes([0x82, 0x01, 0x02, 0x84, 0x02]) + RATE.to_bytes(2, 'big') + b'\xFF'
    hdr = b'PT\x00\x00' + tagbytes
    nhsz = (8 + len(hdr) + 3) & ~3
    assert nhsz == (32 if ln == 3 else 36)
    header = b'SCHl' + struct.pack('<I', nhsz) + hdr + b'\0' * (nhsz - 8 - len(hdr))
    stream = header + body
    stream += b'\0' * ((-len(stream)) % 0x80)
    assert len(stream) <= len(span) and len(span) - len(stream) in (0, 128), (w, len(stream), len(span))
    wu, wc, wln, wms, wsz, wsp, wd, wsha = want[w]
    ms = cntv * 1000 // RATE
    assert (wu, wc, wln, wms, wsz, wsp, wd) == (95 + w, cntv, ln, ms, len(stream), len(span), len(stream) - len(span)), (w, want[w], cntv, ln, ms, len(stream), len(span))
    assert sha(stream) == wsha, (w, sha(stream), wsha)                                                                     # = data/carbon_streams_sha256.txt
    streams.append(stream); spans.append(span); counts.append(cntv); mss.append(ms)
    L("Carbon wave %2d -> UG2 wave %3d: span 0x%08X..0x%08X %d B, count %d (%d-byte 0x85), %d SCDl blocks, %d ms at %d Hz; re-headered %d B (header %d B, %+d vs the span), SHA-256 %s" % (w, 95 + w, p, nxt, len(span), cntv, ln, nb, ms, RATE, len(stream), nhsz, len(stream) - len(span), sha(stream)))
del C
joined = b''.join(streams)
assert len(joined) == 262155392 and sum(len(s) for s in spans) == 262156032, (len(joined), sum(len(s) for s in spans))
assert sorted(set(struct.unpack_from('<I', s, 4)[0] for s in streams)) == [32, 36]
# --- the new .mus = step 2's bytes + the 30
newmus = mus + joined
del joined
assert len(newmus) == 759786880 and newmus[:len(mus)] == mus, len(newmus)
newoff = []; pos = len(mus)
for s in streams: newoff.append(pos); pos += len(s)
assert pos == len(newmus) and newoff[0] == 0x1DA94100 and newoff[29] == 0x2CEB2B00, (hex(newoff[0]), hex(newoff[29]))
for i in range(30): assert newoff[i] % 0x80 == 0 and newmus[newoff[i]:newoff[i] + 4] == b'SCHl'
assert [m.start() for m in re.finditer(b'SCHl', newmus)] == schl + newoff, "the new .mus holds other SCHl than the 126"
# --- the new .mpf = step 2's bytes except [0x195C] + 30 entries appended
newtab = b''.join(struct.pack('<II', newoff[i] // 0x80, mss[i]) for i in range(30))
assert len(newtab) == 240
newmpf = bytearray(mpf0) + newtab
struct.pack_into('<I', newmpf, 0x195C, len(newmpf) // 4)
assert len(newmpf) == 7504 and struct.unpack_from('<I', newmpf, 0x195C)[0] == 0x754
assert bytes(newmpf[:0x195C]) == mpf0[:0x195C] and bytes(newmpf[0x1960:0x1C60]) == mpf0[0x1960:0x1C60] and bytes(newmpf[0x1C60:]) == newtab
diff = [i for i in range(7264) if newmpf[i] != mpf0[i]]
assert all(0x195C <= i < 0x1960 for i in diff), diff
md = open(model, 'rb').read()
assert sha(md) == '34cba3025417d8057959990ed3019a6ffb1a3e2bb4ebea11ef045bb30e7ff409' and bytes(newmpf) == md, "the built map != the expected map (34cba302..)"
# UG2's map loader on the new map (as in add_mw_songs.py): +0x48 0x1954, +0x4C 0x1958, +0x50 0x1960, +0x54 0x1D50 = the file end
def ug2node(i): return struct.unpack_from('<H', newmpf, 0x20 + 2 * i)[0] * 4
nodeTabEnd = 0x20 + nn * 2; lastNode = ug2node(nn - 1); nbrLast = (struct.unpack_from('<I', newmpf, lastNode + 4)[0] >> 12) & 0x1F
p40 = (lastNode + 0x13 + nbrLast * 4) & ~3; p44 = (p40 + 3 + nev * 2) & ~3; p48 = 0x1954
assert (nodeTabEnd, p40, p44, p48) == (0x164, 0xD34, 0xDD4, 0x1954), (hex(nodeTabEnd), hex(p40), hex(p44), hex(p48))
p4c = struct.unpack_from('<I', newmpf, p48 + newmpf[0x10] * 4)[0] * 4; p50 = p4c + 4 + newmpf[0xD] * 4; p54 = struct.unpack_from('<I', newmpf, p4c + newmpf[0xD] * 4)[0] * 4
assert (p4c, p50, p54) == (0x1958, 0x1960, 0x1D50) and p54 == len(newmpf), (hex(p4c), hex(p50), hex(p54))
nent = (p54 - p50) // 8; assert nent == 126
allent = [struct.unpack_from('<II', newmpf, p50 + 8 * i) for i in range(nent)]
for i in range(nent): assert newmus[allent[i][0] * 0x80:allent[i][0] * 0x80 + 4] == b'SCHl', i
assert allent[:96] == oldent
for i in range(30): assert allent[96 + i] == (newoff[i] // 0x80, counts[i] * 1000 // RATE)
assert max(e[0] for e in allent) == 5887574 and 5887574 << 7 == 753609472 < 2 ** 31
# the stream buffer UG2 sizes from the table (as in add_mw_songs.py): the exe's loop leaves r(n-2); the maximum is stored but not read
def rate(k): return (allent[k + 1][0] - allent[k][0]) * 0x80 / allent[k][1]
rN2 = rate(nent - 2); rMax = max(rate(k) for k in range(nent - 1)); kMax = max(range(nent - 1), key=rate)
bN2 = int(4780 * rN2 + 0.5); bMax = int(4780 * rMax + 0.5)
assert (bN2, bMax, kMax, round(rN2, 4), round(rMax, 4)) == (185855, 186353, 49, 38.8819, 38.9859), (bN2, bMax, kMax, rN2, rMax)
L("the new MusicSFx.mpf %d B = the expected map (SHA-256 %s): only [0x195C] (0x718 -> 0x754) + the 240 appended bytes differ from step 2's; loader chain +0x40/+0x44/+0x48/+0x4C/+0x50/+0x54 = 0x%X/0x%X/0x%X/0x%X/0x%X/0x%X; %d entries, every offset*0x80 = a SCHl; max offset/0x80 %d (<<7 = %d < 2^31)"
  % (len(newmpf), sha(bytes(newmpf)), p40, p44, p48, p4c, p50, p54, nent, 5887574, 5887574 << 7))
L("the stream buffer by the exe's loop: r(n-2) = entry %d (wave %d = Carbon wave %d) %.4f B/ms -> BufferOverhead %d B (+ Track Overhead 4,192 = %d B); the max (stored, not read) = entry %d (wave %d) %.4f B/ms -> %d B"
  % (nent - 2, nent - 1, nent - 1 - 95, rN2, bN2, bN2 + 4192, kMax, kMax + 1, rMax, bMax))
# --- the archive
newdata = {'pfdata/MusicSFx.mpf': bytes(newmpf), 'pfdata/MusicSFx.mus': newmus}
dirb = b''; pos = dirlen; layout = []
for off, sz, name in ents:
    n = name.decode(); nsz = len(newdata[n]) if n in newdata else sz
    pos = (pos + 0x3F) & ~0x3F; layout.append((pos, nsz, name)); dirb += struct.pack('>II', pos, nsz) + name + b'\0'; pos += nsz
assert 16 + len(dirb) <= dirlen, (len(dirb), dirlen)
assert pos == 893398656, pos
assert all(p < 2 ** 31 and sz < 2 ** 31 for p, sz, _ in layout)
NEW_LAYOUT = {b'pfdata/MusicSFx.mus': (0x03635A40, 759786880), b'pfdata/MusicSFx.mpf': (0x30ACC7C0, 7504)}
moved = 0
for p, sz, name in layout:
    if name in NEW_LAYOUT: assert (p, sz) == NEW_LAYOUT[name], (name, hex(p), sz)
    else:
        assert sz == E[name.decode()][1], name
        if p != E[name.decode()][0]: moved += 1
assert moved == 4 and [n for p, sz, n in layout if n not in NEW_LAYOUT and p != E[n.decode()][0]] == [b'speech/nisaudio.csi', b'speech/nisaudio.evt', b'speech/nisaudio.idx', b'speech/nisaudio.big'], layout
L("new sdat.viv %d B (step 2's is %d, %+d; the 4 nisaudio entries moved, the other 8 entries' bytes unchanged): " % (pos, len(vd), pos - len(vd)) + "; ".join("%s @0x%X %d B" % (n.decode(), p, sz) for p, sz, n in layout))
if CHECK:
    L("CHECK ONLY: every check passed; nothing written (%s)" % out); sys.exit(0)
# ===== every check passed: the outputs
os.makedirs(os.path.join(out, 'streams'), exist_ok=True)
for i, w in enumerate(cbw):
    open(os.path.join(out, 'streams', 'cb_%02d.bin' % w), 'wb').write(streams[i])
    open(os.path.join(out, 'streams', 'cbo_%02d.bin' % w), 'wb').write(spans[i])
del streams, spans
open(os.path.join(out, 'MusicSFx.mpf'), 'wb').write(bytes(newmpf)); open(os.path.join(out, 'MusicSFx.mus'), 'wb').write(newmus)
with open(os.path.join(out, 'sdat.viv'), 'wb') as f:
    f.write(b'BIG4' + struct.pack('>III', pos, cnt, dirlen) + dirb + b'\0' * (dirlen - 16 - len(dirb)))
    for (p, sz, name), (off0, sz0, _) in zip(layout, [(e[0], e[1], e[2]) for e in ents]):
        f.write(b'\0' * (p - f.tell()))
        n = name.decode(); f.write(newdata[n] if n in newdata else vd[off0:off0 + sz0])
    assert f.tell() == pos
# re-read the written archive entry by entry: the 8 untouched entries byte-identical, the 2 new = the built files
h = hashlib.sha256()
with open(os.path.join(out, 'sdat.viv'), 'rb') as f:
    f.seek(0, 2); assert f.tell() == pos; f.seek(0)
    for chunk in iter(lambda: f.read(1 << 24), b''): h.update(chunk)
    f.seek(4); assert struct.unpack('>I', f.read(4))[0] == pos
    for (p, sz, name), (off0, sz0, _) in zip(layout, ents):
        n = name.decode(); f.seek(p)
        if n in newdata: assert f.read(sz) == newdata[n], n
        else: assert f.read(sz) == vd[off0:off0 + sz0], n
L("OUT %s: sdat.viv %d B SHA-256 %s; MusicSFx.mus %d B %s; MusicSFx.mpf %d B %s (re-read: the 8 untouched entries byte-identical, the 2 new = the built files)"
  % (out, pos, h.hexdigest(), len(newmus), sha(newmus), len(newmpf), sha(bytes(newmpf))))
open(os.path.join(out, 'build_log.txt'), 'w').write('\n'.join(LOG) + '\n')

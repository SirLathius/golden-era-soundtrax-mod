#!/usr/bin/env python3
# add_ug1_songs.py - step 2 of the song archive: adds NFS Underground's 27 music streams to the archive of step 1 (add_mw_songs.py).
# Reads only its inputs. Every check runs before the first output byte is written; the output is the same bytes on every run.
#  1. MusicSFx.mus = step 1's file unchanged (UG2's 43 waves + MW's 26 joined streams at their offsets) + UG1's 27 streams appended in bank
#     order: bank entry s -> wave 70 + s (waves 70..96; stream 0 too, so the archive mirrors UG1's bank). Each UG1 stream gets the same header
#     tag set as the MW streams ('06 01 65 | FD | 80 01 02 | 85 0N <count> | 82 01 02 | 84 02 56 22 | FF'): the rate 22050 is written out (both
#     games' default when the tag is missing: UG2-EXE 727ada, UG1-EXE 655c19); the count takes 3 bytes up to 0x7FFFFF and 4 above (EA's own
#     rule). Then UG1's 'SCCl'..'SCEl' bytes unchanged + zero padding to 0x80 = its UG1 span's size exactly. Dropped: tag 0x13 = 0x7F and tag
#     0x0B = 2 (in neither UG2's own waves nor the MW streams).
#  2. MusicSFx.mpf = step 1's 7,048 B with ONE word changed ([0x195C] 0x6E2 -> 0x718 = the new table end / 4) + 27 entries
#     {offset / 0x80, floor(count * 1000 / 22050)} = 7,264 B, which must equal the expected map (data/MusicSFx_ug2_mw_ug1.mpf) byte for byte.
#     No node is changed.
#  3. sdat.viv = the BIG4 archive rebuilt (the same 10 names in the same order, 0x40-aligned, header 0x140); the other 8 entries byte-identical.
#  4. streams/u1_NN.bin (the 27 re-headered streams = the bytes appended) + streams/ug1_NN.bin (the 27 UG1 spans as they are in the .ast)
#     for check_decode_ug1.sh.
# usage: add_ug1_songs.py <step 1 sdat.viv> <STRM_Music_UG0_MB.ast> <STRM_Music_UG0_MB.abk> <expected MusicSFx.mpf> OUTDIR [--check]
#   --check = every check, nothing written.
import struct, sys, os, re, hashlib
viv, ast, abk, model, out = sys.argv[1:6]
CHECK = len(sys.argv) > 6 and sys.argv[6] == '--check'
LOG = []
def L(s): print(s); LOG.append(s)
def sha(b): return hashlib.sha256(b).hexdigest()
# --- step 1's archive
vd = open(viv, 'rb').read()
assert sha(vd) == '4dc23bd208b5fbb28437bcfc98b23e861109c0e8c717a7b4a9887b18c914e77b', "the input sdat.viv is not step 1's archive (4dc23bd2..)"
assert len(vd) == 491477696
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
assert len(mpf0) == 7048 and len(mus) == 357866368, (len(mpf0), len(mus))
assert sha(mpf0) == '1e98aec576c194b51633381697ae736c344a99757f5a5a36f22463d9887ad37a' and sha(mus) == '414c02b8d5ea71fbbc647a558ee13eb2a37f1518bc13e9f08e03a5d03e11febb'
assert mpf0[:4] == b'xDFP' and mpf0[4] == 4
nn = struct.unpack_from('<H', mpf0, 0x12)[0]; nev = mpf0[0xF]
assert nn == 162 and nev == 79, (nn, nev)
w54, w58, w5c = struct.unpack_from('<III', mpf0, 0x1954)
assert (w54, w58, w5c * 4) == (0x656, 0x658, 7048), (hex(w54), hex(w58), hex(w5c))
L("step 1 MusicSFx.mpf %d B (xDFP v4, %d nodes, %d events; [0x1954] 0x%X [0x1958] 0x%X [0x195C] 0x%X), MusicSFx.mus %d B" % (len(mpf0), nn, nev, w54, w58, w5c, len(mus)))
# --- step 1's sample table: 69 entries at 0x1960, each * 0x80 = a SCHl
schl = [m.start() for m in re.finditer(b'SCHl', mus)]
tab = 0x1960
assert struct.unpack_from('<I', mpf0, tab - 8)[0] * 4 == tab and struct.unpack_from('<I', mpf0, tab - 4)[0] * 4 == len(mpf0), "the two words before the table"
nold = (len(mpf0) - tab) // 8
assert nold == 69, nold
oldent = [struct.unpack_from('<II', mpf0, tab + 8 * i) for i in range(nold)]
for i in range(nold): assert mus[oldent[i][0] * 0x80:oldent[i][0] * 0x80 + 4] == b'SCHl', i
assert len(schl) == 69 and [e[0] * 0x80 for e in oldent] == schl, (len(schl))
assert oldent[-1][0] == 2732754
L("step 1 sample table at mpf+0x%X: %d entries = the 69 SCHl positions of the .mus (UG2 1..43 + MW 44..69)" % (tab, nold))
# --- UG1: the bank + the .ast
A = open(ast, 'rb').read(); K = open(abk, 'rb').read()
assert sha(A) == '12b02f4065c21fffb593a3aee006e24a3c3ff313940e193ed56cbc13c7e7068e' and len(A) == 139765120, "the .ast is not UG1's STRM_Music_UG0_MB.ast (12b02f40..)"
assert sha(K) == '23f9e67c9949ff81c439492053ad272a187fdc3ee2584ddbaf65a254db48846f' and len(K) == 968, "the .abk is not UG1's STRM_Music_UG0_MB.abk (23f9e67c..)"
assert K[:4] == b'ABKC'
nk = struct.unpack_from('<I', K, 0x1E0)[0]; assert nk == 27, nk
kent = [struct.unpack_from('<III', K, 0x1E4 + 12 * i) for i in range(nk)]
assert all(a == 1 and c == 0 for a, _, c in kent) and struct.unpack_from('<III', K, 0x1E4 + 12 * nk) == (0, 0, 0), kent
offs = [b for _, b, _ in kent]
UG1_STREAM_OFFSET = [0x00000000, 0x000AF400, 0x00518A00, 0x008DE500, 0x00B26400, 0x00F21000, 0x015E1900, 0x019C9C80, 0x01E46C00, 0x0238D500, 0x0277D700, 0x02BE4180,
                     0x035B0580, 0x03B0CF00, 0x0400F580, 0x04401E00, 0x04979800, 0x04E83B80, 0x0550E200, 0x05CE4E80, 0x06031C80, 0x06643A00, 0x06A97980, 0x06FF5A80,
                     0x075DB980, 0x07ACB480, 0x07FA7480]
assert offs == UG1_STREAM_OFFSET, offs
assert offs == sorted(offs) and all(o % 0x80 == 0 for o in offs)
assert [m.start() for m in re.finditer(b'SCHl', A)] == offs, "the .ast's SCHl positions != the bank's offsets"
ends = offs[1:] + [len(A)]
L("UG1 bank %s: %d entries {1, offset, 0}; the .ast holds exactly those 27 SCHl; spans end on 0x80 multiples; the spans sum to %d = the file" % (abk, nk, len(A)))
# --- each stream: UG1's one tag set, re-headered; the known result per stream: size, sample count, ms, SHA-256 (first 16 hex digits)
UG1_STREAM_SIZE = [717824, 4625920, 3955456, 2391808, 4172800, 7080192, 4096896, 4706176, 5531904, 4129280, 4614784, 10273792, 5622144, 5252736, 4139136, 5732864, 5284736,
                   6858368, 8219776, 3460608, 6364544, 4538240, 5628160, 6184704, 5176064, 5095424, 5910784]
UG1_COUNT = [660717, 4260282, 3642720, 2202554, 3842896, 6520788, 3773053, 4334187, 5094743, 3802867, 4250019, 9462227, 5177846, 4837597, 3811840, 5279798, 4867032, 6316481,
             7570472, 3186925, 5861577, 4179466, 5183472, 5695968, 4766931, 4692736, 5443623]
UG1_MS = [29964, 193210, 165202, 99889, 174280, 295727, 171113, 196561, 231054, 172465, 192744, 429125, 234822, 219392, 172872, 239446, 220727, 286461, 343332, 144531, 265831,
          189544, 235078, 258320, 216187, 212822, 246876]
UG1_STREAM_SHA = ['c1915891890a2e11', 'f9541a74dd511570', '8e8f16836df1d6dd', '1d40046283c7d351', '240e6248962e7add', '675726a6021651fa', 'fca112c14348e316', 'c4d46a4495ddbf8d',
                  'b5a5f531f1974b91', '104f477d2fce6707', '362ea9c896b0f49d', '95e4b1290cd8f42c', 'c2c356698140ba5f', '9465d04ff4c983ae', '8eee96d946b4fe7c', '5623db3ecf6da87e',
                  '8053461b7a58e31f', 'd2a988f4361dfe14', '13cbc62a82f36ac1', '731c1c219d986c49', '5a5e1629a60ff4e0', 'dc5e0950b5c45f8f', 'e22a70dd5c5303f7', '692d05327d401d91',
                  'c818a22f5664d9fc', '19194ab6149f8dba', 'dc5559d1f9091dc4']
RATE = 22050
streams = []; spans = []; counts = []; mss = []
for s in range(nk):
    span = A[offs[s]:ends[s]]; spans.append(span)
    assert span[:4] == b'SCHl'
    hsz = struct.unpack_from('<I', span, 4)[0]; assert hsz == 0x24 and span[8:12] == b'PT\0\0', (s, hsz, span[8:12])
    q = 12; tags = []; tagd = {}
    while q < hsz:
        t = span[q]; q += 1
        if t == 0xFF: tags.append((0xFF, None, None)); break
        if t in (0xFC, 0xFD, 0xFE): tags.append((t, None, None)); continue
        n = span[q]; q += 1; v = int.from_bytes(span[q:q + n], 'big'); q += n; tags.append((t, n, v)); tagd[t] = v
    assert [t for t, _, _ in tags] == [0x06, 0x13, 0x0B, 0xFD, 0x80, 0x85, 0x82, 0xFF], (s, tags)                      # the one UG1 tag set
    assert tagd[0x06] == 0x65 and tagd[0x13] == 0x7F and tagd[0x0B] == 2 and tagd[0x80] == 2 and tagd[0x82] == 2 and 0x84 not in tagd and 0xA0 not in tagd, (s, tagd)
    count = tagd[0x85]; nlen = [n for t, n, _ in tags if t == 0x85][0]
    assert nlen == (4 if count > 0x7FFFFF else 3), (s, count, nlen)                                                     # EA's length rule
    assert span[hsz:hsz + 4] == b'SCCl' and struct.unpack_from('<I', span, hsz + 4)[0] == 12
    nblk = struct.unpack_from('<I', span, hsz + 8)[0]
    p = hsz + 12; got = 0; nb = 0; first = True
    while True:
        bid = span[p:p + 4]; bs = struct.unpack_from('<I', span, p + 4)[0]
        if bid == b'SCEl': assert bs == 8; p += 8; break
        assert bid == b'SCDl' and 204 <= bs <= 1888, (s, p, bid, bs)
        ns, o0, o1 = struct.unpack_from('<III', span, p + 8)
        if first: assert (bs, ns, o0, o1) == (0x760, 1484, 0, 0x3A6) and span[p + 0x14 + o0] == 0xEE and span[p + 0x14 + o1] == 0xEE, (s, bs, ns, o0, o1); first = False
        got += ns; nb += 1; p += bs
    assert nb == nblk and got == count, (s, nb, nblk, got, count)                                                        # SCCl n = SCDl count; samples sum = tag 0x85
    body = span[hsz:p]                                                                                                 # 'SCCl' .. 'SCEl' + 8, unchanged
    assert span[p:] == b'\0' * (len(span) - p) and 0 <= len(span) - p < 0x80 + 0x24, (s, len(span) - p)             # the span's end padding is zero
    tagbytes = bytes([0x06, 0x01, 0x65, 0xFD, 0x80, 0x01, 0x02, 0x85, nlen]) + count.to_bytes(nlen, 'big') + bytes([0x82, 0x01, 0x02, 0x84, 0x02]) + RATE.to_bytes(2, 'big') + b'\xFF'
    hdr = b'PT\x00\x00' + tagbytes
    nhsz = 8 + len(hdr); nhsz = (nhsz + 3) & ~3
    assert nhsz == (32 if nlen == 3 else 36)
    header = b'SCHl' + struct.pack('<I', nhsz) + hdr + b'\0' * (nhsz - 8 - len(hdr))
    stream = header + body
    stream += b'\0' * ((-len(stream)) % 0x80)
    assert len(stream) == len(span) == UG1_STREAM_SIZE[s], (s, len(stream), len(span), UG1_STREAM_SIZE[s])            # the re-headered stream = its UG1 span's size
    assert count == UG1_COUNT[s] and sha(stream)[:16] == UG1_STREAM_SHA[s], (s, count, sha(stream)[:16])
    ms = count * 1000 // RATE; assert ms == UG1_MS[s], (s, ms, UG1_MS[s])
    streams.append(stream); counts.append(count); mss.append(ms)
    L("UG1 stream %2d -> wave %2d: span 0x%08X..0x%08X %d B, count %d (%d-byte 0x85), %d SCDl blocks, %d ms at %d Hz; re-headered %d B (header %d B), SHA-256 %s" % (s, 70 + s, offs[s], ends[s], len(span), count, nlen, nb, ms, RATE, len(stream), nhsz, sha(stream)))
del A
joined = b''.join(streams)
assert len(joined) == 139765120
assert streams[11][:12] == b'SCHl' + struct.pack('<I', 36) + b'PT\0\0' and streams[0][:12] == b'SCHl' + struct.pack('<I', 32) + b'PT\0\0'
# --- the new .mus = step 1's bytes + the 27
newmus = mus + joined
del joined
assert len(newmus) == 497631488 and newmus[:len(mus)] == mus, len(newmus)
newoff = []; pos = len(mus)
for s in range(nk): newoff.append(pos); pos += len(streams[s])
assert pos == len(newmus) and newoff[0] == 0x15549B80 and newoff[26] == 0x1D4F1000, (hex(newoff[0]), hex(newoff[26]))
for s in range(nk): assert newoff[s] % 0x80 == 0 and newmus[newoff[s]:newoff[s] + 4] == b'SCHl'
assert [m.start() for m in re.finditer(b'SCHl', newmus)] == schl + newoff, "the new .mus holds other SCHl than the 96"
# --- the new .mpf = step 1's bytes except [0x195C] + 27 entries appended
newtab = b''.join(struct.pack('<II', newoff[s] // 0x80, mss[s]) for s in range(nk))
assert len(newtab) == 216
newmpf = bytearray(mpf0) + newtab
struct.pack_into('<I', newmpf, 0x195C, len(newmpf) // 4)
assert len(newmpf) == 7264 and struct.unpack_from('<I', newmpf, 0x195C)[0] == 0x718
assert bytes(newmpf[:0x195C]) == mpf0[:0x195C] and bytes(newmpf[0x1960:0x1B88]) == mpf0[0x1960:0x1B88] and bytes(newmpf[0x1B88:]) == newtab
diff = [i for i in range(7048) if newmpf[i] != mpf0[i]]
assert all(0x195C <= i < 0x1960 for i in diff), diff
md = open(model, 'rb').read()
assert sha(md) == '86a9d10f94026aa0a01f955ace1879b9272088f6adb84769c891bdb2ffa968be' and bytes(newmpf) == md, "the built map != the expected map (86a9d10f..)"
# UG2's map loader on the new map (as in add_mw_songs.py): +0x48 0x1954, +0x4C 0x1958, +0x50 0x1960, +0x54 0x1C60 = the file end
def ug2node(i): return struct.unpack_from('<H', newmpf, 0x20 + 2 * i)[0] * 4
nodeTabEnd = 0x20 + nn * 2; lastNode = ug2node(nn - 1); nbrLast = (struct.unpack_from('<I', newmpf, lastNode + 4)[0] >> 12) & 0x1F
p40 = (lastNode + 0x13 + nbrLast * 4) & ~3; p44 = (p40 + 3 + nev * 2) & ~3; p48 = 0x1954
assert (nodeTabEnd, p40, p44, p48) == (0x164, 0xD34, 0xDD4, 0x1954), (hex(nodeTabEnd), hex(p40), hex(p44), hex(p48))
p4c = struct.unpack_from('<I', newmpf, p48 + newmpf[0x10] * 4)[0] * 4; p50 = p4c + 4 + newmpf[0xD] * 4; p54 = struct.unpack_from('<I', newmpf, p4c + newmpf[0xD] * 4)[0] * 4
assert (p4c, p50, p54) == (0x1958, 0x1960, 0x1C60) and p54 == len(newmpf), (hex(p4c), hex(p50), hex(p54))
nent = (p54 - p50) // 8; assert nent == 96
allent = [struct.unpack_from('<II', newmpf, p50 + 8 * i) for i in range(nent)]
for i in range(nent): assert newmus[allent[i][0] * 0x80:allent[i][0] * 0x80 + 4] == b'SCHl', i
assert allent[:69] == oldent
for s in range(nk): assert allent[69 + s] == (newoff[s] // 0x80, counts[s] * 1000 // RATE)
assert max(e[0] for e in allent) == 3841568 and 3841568 << 7 == 491720704 < 2 ** 31
# the stream buffer UG2 sizes from the table (as in add_mw_songs.py): the exe's loop leaves r(n-2); the maximum is stored but not read
def rate(k): return (allent[k + 1][0] - allent[k][0]) * 0x80 / allent[k][1]
rN2 = rate(nent - 2); rMax = max(rate(k) for k in range(nent - 1)); kMax = max(range(nent - 1), key=rate)
bN2 = int(4780 * rN2 + 0.5); bMax = int(4780 * rMax + 0.5)
assert (bN2, bMax, kMax, round(rN2, 4), round(rMax, 4)) == (114444, 186353, 49, 23.9422, 38.9859), (bN2, bMax, kMax, rN2, rMax)
L("the new MusicSFx.mpf %d B = the expected map (SHA-256 %s): only [0x195C] (0x6E2 -> 0x718) + the 216 appended bytes differ from step 1's; loader chain +0x40/+0x44/+0x48/+0x4C/+0x50/+0x54 = 0x%X/0x%X/0x%X/0x%X/0x%X/0x%X; %d entries, every offset*0x80 = a SCHl; max offset/0x80 %d (<<7 = %d < 2^31)"
  % (len(newmpf), sha(bytes(newmpf)), p40, p44, p48, p4c, p50, p54, nent, 3841568, 3841568 << 7))
L("the stream buffer by the exe's loop: r(n-2) = entry %d (wave %d = UG1 stream %d) %.4f B/ms -> BufferOverhead %d B (+ Track Overhead 4,192 = %d B); the max (stored, not read) = entry %d (wave %d) %.4f B/ms -> %d B"
  % (nent - 2, nent - 1, nent - 1 - 70, rN2, bN2, bN2 + 4192, kMax, kMax + 1, rMax, bMax))
# --- the archive
newdata = {'pfdata/MusicSFx.mpf': bytes(newmpf), 'pfdata/MusicSFx.mus': newmus}
dirb = b''; pos = dirlen; layout = []
for off, sz, name in ents:
    n = name.decode(); nsz = len(newdata[n]) if n in newdata else sz
    pos = (pos + 0x3F) & ~0x3F; layout.append((pos, nsz, name)); dirb += struct.pack('>II', pos, nsz) + name + b'\0'; pos += nsz
assert 16 + len(dirb) <= dirlen, (len(dirb), dirlen)
assert pos == 631243008, pos
assert all(p < 2 ** 31 and sz < 2 ** 31 for p, sz, _ in layout)
NEW_LAYOUT = {b'pfdata/MusicSFx.mus': (0x03635A40, 497631488), b'pfdata/MusicSFx.mpf': (0x210C9B40, 7264)}
moved = 0
for p, sz, name in layout:
    if name in NEW_LAYOUT: assert (p, sz) == NEW_LAYOUT[name], (name, hex(p), sz)
    else:
        assert sz == E[name.decode()][1], name
        if p != E[name.decode()][0]: moved += 1
assert moved == 4 and [n for p, sz, n in layout if n not in NEW_LAYOUT and p != E[n.decode()][0]] == [b'speech/nisaudio.csi', b'speech/nisaudio.evt', b'speech/nisaudio.idx', b'speech/nisaudio.big'], layout
L("new sdat.viv %d B (step 1's is %d; the 4 nisaudio entries moved, the other 8 entries' bytes unchanged): " % (pos, len(vd)) + "; ".join("%s @0x%X %d B" % (n.decode(), p, sz) for p, sz, n in layout))
if CHECK:
    L("CHECK ONLY: every check passed; nothing written (%s)" % out); sys.exit(0)
# ===== every check passed: the outputs
os.makedirs(os.path.join(out, 'streams'), exist_ok=True)
for s in range(nk):
    open(os.path.join(out, 'streams', 'u1_%02d.bin' % s), 'wb').write(streams[s])
    open(os.path.join(out, 'streams', 'ug1_%02d.bin' % s), 'wb').write(spans[s])
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

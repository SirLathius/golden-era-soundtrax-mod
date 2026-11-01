#!/usr/bin/env python3
# add_mw_songs.py - step 1 of the song archive: adds Most Wanted's 26 EA Trax songs to UG2's original SDATA/sdat.viv.
# Reads only its inputs. Every check runs before the first output byte is written; the output is the same bytes on every run.
#  1. MusicSFx.mus = UG2's own file unchanged (waves 1..43 at their offsets) + MW songs 1..26 joined in order as waves 44..69. One MW song is
#     many short bars in MW_Music.mus; the join follows the song's event from MW_Music.mpf to its first send-event node, walks the node chain,
#     and puts every bar's SCDl blocks (their 3 fields turned little-endian) behind one new SCHl header in UG2's tag set
#     ('06 01 65 | FD | 80 01 02 | 85 03 <count> | 82 01 02 | 84 02 8C A0 | FF' = 2 ch, 36000 Hz), then pads the stream to 0x80.
#  2. MusicSFx.mpf = UG2's 6,840 bytes with ONE word changed ([0x195C] 0x6AE -> 0x6E2 = the table end / 4) + 26 entries
#     {offset / 0x80, MW map ms} appended = 7,048 B. No node is changed: the mod writes its two carrier nodes (songs 25 / 26) in memory.
#  3. sdat.viv = the BIG4 archive rebuilt: the same 10 names in the same order, 0x40-aligned, header 0x140; the other 8 entries byte-identical.
#  4. streams/mw_NN.bin (the 26 joined streams) + streams/bars_NN.txt (each bar's offset / size / samples / ms in MW_Music.mus) for
#     check_decode_mw.sh; MusicSFx_original.mpf = UG2's own map as read from the archive (an input of make_map_header.py).
# usage: add_mw_songs.py <UG2's original sdat.viv> <MW_Music.mpf> <MW_Music.mus> OUTDIR [--check]
#   --check = every check, nothing written.
import struct, sys, os, re, hashlib
viv, mwmpf, mwmus, out = sys.argv[1:5]
CHECK = len(sys.argv) > 5 and sys.argv[5] == '--check'
LOG = []
def L(s): print(s); LOG.append(s)
def sha(b): return hashlib.sha256(b).hexdigest()
# --- UG2's original archive
vd = open(viv, 'rb').read()
assert sha(vd) == '379a3cd7c16a5090590248616042f8af3ac0c234a58dc9f58c7eee200377e3f1', "the input sdat.viv is not UG2's original archive (379a3cd7..)"
assert len(vd) == 259484480
assert vd[:4] == b'BIG4'
total, cnt, dirlen = struct.unpack('>III', vd[4:16])
assert cnt == 10 and dirlen == 0x140 and total == len(vd), (cnt, dirlen, total)
o = 16; ents = []
for i in range(cnt):
    off, sz = struct.unpack('>II', vd[o:o + 8]); o += 8; e = vd.index(b'\0', o); ents.append([off, sz, vd[o:e]]); o = e + 1
L("viv %s: %d entries, header 0x%X, total %d = file length, SHA-256 %s" % (viv, cnt, dirlen, total, sha(vd)))
E = {e[2].decode(): e for e in ents}
mpf = bytearray(vd[E['pfdata/MusicSFx.mpf'][0]:E['pfdata/MusicSFx.mpf'][0] + E['pfdata/MusicSFx.mpf'][1]])
mus = vd[E['pfdata/MusicSFx.mus'][0]:E['pfdata/MusicSFx.mus'][0] + E['pfdata/MusicSFx.mus'][1]]
mpf0 = bytes(mpf)
assert len(mpf) == 6840 and len(mus) == 125873408, (len(mpf), len(mus))
assert sha(mpf0) == '010c929e17524d5cfcd54377d14d75de06028a318abf427bb19c122bcd3b0cf5', "UG2's MusicSFx.mpf is not the original (010c929e..)"
assert mpf[:4] == b'xDFP' and mpf[4] == 4
nn = struct.unpack_from('<H', mpf, 0x12)[0]; nev = mpf[0xF]
assert nn == 162 and nev == 79, (nn, nev)
w54, w58, w5c = struct.unpack_from('<III', mpf, 0x1954)
assert (w54, w58, w5c * 4) == (0x656, 0x658, 6840), (hex(w54), hex(w58), hex(w5c))
L("UG2 MusicSFx.mpf %d B (xDFP v4, %d nodes, %d events; [0x1954] 0x%X [0x1958] 0x%X [0x195C] 0x%X), MusicSFx.mus %d B" % (len(mpf), nn, nev, w54, w58, w5c, len(mus)))
# --- the sample table, found by the positions of the first two SCHl headers in the .mus
schl = [m.start() for m in re.finditer(b'SCHl', mus)]
tab = None
for q in range(0x48, len(mpf) - 8, 4):
    if struct.unpack_from('<I', mpf, q)[0] * 0x80 == schl[0] and struct.unpack_from('<I', mpf, q + 8)[0] * 0x80 == schl[1]: tab = q; break
assert tab == 0x1960, tab
nsmp = (len(mpf) - tab) // 8
assert nsmp == len(schl) == 43, (nsmp, len(schl))
assert struct.unpack_from('<I', mpf, tab - 8)[0] * 4 == tab and struct.unpack_from('<I', mpf, tab - 4)[0] * 4 == len(mpf), "the two words before the table"
entries = [list(struct.unpack_from('<II', mpf, tab + 8 * i)) for i in range(nsmp)]
for i in range(nsmp): assert entries[i][0] * 0x80 == schl[i]
L("UG2 sample table at mpf+0x%X, %d entries = the 43 SCHl positions, bounded by the words at 0x%X / 0x%X" % (tab, nsmp, tab - 8, tab - 4))
# --- the 27 song waves: song s enters at node 3s, whose one branch (target at +18) is the audio node; its word 0 low 16 bits = the wave
def ug2node(i): return struct.unpack_from('<H', mpf, 0x20 + 2 * i)[0] * 4
UG2_SONG_WAVES = [39, 4, 35, 8, 40, 23, 2, 19, 20, 38, 33, 34, 30, 9, 5, 6, 3, 31, 37, 36, 7, 10, 11, 18, 22, 21, 32]
waves = []
for s in range(27):
    e = ug2node(3 * s); w0e = struct.unpack_from('<I', mpf, e)[0]; nbr = (struct.unpack_from('<I', mpf, e + 4)[0] >> 12) & 0x1F
    assert (w0e & 0xFFFF) == 0 and nbr == 1, (s, hex(w0e), nbr)
    aud = struct.unpack_from('<H', mpf, e + 18)[0]; ao = ug2node(aud); waves.append(struct.unpack_from('<I', mpf, ao)[0] & 0xFFFF)
assert waves == UG2_SONG_WAVES, (waves, UG2_SONG_WAVES)
L("the 27 song waves: " + ' '.join('%d:%d' % (s, w) for s, w in enumerate(waves)))
# --- MW: the map + the 26 songs' bars
md = open(mwmpf, 'rb').read(); mm = open(mwmus, 'rb')
assert len(md) == 136148 and sha(md) == '15c7fdaa626940319a74965d4d68db1507475ba743d574c5aa6de214482e785b', "MW_Music.mpf is not MW's own (15c7fdaa..)"
hm = hashlib.sha256()
for chunk in iter(lambda: mm.read(1 << 24), b''): hm.update(chunk)
assert mm.tell() == 533877888 and hm.hexdigest() == 'bb191d34c4c3ac3b5bd33a9e02049a08df13014e4fb5c8d7d07320abae1c5811', "MW_Music.mus is not MW's own (bb191d34..)"
L("MW_Music.mpf %d B SHA-256 %s; MW_Music.mus %d B SHA-256 %s" % (len(md), sha(md), mm.tell(), hm.hexdigest()))
def U32(o): return struct.unpack_from('<I', md, o)[0]
def U16(o): return struct.unpack_from('<H', md, o)[0]
def SX(v, n): return v - (1 << n) if v & (1 << (n - 1)) else v
nodeOffs, evOffs, samples, mapLen = U32(0x14), U32(0x1C), U32(0x34), U32(0x38); numNodes = U16(0x12); nS = (mapLen - samples) // 8
assert md[0x10] == 123, "MW's map must hold 123 routers"
def node(i):
    o = U16(nodeOffs + 2 * i) * 4; w0, w1 = U32(o), U32(o + 4); nbr = (w1 >> 12) & 0x1F; rt = (int)(w1 & 0xFFF) - 1
    br = [struct.unpack_from('<bbH', md, o + 16 + 4 * j) for j in range(nbr)]
    return SX(w0 & 0xFFFF, 16), br, rt
def mwsample(w): off = U32(samples + 8 * (w - 1)) * 0x80; nxt = U32(samples + 8 * w) * 0x80 if w < nS else None; return off, (nxt - off), U32(samples + 8 * (w - 1) + 4)
# the known result per song: the joined stream's size, its MW map ms (the sum of its bars') and its SHA-256
MW_STREAM_SIZE = {1: 7722368, 2: 6791296, 3: 10556800, 4: 7685376, 5: 7978240, 6: 9057536, 7: 7984000, 8: 7891840, 9: 13793152, 10: 6766464, 11: 7902720, 12: 7702784, 13: 9268864,
                  14: 7641216, 15: 8846080, 16: 8448768, 17: 9522304, 18: 8591488, 19: 6983040, 20: 7229824, 21: 9092480, 22: 15061376, 23: 13954688, 24: 9139712, 25: 8306688, 26: 8073856}
MW_SONG_MS = {1: 198958, 2: 175186, 3: 271848, 4: 197933, 5: 205510, 6: 233031, 7: 204792, 8: 203499, 9: 355255, 10: 174410, 11: 203899, 12: 198412, 13: 239067, 14: 197051,
              15: 228143, 16: 217526, 17: 245335, 18: 221616, 19: 180078, 20: 186357, 21: 234428, 22: 388544, 23: 359482, 24: 235421, 25: 214196, 26: 208161}
MW_STREAM_SHA = {
    1: '47c93fb200c1f7822e8eab1485a7c5ea696210ff356008336eb3bf527d17a1b3', 2: 'd4a58620716e08b461a41292dce48ecb110292cc78ba12ee8ab8f570f3411cfc',
    3: 'ccf6bbbc675aeb2b5758638f39995e9d87c661c1fe3eb4b24efef42a0bbf1a5b', 4: 'bbbff8ea7765921e9c049b389f435a53e307f81abe30800d1d23b3f85ffc17b4',
    5: '32321680ac17c14b1efb55916c59ba924f621fbc1fc9a24ffe6559672a5ae1b3', 6: '25062e6c5b8bae9946a26b74f6a2546d1278d6cfc8c3c646cd2d251b5456b7c7',
    7: '5b9bb2906340bb5a9acfc94967a1455617be0d20cb96f2d32a4f12ce2f358a52', 8: 'c6f060e3abb0368b42b4601ac721e216625f84a043cf42331aa5c148fbb4a482',
    9: 'efbef24e1c0ac254a5dfbd15b28531f72cc067d77fef8f2bf9279b15199b30c0', 10: 'fa5b9be94726255e81a7b44dda30dc172eb34681f316218409fcf84c7b90c258',
    11: '64185bcc3897399852e142f9161fbd6f65a278d356dbeff7186672ea8daeadb4', 12: '9ac17645bba1d5a570a91c83837542a32fee8f416adc4c1639aba81c9343c1dc',
    13: 'dbacce23594a10ffd78f2b8b8e466d441c24a7ac5d4295c9a97a9a0d13d97a22', 14: '2e2feab3eb4481ab1781008d149ae01c7c849866b04e20bfb06babe127f36a96',
    15: 'ac0a078f56d11f4cde52c1c32be330b5e85f827ef26718b0c5ceb6047f9ad1a9', 16: 'cc5c4574d8f7d1eb320b6c2a17d64577bae4ccaa0caf81d367de81ac7996317e',
    17: '20c488384ea1e67b80c024d167bd5170fcfe166d320227577cf527d6f18f420d', 18: '1a02b25d785404c4f5beb69c378a6cff9e4b1280243d5cb246349555c1dde179',
    19: 'f71ced40613b4982e84543d19c637552323a5ad23ac524af463fcaa317cc0626', 20: '3a21dc8fc81b967f57215da3431ab98a2d3c17ba9dbbfb735fa7f47b06ac6169',
    21: '67047a743c4c6e5d1ba13b703304e79d6deb925d105b9b92fa59e52bf77200a7', 22: '2544b4982d6547b693c847bd333aeacb3487d94a3a5ee0c45257e9a7b9e1ad07',
    23: '1f04c1d128ae2e6610a6954e6c84c25f786cae05e63865d0894e8af52430a5a8', 24: 'e70661dafd3cd7d25872b2007ac9a13c2adaf60c1f9e7df90386632422ff3d3e',
    25: '5f76a718e4c171017bde48adf12c5791787c725cd352560ab108805bd2b681b3', 26: '6e9896c041401819bd6fa207d8c2e0f12a92276ce6a7f947bbe36d3c83f90c2e'}
streams = {}; barlists = {}; songms = {}
for song in range(1, 27):
    eo = U16(evOffs + 2 * song) * 4; na = U32(eo + 12) >> 24; entry = None
    for pc in range(na):
        p = eo + 0x14 + 12 * pc; f, a = U32(p + 4), U32(p + 8)
        if (f >> 8) & 0x7F == 4 and (f >> 16) & 3 == 3 and SX(a & 0xFFFF, 16) >= 0: entry = SX(a & 0xFFFF, 16); break
    assert entry is not None, song
    bars = []; cur = entry; seen = set()
    while cur not in seen:
        seen.add(cur); w, br, rt = node(cur)
        assert rt < 0, "a router on the song's path"
        if w == -3: break
        if w > 0: bars.append(w)
        nxt = [d for (mn, mx, d) in br if mn <= 0 <= mx]
        if not nxt: break
        cur = nxt[0]
    blocks = []; total = 0; ms = 0; barlist = []
    for w in bars:
        off, size, wms = mwsample(w); mm.seek(off); b = mm.read(size); assert b[:4] == b'SCHl'
        hsz = struct.unpack_from('<I', b, 4)[0]; assert b[8:12] == b'GSTR'
        q = 16; tags = {}
        while q < hsz:
            t = b[q]; q += 1
            if t == 0xFF: break
            if t in (0xFC, 0xFD, 0xFE): continue
            n = b[q]; q += 1; v = 0
            for _ in range(n): v = (v << 8) | b[q]; q += 1
            tags[t] = v
        assert tags[0x82] == 2 and tags[0x84] == 36000 and 0xA0 not in tags and tags[0x80] == 3, tags
        p = hsz; got = 0
        while p + 8 <= len(b):
            bid = b[p:p + 4]; bs = struct.unpack_from('<I', b, p + 4)[0]
            if bs < 8: break
            if bid == b'SCDl':
                ns = struct.unpack_from('>I', b, p + 8)[0]; offs = struct.unpack_from('>2I', b, p + 0x0C)
                data = b[p + 0x14:p + bs]
                if got == 0:
                    for c in range(2): assert b[p + 0x14 + offs[c]] == 0xEE, "bar %d does not start with a PCM frame" % w
                blocks.append(struct.pack('<4sIII', b'SCDl', bs, ns, offs[0]) + struct.pack('<I', offs[1]) + data)
                got += ns
            elif bid == b'SCEl': break
            p += bs
        assert got == tags[0x85], (w, got, tags[0x85])
        total += got; ms += wms; barlist.append((w, off, size, got, wms))
    rate = 36000
    tagbytes = bytes([0x06, 0x01, 0x65, 0xFD, 0x80, 0x01, 0x02, 0x85, 0x03]) + total.to_bytes(3, 'big') + bytes([0x82, 0x01, 0x02, 0x84, 0x02]) + rate.to_bytes(2, 'big') + b'\xFF'
    hdr = b'PT\x00\x00' + tagbytes
    hsz = 8 + len(hdr); hsz = (hsz + 3) & ~3
    header = b'SCHl' + struct.pack('<I', hsz) + hdr + b'\0' * (hsz - 8 - len(hdr))
    stream = header + b'SCCl' + struct.pack('<II', 12, len(blocks)) + b''.join(blocks) + b'SCEl' + struct.pack('<I', 8)
    stream += b'\0' * ((-len(stream)) % 0x80)
    assert len(stream) == MW_STREAM_SIZE[song], (song, len(stream), MW_STREAM_SIZE[song])
    assert ms == MW_SONG_MS[song], (song, ms, MW_SONG_MS[song])
    assert sha(stream) == MW_STREAM_SHA[song], (song, sha(stream), MW_STREAM_SHA[song])
    streams[song] = stream; barlists[song] = barlist; songms[song] = ms
    L("MW song %2d: entry node %d, %d bars (waves %d..%d), %d SCDl blocks, %d samples = %.3f s, MW map ms %d, joined %d B, SHA-256 %s" % (song, entry, len(bars), bars[0], bars[-1], len(blocks), total, total / rate, ms, len(stream), sha(stream)))
mm.close()
joined = b''.join(streams[s] for s in range(1, 27))
assert len(joined) == 231992960, len(joined)
# --- the new .mus = the old bytes + the 26 streams
newmus = mus + joined
del joined
assert len(newmus) == 357866368 and newmus[:len(mus)] == mus, len(newmus)
newoff = {}; pos = len(mus)
for s in range(1, 27): newoff[s] = pos; pos += len(streams[s])
assert pos == len(newmus)
MW_STREAM_OFFSET = {1: 125873408, 2: 133595776, 3: 140387072, 4: 150943872, 5: 158629248, 6: 166607488, 7: 175665024, 8: 183649024, 9: 191540864, 10: 205334016, 11: 212100480,
                    12: 220003200, 13: 227705984, 14: 236974848, 15: 244616064, 16: 253462144, 17: 261910912, 18: 271433216, 19: 280024704, 20: 287007744, 21: 294237568,
                    22: 303330048, 23: 318391424, 24: 332346112, 25: 341485824, 26: 349792512}
assert newoff == MW_STREAM_OFFSET, newoff
for s in range(1, 27): assert newoff[s] % 0x80 == 0 and newmus[newoff[s]:newoff[s] + 4] == b'SCHl'
# --- the new .mpf = the old bytes except [0x195C] + 26 entries appended
newtab = b''.join(struct.pack('<II', newoff[s] // 0x80, songms[s]) for s in range(1, 27))
assert len(newtab) == 208
newmpf = bytearray(mpf0) + newtab
struct.pack_into('<I', newmpf, 0x195C, len(newmpf) // 4)
assert len(newmpf) == 7048 and struct.unpack_from('<I', newmpf, 0x195C)[0] == 0x6E2
assert bytes(newmpf[:0x195C]) == mpf0[:0x195C] and bytes(newmpf[0x1960:0x1AB8]) == mpf0[0x1960:0x1AB8] and bytes(newmpf[0x1AB8:]) == newtab
diff = [i for i in range(6840) if newmpf[i] != mpf0[i]]
assert all(0x195C <= i < 0x1960 for i in diff), diff
assert sha(bytes(newmpf)) == '1e98aec576c194b51633381697ae736c344a99757f5a5a36f22463d9887ad37a', "the built map is not the known UG2 + MW map (1e98aec5..)"
# UG2's map loader on the new map: +0x38 = base+0x20, +0x3C = +0x38 + nodes*2, +0x40 = (last node + 0x13 + nbr*4) & ~3 (from a decompile, not
# checked in the exe); +0x48 = base+0x1954 (after the last event; bytes 0..0x195B are the original's, checked above); UG2-EXE: +0x4C = base +
# [+0x48 + byte[0x10]*4]*4, +0x50 = +0x4C + 4 + byte[0xD]*4, +0x54 = base + [+0x4C + byte[0xD]*4]*4 = the table end
nodeTabEnd = 0x20 + nn * 2; lastNode = ug2node(nn - 1); nbrLast = (struct.unpack_from('<I', newmpf, lastNode + 4)[0] >> 12) & 0x1F
p40 = (lastNode + 0x13 + nbrLast * 4) & ~3; p44 = (p40 + 3 + nev * 2) & ~3; p48 = 0x1954
assert (nodeTabEnd, p40, p44, p48) == (0x164, 0xD34, 0xDD4, 0x1954), (hex(nodeTabEnd), hex(p40), hex(p44), hex(p48))
p4c = struct.unpack_from('<I', newmpf, p48 + newmpf[0x10] * 4)[0] * 4; p50 = p4c + 4 + newmpf[0xD] * 4; p54 = struct.unpack_from('<I', newmpf, p4c + newmpf[0xD] * 4)[0] * 4
assert (p4c, p50, p54) == (0x1958, 0x1960, 0x1B88) and p54 == len(newmpf), (hex(p4c), hex(p50), hex(p54))
nent = (p54 - p50) // 8; assert nent == 69
allent = [struct.unpack_from('<II', newmpf, p50 + 8 * i) for i in range(nent)]
for i in range(nent): assert newmus[allent[i][0] * 0x80:allent[i][0] * 0x80 + 4] == b'SCHl', i
assert allent[:43] == [tuple(e) for e in entries]
assert max(e[0] for e in allent) == 2732754 and 2732754 << 7 < 2 ** 31
# UG2 sizes its stream buffer from the table: r(k) = (off[k+1] - off[k]) * 0x80 / ms[k] over entries 0..n-2; the exe's loop leaves r(n-2)
# (the maximum is stored but not read); BufferOverhead = 4780 * r
def rate(k): return (allent[k + 1][0] - allent[k][0]) * 0x80 / allent[k][1]
rN2 = rate(nent - 2); rMax = max(rate(k) for k in range(nent - 1)); kMax = max(range(nent - 1), key=rate)
bN2 = int(4780 * rN2 + 0.5); bMax = int(4780 * rMax + 0.5)
assert (bN2, bMax, kMax + 1) == (185372, 186353, 50), (bN2, bMax, kMax + 1)
L("the new MusicSFx.mpf %d B: only [0x195C] (0x6AE -> 0x6E2) + the 208 appended bytes differ; loader chain +0x40/+0x44/+0x48/+0x4C/+0x50/+0x54 = 0x%X/0x%X/0x%X/0x%X/0x%X/0x%X; %d entries, every offset*0x80 = a SCHl; max offset/0x80 %d (<<7 = %d < 2^31)"
  % (len(newmpf), p40, p44, p48, p4c, p50, p54, nent, 2732754, 2732754 << 7))
L("the stream buffer by the exe's loop: r(n-2) = entry %d (wave %d) %.4f B/ms -> BufferOverhead %d B; the max = entry %d (wave %d) %.4f B/ms -> %d B"
  % (nent - 2, nent - 1, rN2, bN2, kMax, kMax + 1, rMax, bMax))
# --- the archive
newdata = {'pfdata/MusicSFx.mpf': bytes(newmpf), 'pfdata/MusicSFx.mus': newmus}
dirb = b''; pos = dirlen; layout = []
for off, sz, name in ents:
    n = name.decode(); nsz = len(newdata[n]) if n in newdata else sz
    pos = (pos + 0x3F) & ~0x3F; layout.append((pos, nsz, name)); dirb += struct.pack('>II', pos, nsz) + name + b'\0'; pos += nsz
assert 16 + len(dirb) <= dirlen, (len(dirb), dirlen)
assert pos == 491477696, pos
assert all(p < 2 ** 31 and sz < 2 ** 31 for p, sz, _ in layout)
NEW_LAYOUT = {b'pfdata/MusicSFx.mus': (0x03635A40, 357866368), b'pfdata/MusicSFx.mpf': (0x18B7F5C0, 7048), b'speech/nisaudio.csi': (0x18B81180, 152),
              b'speech/nisaudio.evt': (0x18B81240, 440), b'speech/nisaudio.idx': (0x18B81400, 960), b'speech/nisaudio.big': (0x18B817C0, 76759808)}
for p, sz, name in layout:
    if name in NEW_LAYOUT: assert (p, sz) == NEW_LAYOUT[name], (name, hex(p), sz)
    else: assert (p, sz) == tuple(E[name.decode()][:2]), (name, hex(p), sz)
L("new sdat.viv %d B (UG2's is %d): " % (pos, len(vd)) + "; ".join("%s @0x%X %d B" % (n.decode(), p, sz) for p, sz, n in layout))
if CHECK:
    L("CHECK ONLY: every check passed; nothing written (%s)" % out); sys.exit(0)
# ===== every check passed: the outputs
os.makedirs(os.path.join(out, 'streams'), exist_ok=True)
for s in range(1, 27):
    open(os.path.join(out, 'streams', 'mw_%02d.bin' % s), 'wb').write(streams[s])
    with open(os.path.join(out, 'streams', 'bars_%02d.txt' % s), 'w') as f:
        for (w, off, size, got, wms) in barlists[s]: f.write('%d %d %d %d %d\n' % (w, off, size, got, wms))
del streams
open(os.path.join(out, 'MusicSFx_original.mpf'), 'wb').write(mpf0)
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

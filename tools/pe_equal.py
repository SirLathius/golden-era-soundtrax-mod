#!/usr/bin/env python3
# pe_equal.py A B: are two PE files equal byte for byte once the header's time stamp and checksum are set to zero in both?
import sys, struct
def norm(p):
    b = bytearray(open(p, 'rb').read())
    pe = struct.unpack_from('<I', b, 0x3C)[0]
    assert b[pe:pe+4] == b'PE\0\0', p
    struct.pack_into('<I', b, pe + 8, 0)          # COFF TimeDateStamp
    struct.pack_into('<I', b, pe + 24 + 64, 0)    # optional header CheckSum
    return bytes(b)
a, b = norm(sys.argv[1]), norm(sys.argv[2])
if a == b: print('EQUAL (%d B, time stamp + checksum zeroed)' % len(a)); sys.exit(0)
d = next(i for i in range(min(len(a), len(b))) if a[i] != b[i]) if len(a) == len(b) or min(len(a), len(b)) else 0
print('DIFFERENT: sizes %d / %d, first difference at 0x%X' % (len(a), len(b), d)); sys.exit(1)

#!/usr/bin/env python3
# pe_sections.py A B: compares the loaded sections of two PE files - for each section its name, virtual address, virtual size and raw
# bytes must be equal. Prints one verdict per section and 'SECTIONS EQUAL' or 'SECTIONS DIFFER'; the exit code is 0 only when every
# loaded section is equal. The headers' symbol-table fields, the time stamp, the checksum, the file tail (a COFF symbol table, if any)
# and the .debug* sections (named through the string table; never loaded) are not compared, so a build linked with -s and the same
# build without it compare EQUAL. Link both under the same output name: mingw's linker takes the image base from it. Reads only.
import sys, struct
def sections(p):
    b = open(p, 'rb').read(); pe = struct.unpack_from('<I', b, 0x3C)[0]; assert b[pe:pe + 4] == b'PE\0\0', p
    nsec, optSize = struct.unpack_from('<H', b, pe + 6)[0], struct.unpack_from('<H', b, pe + 20)[0]
    symPtr, nSym = struct.unpack_from('<II', b, pe + 12); strTab = symPtr + 18 * nSym if symPtr else 0   # the string table follows the symbol table
    base = pe + 24 + optSize; out = []
    for i in range(nsec):
        o = base + 40 * i; name = b[o:o + 8].rstrip(b'\0').decode('latin1')
        if name.startswith('/') and strTab: k = strTab + int(name[1:]); name = b[k:b.index(b'\0', k)].decode('latin1')   # a long name: '/offset' into the string table
        vsize, vaddr, rsize, raw = struct.unpack_from('<IIII', b, o + 8)
        if name.startswith('.debug'): print('%-16s not loaded (debug information): skipped' % name); continue
        out.append((name, vaddr, vsize, b[raw:raw + rsize] if rsize else b''))
    return out
a, b = sections(sys.argv[1]), sections(sys.argv[2]); ok = len(a) == len(b)
if len(a) != len(b): print('section counts differ: %d / %d' % (len(a), len(b)))
for i in range(max(len(a), len(b))):
    if i >= len(a) or i >= len(b): print('only in one file: %s' % ((a + b)[i][0])); continue
    na, va, sa, ra = a[i]; nb, vb, sb, rb = b[i]
    same = na[:8] == nb[:8] and va == vb and sa == sb and ra == rb; ok &= same   # a name over 8 characters shows whole only with a string table
    d = '' if ra == rb else ' (raw bytes differ at 0x%X)' % next((k for k in range(min(len(ra), len(rb))) if ra[k] != rb[k]), min(len(ra), len(rb)))
    print('%-8s VA 0x%05X size 0x%05X raw %6d B  %s%s' % (na, va, sa, len(ra), 'EQUAL' if same else 'DIFFERENT: %s VA 0x%05X size 0x%05X raw %d B' % (nb, vb, sb, len(rb)), d))
print('SECTIONS EQUAL (%d sections)' % len(a) if ok else 'SECTIONS DIFFER'); sys.exit(0 if ok else 1)

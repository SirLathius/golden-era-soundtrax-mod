// streamdec.c - decodes ONE EA SCHl stream in UG2's PC shape ('PT' header, little-endian block fields, EA-XA v2 with 0xEE PCM frames)
// to raw s16le stereo; prints the header tags, the block count and the samples. The decoder core DecodeCh follows vgmstream's decode_ea_xa_v2.
// usage: streamdec <stream.bin> <out.raw>   (exit 0 = every sample of tag 0x85 decoded, 3 = fewer)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static const int kXa[8] = { 0, 240, 460, 392, 0, 0, -208, -220 };
static short Clamp16(int x) { return (short)(x < -32768 ? -32768 : x > 32767 ? 32767 : x); }
static int DecodeCh(const uint8_t *d, int len, int nsamp, short *out, int *h1p, int *h2p)
{
    int n = 0, o = 0, h1 = *h1p, h2 = *h2p;
    while (n < nsamp && o < len) {
        uint8_t fi = d[o];
        if (fi == 0xEE) { if (o + 0x3D > len) break; h1 = (short)((d[o + 1] << 8) | d[o + 2]); h2 = (short)((d[o + 3] << 8) | d[o + 4]);
            for (int i = 0; i < 28 && n < nsamp; i++) { out[n++] = (short)((d[o + 5 + 2 * i] << 8) | d[o + 6 + 2 * i]); }
            o += 0x3D; continue; }
        if (o + 0x0F > len) break;
        int c1 = kXa[(fi >> 4) & 3], c2 = kXa[4 + ((fi >> 4) & 3)], sh = (fi & 0x0F) + 8;
        for (int i = 0; i < 28 && n < nsamp; i++) { uint8_t b = d[o + 1 + i / 2]; int nib = (i & 1) ? (b & 0x0F) : (b >> 4);
            int s = (int)((unsigned)nib << 28) >> sh; s = (s + c1 * h1 + c2 * h2) >> 8; short v = Clamp16(s); out[n++] = v; h2 = h1; h1 = v; }
        o += 0x0F;
    }
    *h1p = h1; *h2p = h2; return n;
}
static uint32_t LE(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(len); if (fread(b, 1, len, f) != (size_t)len) return 1; fclose(f);
    if (memcmp(b, "SCHl", 4)) { printf("no SCHl\n"); return 1; }
    uint32_t hsz = LE(b + 4); int ch = 1; uint32_t rate = 0, total = 0, ver = 0; int o = 8 + ((b[8] == 'P' && b[9] == 'T') ? 4 : 0);
    while ((uint32_t)o < hsz) { uint8_t t = b[o++]; if (t == 0xFF) break; if (t == 0xFC || t == 0xFD || t == 0xFE) continue; uint8_t n = b[o++]; uint32_t v = 0;
        if (n > 4) { o += n; continue; } for (int k = 0; k < n; k++) v = (v << 8) | b[o++];
        if (t == 0x82) ch = v; else if (t == 0x84) rate = v; else if (t == 0x85) total = v; else if (t == 0x80) ver = v; }
    printf("header %u B: PT, version %u, %d ch, %u Hz, %u samples (%.3f s)\n", hsz, ver, ch, rate, total, rate ? total / (double)rate : 0);
    short *pcm = calloc((size_t)total * 2, sizeof(short)); short *tmp = malloc(sizeof(short) * 65536);
    int h1[2] = {0}, h2[2] = {0}; uint32_t got = 0; long p = hsz; int nb = 0, nc = 0, firstEE = 0;
    while (p + 8 <= len && got < total) {
        uint32_t bs = LE(b + p + 4); if (bs < 8) break;
        if (!memcmp(b + p, "SCEl", 4)) break;
        if (!memcmp(b + p, "SCCl", 4)) { nc = LE(b + p + 8); }
        else if (!memcmp(b + p, "SCDl", 4)) {
            uint32_t ns = LE(b + p + 8); uint32_t base = 0x0C + 4 * ch;
            for (int c = 0; c < ch; c++) { uint32_t st = base + LE(b + p + 0x0C + 4 * c), en = (c + 1 < ch) ? base + LE(b + p + 0x0C + 4 * (c + 1)) : bs;
                if (nb == 0 && b[p + st] == 0xEE) firstEE++;
                int n = DecodeCh(b + p + st, (int)(en - st), (int)ns, tmp, &h1[c], &h2[c]);
                for (int i = 0; i < n && got + i < total; i++) pcm[(got + i) * 2 + c] = tmp[i]; }
            got += ns; nb++;
        }
        p += bs;
    }
    printf("blocks: SCCl says %d, decoded %d SCDl, %u frames of %u; the first block's channels start with 0xEE: %d of %d\n", nc, nb, got, total, firstEE, ch);
    FILE *w = fopen(argv[2], "wb"); fwrite(pcm, 4, got, w); fclose(w);
    return got == total ? 0 : 3;
}

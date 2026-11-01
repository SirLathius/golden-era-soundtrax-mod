// bardec.c - the reference decode of one MW song: each bar of MW_Music.mus decoded on its own ('GSTR' = big-endian block fields, the
// decoder history reset at every bar, as MW plays its bars), concatenated to raw s16le stereo. The decoder core = streamdec.c's DecodeCh.
// The joined stream (streamdec on streams/mw_NN.bin) must decode to the same bytes.
// usage: bardec <MW_Music.mus> <bars_NN.txt (lines: wave offset size samples ms)> <out.raw>
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
static uint32_t BE(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    FILE *mus = fopen(argv[1], "rb"), *list = fopen(argv[2], "r"), *w = fopen(argv[3], "wb");
    if (!mus || !list || !w) { perror("open"); return 2; }
    long wave, off, size, want, ms; int bars = 0, bad = 0; unsigned long long frames = 0; const int ch = 2;
    short *tmp = malloc(sizeof(short) * 65536);
    while (fscanf(list, "%ld %ld %ld %ld %ld", &wave, &off, &size, &want, &ms) == 5) {
        uint8_t *b = malloc(size); fseek(mus, off, SEEK_SET);
        if (fread(b, 1, size, mus) != (size_t)size || memcmp(b, "SCHl", 4) || memcmp(b + 8, "GSTR", 4)) { printf("bar %ld: read / SCHl / GSTR failed\n", wave); bad++; free(b); continue; }
        uint32_t hsz = LE(b + 4); long p = hsz; int h1[2] = {0, 0}, h2[2] = {0, 0}; long got = 0;   // history reset per bar
        short *pcm = calloc((size_t)want * ch, sizeof(short));
        while (p + 8 <= size) {
            uint32_t bs = LE(b + p + 4); if (bs < 8) break;
            if (!memcmp(b + p, "SCEl", 4)) break;
            if (!memcmp(b + p, "SCDl", 4)) {
                uint32_t ns = BE(b + p + 8); uint32_t base = 0x0C + 4 * ch;
                for (int c = 0; c < ch; c++) { uint32_t st = base + BE(b + p + 0x0C + 4 * c), en = (c + 1 < ch) ? base + BE(b + p + 0x0C + 4 * (c + 1)) : bs;
                    int n = DecodeCh(b + p + st, (int)(en - st), (int)ns, tmp, &h1[c], &h2[c]);
                    for (int i = 0; i < n && got + i < want; i++) pcm[(got + i) * ch + c] = tmp[i]; }
                got += ns;
            }
            p += bs;
        }
        if (got != want) { printf("bar %ld: decoded %ld of %ld frames\n", wave, got, want); bad++; }
        fwrite(pcm, 2 * ch, want, w); frames += want; bars++; free(pcm); free(b);
    }
    fclose(w); printf("bars %d, frames %llu (%llu B raw s16le stereo), failures %d\n", bars, frames, frames * 4, bad);
    return bad != 0;
}

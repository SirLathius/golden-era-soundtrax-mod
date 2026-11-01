#!/bin/sh
# check_decode_mw.sh - every one of the 26 joined MW streams (streams/mw_NN.bin = the bytes add_mw_songs.py appends to MusicSFx.mus) is decoded
# with streamdec and compared byte for byte with the reference decode of the same song from MW's own file (bardec: each bar of MW_Music.mus on
# its own, big-endian fields, history reset per bar). Equal PCM = the join changed no sample.
# usage: check_decode_mw.sh <streamdec bin> <bardec bin> <step 1 OUTDIR with streams/> <MW_Music.mus> <result file>   (exit 0 = PASS)
SD=$1; BD=$2; O=$3; MUS=$4; R=$5; fails=0; : > "$R"
for s in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26; do
  a=$("$SD" "$O/streams/mw_$s.bin" "$O/dec_stream.raw" | tr '\n' ' '); b=$("$BD" "$MUS" "$O/streams/bars_$s.txt" "$O/dec_bars.raw" | tr '\n' ' ')
  if cmp -s "$O/dec_stream.raw" "$O/dec_bars.raw"; then v=PASS; else v=FAIL; fails=$((fails+1)); fi
  echo "$v song $s: joined stream decode == per-bar decode ($(stat -c %s "$O/dec_stream.raw") B raw; sha256 $(sha256sum "$O/dec_stream.raw" | cut -c1-16) vs $(sha256sum "$O/dec_bars.raw" | cut -c1-16)) | stream: $a| bars: $b" >> "$R"
done
echo "DECODE CHECK MW: 26 songs, $fails failures -> $([ $fails = 0 ] && echo PASS || echo FAIL)" >> "$R"; tail -1 "$R"
[ $fails = 0 ]

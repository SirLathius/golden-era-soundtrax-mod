#!/bin/sh
# check_decode_ug1.sh - every one of the 27 re-headered UG1 streams (streams/u1_NN.bin = the bytes add_ug1_songs.py appends to MusicSFx.mus as
# waves 70..96) is decoded with streamdec and compared byte for byte with its UG1 span (streams/ug1_NN.bin, cut from STRM_Music_UG0_MB.ast as
# it is) decoded the same way: only the header changed, so the PCM must be the same. Both decodes must be whole ('N frames of N', exit 0); the
# new header must read as 2 ch, 22050 Hz (the rate written out), the UG1 one as 2 ch, 0 Hz (no rate tag: the decoder prints the tag, the
# game's sound library uses its 22050 default).
# usage: check_decode_ug1.sh <streamdec bin> <step 2 OUTDIR with streams/> <result file>   (exit 0 = PASS)
SD=$1; O=$2; R=$3; fails=0; : > "$R"
for s in 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26; do
  a=$("$SD" "$O/streams/u1_$s.bin" "$O/dec_u1.raw"); ra=$?; a=$(echo "$a" | tr '\n' ' ')
  b=$("$SD" "$O/streams/ug1_$s.bin" "$O/dec_ug1.raw"); rb=$?; b=$(echo "$b" | tr '\n' ' ')
  v=PASS
  cmp -s "$O/dec_u1.raw" "$O/dec_ug1.raw" || v=FAIL
  case "$a" in *"2 ch, 22050 Hz"*) ;; *) v=FAIL;; esac
  case "$b" in *"2 ch, 0 Hz"*) ;; *) v=FAIL;; esac
  fa=$(echo "$a" | sed -n 's/.* \([0-9]*\) frames of \([0-9]*\);.*/\1 \2/p'); fb=$(echo "$b" | sed -n 's/.* \([0-9]*\) frames of \([0-9]*\);.*/\1 \2/p')
  [ -n "$fa" ] && [ "${fa% *}" = "${fa#* }" ] || v=FAIL
  [ "$fa" = "$fb" ] || v=FAIL
  [ "$ra" = 0 ] && [ "$rb" = 0 ] || v=FAIL
  [ $v = PASS ] || fails=$((fails+1))
  echo "$v UG1 stream $s (wave $((70 + ${s#0}))): re-headered decode == UG1 span decode ($(stat -c %s "$O/dec_u1.raw") B raw; sha256 $(sha256sum "$O/dec_u1.raw" | cut -c1-16) vs $(sha256sum "$O/dec_ug1.raw" | cut -c1-16); frames $fa / $fb; exit $ra / $rb) | re-headered: $a| UG1: $b" >> "$R"
done
echo "DECODE CHECK UG1: 27 streams, $fails failures -> $([ $fails = 0 ] && echo PASS || echo FAIL)" >> "$R"; tail -1 "$R"
[ $fails = 0 ]

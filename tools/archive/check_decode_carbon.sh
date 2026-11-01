#!/bin/sh
# check_decode_carbon.sh - every one of the 30 re-headered Carbon streams (streams/cb_NN.bin = the bytes add_carbon_songs.py appends to
# MusicSFx.mus as waves 97..126; NN = the Carbon wave 02..31) is decoded with streamdec and compared byte for byte with its Carbon span
# (streams/cbo_NN.bin, cut from cb_mus_0.mus as it is) decoded the same way: only the header changed, so the PCM must be the same. Both
# decodes must be whole ('N frames of N', exit 0); both headers must read as 2 ch, 36000 Hz (Carbon's own rate, kept). The PCM hash is also
# compared with the known one in data/carbon_pcm_sha256.txt (first 16 hex digits).
# usage: check_decode_carbon.sh <streamdec bin> <step 3 OUTDIR with streams/> <data/carbon_pcm_sha256.txt> <result file>   (exit 0 = PASS)
SD=$1; O=$2; PCM=$3; R=$4; fails=0; : > "$R"
for w in $(seq 2 31); do
  s=$(printf %02d $w)
  a=$("$SD" "$O/streams/cb_$s.bin" "$O/dec_cb.raw"); ra=$?; a=$(echo "$a" | tr '\n' ' ')
  b=$("$SD" "$O/streams/cbo_$s.bin" "$O/dec_cbo.raw"); rb=$?; b=$(echo "$b" | tr '\n' ' ')
  v=PASS
  cmp -s "$O/dec_cb.raw" "$O/dec_cbo.raw" || v=FAIL
  case "$a" in *"2 ch, 36000 Hz"*) ;; *) v=FAIL;; esac
  case "$b" in *"2 ch, 36000 Hz"*) ;; *) v=FAIL;; esac
  fa=$(echo "$a" | sed -n 's/.* \([0-9]*\) frames of \([0-9]*\);.*/\1 \2/p'); fb=$(echo "$b" | sed -n 's/.* \([0-9]*\) frames of \([0-9]*\);.*/\1 \2/p')
  [ -n "$fa" ] && [ "${fa% *}" = "${fa#* }" ] || v=FAIL
  [ "$fa" = "$fb" ] || v=FAIL
  [ "$ra" = 0 ] && [ "$rb" = 0 ] || v=FAIL
  h=$(sha256sum "$O/dec_cb.raw" | cut -c1-16); hp=$(grep "^Carbon wave $w -> " "$PCM" | sed -n 's/.*vs Carbon.s own \([0-9a-f]*\): SAME.*/\1/p')
  [ "$h" = "$hp" ] || v=FAIL
  [ $v = PASS ] || fails=$((fails+1))
  echo "$v Carbon wave $w (UG2 wave $((95 + w))): re-headered decode == Carbon span decode ($(stat -c %s "$O/dec_cb.raw") B raw; sha256 $h vs $(sha256sum "$O/dec_cbo.raw" | cut -c1-16); known $hp; frames $fa / $fb; exit $ra / $rb) | re-headered: $a| Carbon: $b" >> "$R"
done
echo "DECODE CHECK CARBON: 30 streams, $fails failures -> $([ $fails = 0 ] && echo PASS || echo FAIL)" >> "$R"; tail -1 "$R"
[ $fails = 0 ]

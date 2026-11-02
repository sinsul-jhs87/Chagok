#!/bin/bash
# 형식별 압축/해제 시간 (암호 유무). 사용: speed.sh <자료폴더> <결과파일>
export LANG=C.UTF-8 WINEDEBUG=-all ZLEVEL=${ZLEVEL:-5}
D=$1; OUT=$2; Z="wine $(cd "$(dirname "$0")/.." && pwd)/build/zipcli.exe"; W=$(mktemp -d)
cat $D/* >/dev/null 2>&1
t(){ local s=$(date +%s.%N); eval "$1" >/dev/null 2>&1; local e=$(date +%s.%N); echo "$e - $s" | bc; }
printf "형식\t암호\t압축(초)\t크기(MB)\t해제(초)\t확인\n" > $OUT
for f in zip tar.gz tar 7z; do for pw in 0 1; do
  [ $pw = 1 ] && [[ $f == tar* ]] && continue
  rm -rf $W/*; a=$W/out.$f
  if [ $pw = 1 ]; then c=$(t "CPW=Test#2026 $Z c $a 0 $D"); x=$(t "PW=Test#2026 $Z x $a $W/x"); else c=$(t "$Z c $a 0 $D"); x=$(t "$Z x $a $W/x"); fi
  sz=$(echo "$(stat -c %s $a)/1000000" | bc -l)
  diff -r $D $W/x/$(basename $D) >/dev/null 2>&1 && ok=OK || ok=DIFF
  printf "%s\t%s\t%.1f\t%.1f\t%.1f\t%s\n" $f $([ $pw = 1 ] && echo AES || echo -) $c $sz $x $ok | tee -a $OUT
done; done
rm -rf $W

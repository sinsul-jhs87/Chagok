#!/bin/bash
# 형식별 회귀 테스트: 리눅스 도구(7z, rar, tar, genisoimage, mtools, gcab)로 만든 파일을 차곡 엔진으로 열고 풀어 원본과 비교
export LANG=C.UTF-8 LC_ALL=C.UTF-8 WINEDEBUG=-all
Z="wine $(cd "$(dirname "$0")/.." && pwd)/build/zipcli.exe"   # build/7z.dll 필요
T=$(mktemp -d); cd $T
mkdir -p src/하위폴더 src/빈폴더; echo "안녕 7z" > src/한글.txt; yes "텍스트 줄" | head -c 2000000 > src/하위폴더/big.txt; head -c 1500000 /dev/urandom > src/rand.bin
7z a -bd t.7z src >/dev/null; 7z a -bd -pabc123 -mhe=on enc.7z src >/dev/null; 7z a -bd -v1m sp.7z src >/dev/null
rar a -idq t.rar src; rar a -idq -v1000k vol.rar src; rar a -idq -pabc123 enc.rar src
tar cf t.tar src; tar czf t.tar.gz src; cp t.tar.gz t.tgz; tar cjf t.tar.bz2 src; tar cJf t.tar.xz src
genisoimage -quiet -J -R -input-charset utf-8 -o t.iso src 2>/dev/null
(cd src && gcab -c ../t.cab 한글.txt rand.bin); (cd src && zip -qr ../t.jar .)
ok(){ echo "PASS $1"; }; ng(){ echo "FAIL $1"; }
for f in t.7z sp.7z.001 t.rar vol.part1.rar t.tar t.tar.gz t.tgz t.tar.bz2 t.tar.xz t.iso t.jar; do
  rm -rf o; $Z x $f o >/dev/null; d=o/src; [ -d $d ] || d=o; diff -r src $d >/dev/null && ok "풀기 $f" || ng "풀기 $f"; done
rm -rf o; $Z x t.cab o >/dev/null; cmp -s src/rand.bin o/rand.bin && ok "풀기 t.cab" || ng cab
rm -rf o; PW=abc123 $Z x enc.7z o >/dev/null; diff -r src o/src >/dev/null && ok "암호 7z(헤더 암호화)" || ng enc7z
rm -rf o; PW=abc123 $Z x enc.rar o >/dev/null; diff -r src o/src >/dev/null && ok "암호 rar" || ng encrar
PW=wrong $Z l enc.7z | grep -q "암호가 틀렸습니다" && ok "틀린 암호 안내" || ng wrongpw
mkdir add; echo 추가 > add/새파일.txt; cp t.7z w.7z; cp t.tar w.tar
$Z a w.7z add >/dev/null && 7z t w.7z | grep -q Everything && 7z l w.7z | grep -q 새파일 && ok "7z에 추가" || ng add7z
$Z a w.tar add >/dev/null && tar tf w.tar | grep -q 새파일 && ok "tar에 추가" || ng addtar
$Z a t.tgz add | grep -q "지원하지 않습니다" && ok "tar.gz 추가 → 읽기 전용 안내" || ng ro
$Z c n.7z 0 src >/dev/null && rm -rf o && 7z x -oo n.7z >/dev/null && diff -r src o/src && ok "7z 만들기(7-Zip으로 확인)" || ng mk7z
$Z c n.tar.gz 0 src >/dev/null && rm -rf o && mkdir o && tar xzf n.tar.gz -C o && diff -r src o/src && ok "tar.gz 만들기(GNU tar로 확인)" || ng mktgz
$Z c n.tar 0 src >/dev/null && rm -rf o && mkdir o && tar xf n.tar -C o && diff -r src o/src && ok "tar 만들기" || ng mktar
$Z c s.7z 1048576 src >/dev/null && [ -f s.7z.002 ] && 7z t s.7z.001 | grep -q Everything && ok "7z 분할 만들기" || ng sp7z
$Z c s.tar.gz 700000 src >/dev/null && cat s.tar.gz.0* | tar tz >/dev/null && ok "tar.gz 분할 만들기" || ng sptgz
cd /; rm -rf $T

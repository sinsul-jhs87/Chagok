#!/bin/bash
# 엔진 회귀 테스트 (wine + Info-ZIP + 7-Zip 교차 검증)
set -u
export LANG=C.UTF-8 WINEDEBUG=-all
Z="wine $(cd "$(dirname "$0")/.." && pwd)/build/zipcli.exe"
T=$(mktemp -d); cd $T
pass(){ echo "PASS $1"; }; failm(){ echo "FAIL $1"; }
mkdir -p src/sub/깊은폴더 src/빈폴더 add
echo "안녕하세요 hello" > src/한글파일.txt; head -c 3000000 /dev/urandom > src/sub/random.bin
yes "압축 잘 되는 텍스트 줄입니다" | head -c 5000000 > src/sub/깊은폴더/text.txt; : > src/empty.txt
head -c 2500000 /dev/urandom > src/photo.jpg; head -c 1048576 /dev/zero > src/exact1m.bin
echo new > add/새파일.txt
$Z c plain.zip 0 src >/dev/null && unzip -tq plain.zip >/dev/null && 7z t plain.zip | grep -q Everything && pass "일반 압축(Info-ZIP·7z 검사)" || failm plain
unzip -Z -v plain.zip | grep -A30 "photo.jpg" | grep -q "none (stored)" && pass "jpg는 저장 방식" || failm stored
$Z c sp.zip 1048576 src >/dev/null && [ -f sp.z01 ] && 7z t sp.zip | grep -q Everything && pass "분할 압축(7z 검사)" || failm split
rm -rf o; $Z x sp.zip o >/dev/null && diff -r src o/src && pass "분할 해제 = 원본" || failm splitx
rm -f iz.*; zip -qr -s 1m iz.zip src; rm -rf o; $Z x iz.zip o >/dev/null && diff -r src o/src && pass "Info-ZIP 분할 읽기" || failm izsplit
cp sp.z01 sq.z01; cp sp.zip sq.zip; for f in sp.z0*; do cp $f ${f/sp/sq}; done; $Z a sq.zip add >/dev/null && 7z t sq.zip | grep -q Everything && pass "분할 파일에 추가" || failm addsplit
split -b 1000000 -d -a 3 --numeric-suffixes=1 plain.zip raw.zip.; rm -rf o; $Z x raw.zip.001 o >/dev/null && diff -r src o/src && pass "7-Zip 방식 분할(.001) 읽기" || failm raw
python3 -c "
import zipfile
zipfile.ZipInfo._encodeFilenameFlags=lambda self:(self.filename.encode('cp437'),self.flag_bits)
with zipfile.ZipFile('cp949.zip','w') as z: z.writestr(zipfile.ZipInfo('한글폴더/문서.txt'.encode('cp949').decode('cp437')),'data')
with zipfile.ZipFile('many.zip','w') as z:
    for i in range(70000): z.writestr(f'd/{i}.txt','x')
"
$Z l cp949.zip | grep -q "한글폴더/문서.txt" && pass "CP949 파일명" || failm cp949
[ "$($Z l many.zip | grep -c "^F ")" = 70000 ] && $Z a many.zip add/새파일.txt >/dev/null && unzip -tq many.zip >/dev/null && pass "항목 7만 개(ZIP64) 읽기·추가" || failm many
mkdir -p big && truncate -s 4700000000 big/huge.bin && echo tail >> big/huge.bin
$Z c big.zip 0 big >/dev/null && unzip -tq big.zip >/dev/null && 7z t big.zip | grep -q Everything && pass "4.7GB 파일(ZIP64)" || failm big
$Z c bigs.zip 104857600 big >/dev/null && rm -rf o && $Z x bigs.zip o >/dev/null && cmp big/huge.bin o/big/huge.bin && pass "4.7GB 분할 압축·해제 = 원본" || failm bigsplit
cd /; rm -rf $T

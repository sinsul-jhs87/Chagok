#!/bin/sh
# 업데이트 위치(공유 폴더 / 웹 서버)에 올릴 파일 만들기
# 사용: make_update.sh <설치파일.exe> <버전> <출력 폴더>
# 바뀐 점은 installer/update-notes.txt 에서 읽는다 (여러 줄 가능, 비어 있으면 생략)
set -e
SETUP=$1; VER=$2; OUT=$3
mkdir -p "$OUT"
rm -f "$OUT"/ChagokSetup-*.exe
cp "$SETUP" "$OUT/"
NAME=$(basename "$SETUP")
SHA=$(sha256sum "$SETUP" | cut -d' ' -f1)
NOTES=""
if [ -f installer/update-notes.txt ]; then
  NOTES=$(sed -e 's/\r$//' installer/update-notes.txt | awk 'NF{printf "%s%s", (n++ ? "\\n" : ""), $0}')
fi
{
  echo "; 차곡 사내 업데이트 정보 — 이 파일과 $NAME 을 업데이트 위치에 함께 올리세요"
  echo "version=$VER"
  echo "file=$NAME"
  echo "sha256=$SHA"
  printf "notes=%s\n" "$NOTES"
} > "$OUT/chagok-update.ini"
echo "업데이트 파일: $OUT/chagok-update.ini + $NAME"

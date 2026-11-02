#!/bin/sh
# MinGW-w64 크로스/네이티브 빌드
set -e
CC=${CC:-x86_64-w64-mingw32-gcc}; CXX=${CXX:-x86_64-w64-mingw32-g++}; WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build
# 사내 서명 인증서 (한 번 만들어 두고 계속 씀). 지문은 자동 업데이트가 설치 파일 서명을 확인할 때 쓴다
sh installer/cert/make_cert.sh >/dev/null
CERT_THUMB=$(openssl x509 -in installer/cert/chagok_sign.pem -noout -fingerprint -sha1 | sed 's/.*=//; s/://g')
# 압축 엔진(deflate·ZIP·암호·CRC)은 속도 우선(-O2), 화면 코드는 크기 우선(-Os)
MZDEF="-DNDEBUG -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_TIME -DMINIZ_NO_STDIO -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES"
CXXDEF="-std=c++17 -DNDEBUG -DUNICODE -D_UNICODE -municode -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -Wall -Wno-unused-function"
$CC -c -O2 $MZDEF -ffunction-sections -fdata-sections src/miniz.c -o build/miniz.o
for f in zipcore zipcrypt fastcrc; do $CXX -c -O2 $CXXDEF src/$f.cpp -o build/$f.o; done
ENGINE="build/zipcore.o build/zipcrypt.o build/fastcrc.o build/miniz.o"
$WINDRES -c 65001 res/app.rc -O coff -o build/app.res.o
$CXX -Os $CXXDEF -mwindows -DCHAGOK_CERT_THUMB="L\"$CERT_THUMB\"" \
  src/main.cpp src/szarc.cpp $ENGINE build/app.res.o -o build/Chagok.exe \
  -static -s -Wl,--gc-sections -lgdiplus -ldwmapi -lshlwapi -lole32 -loleaut32 -luuid -lcomdlg32 -lshell32 -lgdi32 -luser32 -ladvapi32 -lbcrypt \
  -lwininet -lwintrust -lcrypt32 -lversion
ls -la build/Chagok.exe
# 자동 실행 압축 파일(SFX)의 풀기 프로그램 — 차곡이 이 파일 뒤에 ZIP 을 붙여 .exe 를 만든다
$WINDRES -c 65001 res/sfx.rc -O coff -o build/sfx.res.o
# (받는 사람마다 붙는 파일이라 크기 우선: 전체 LTO -Os. 풀기 속도는 deflate 해제가 -Os 와 -O2 에서 같고 CRC 는 fastcrc 가 맡는다)
$CC -c -Os -flto $MZDEF -ffunction-sections -fdata-sections src/miniz.c -o build/miniz_lto.o
$CXX -Os -flto $CXXDEF -mwindows \
  src/sfx.cpp src/zipcore.cpp src/zipcrypt.cpp src/fastcrc.cpp build/miniz_lto.o build/sfx.res.o -o build/ChagokSFX.bin \
  -static -s -Wl,--gc-sections -lcomctl32 -lole32 -luuid -lshell32 -lshlwapi -lbcrypt -lgdi32 -luser32
ls -la build/ChagokSFX.bin
# 탐색기 메뉴 확장 DLL
$WINDRES -c 65001 res/shell.rc -O coff -o build/shell.res.o
$CXX -std=c++17 -Os -DNDEBUG -DUNICODE -D_UNICODE -shared -fno-exceptions -fno-rtti -Wall \
  src/shellext.cpp src/shellext.def build/shell.res.o -o build/ChagokShell.dll \
  -static -s -Wl,--gc-sections -lshlwapi -lshell32 -lole32 -luuid -lgdi32 -luser32
ls -la build/ChagokShell.dll
# 사내 인증서로 실행 파일·메뉴 DLL 서명 (Authenticode)
sh installer/cert/make_cert.sh >/dev/null
SIGN() { osslsigncode sign -pkcs12 installer/cert/chagok_sign.pfx -pass "${CHAGOK_PFX_PASS:-chagok}" -h sha256 -n "차곡" -in "$1" -out "$1.signed" >/dev/null && mv "$1.signed" "$1"; }
SIGN build/Chagok.exe && SIGN build/ChagokShell.dll && echo "Chagok.exe / ChagokShell.dll 서명 완료"
# Windows 11 첫 화면 메뉴용 스파스 패키지 (사내 인증서로 서명)
VER=$(grep -m1 '!define VERSION' installer/chagok.nsi | sed 's/.*"\(.*\)".*/\1/')
sh installer/cert/make_cert.sh >/dev/null
python3 tools/mkmsix.py installer/package build/ChagokShell-unsigned.msix "$VER.0"
rm -f build/ChagokShell.msix
osslsigncode sign -pkcs12 installer/cert/chagok_sign.pfx -pass "${CHAGOK_PFX_PASS:-chagok}" -in build/ChagokShell-unsigned.msix -out build/ChagokShell.msix >/dev/null
osslsigncode verify -CAfile installer/cert/chagok_sign.pem -in build/ChagokShell.msix >/dev/null && echo "ChagokShell.msix 서명 확인"
CERT_THUMB=$(openssl x509 -in installer/cert/chagok_sign.pem -noout -fingerprint -sha1 | sed 's/.*=//; s/://g')
# 7z.dll (7-Zip 24.09 x64 공식 배포본, LGPL) — 다른 형식 지원용. 없으면 받아서 꺼낸다
if [ ! -f build/7z.dll ]; then
  mkdir -p build/7zpkg && curl -sSL -o build/7zpkg/7z.exe https://github.com/ip7z/7zip/releases/download/24.09/7z2409-x64.exe \
  && 7z x -y -obuild/7zpkg build/7zpkg/7z.exe 7z.dll License.txt >/dev/null && cp build/7zpkg/7z.dll build/ && cp build/7zpkg/License.txt build/7-Zip_License.txt
fi
# 설치 프로그램 (NSIS 가 있으면)
if command -v makensis >/dev/null 2>&1; then
  # 사내 업데이트 위치를 설치할 때 넣으려면: CHAGOK_UPDATE_URL='\\파일서버\공유\차곡' sh build_mingw.sh
  (cd installer && makensis -V2 -DCERT_THUMB=$CERT_THUMB ${CHAGOK_UPDATE_URL:+"-DUPDATE_URL=$CHAGOK_UPDATE_URL"} chagok.nsi) && \
  SIGN "build/ChagokSetup-$VER.exe" && echo "설치 프로그램 서명 완료" && ls -la "build/ChagokSetup-$VER.exe"
  # 업데이트 위치에 올릴 두 파일: build/update/ (설치 파일 + chagok-update.ini)
  sh tools/make_update.sh "build/ChagokSetup-$VER.exe" "$VER" build/update
fi

#!/usr/bin/env python3
"""스파스 MSIX 패키지 만들기 (Windows SDK 의 makeappx 대신, 리눅스에서도 동작)
사용법: mkmsix.py <패키지 폴더> <출력.msix> <버전 a.b.c.d>

Windows 는 makeappx 와 같은 ZIP 구조만 받아 주므로(다르면 0x8007000B '패키지를 열 수 없음')
Microsoft MSIX SDK(ZipObjectWriter) 와 똑같이 쓴다:
  - 모든 항목: 로컬 헤더 버전 4.5, 플래그 0x08(데이터 설명자), CRC/크기 0 → 뒤에 ZIP64 데이터 설명자
  - 중앙 디렉터리: 만든 버전 4.5, 외부 속성 0
  - ZIP64 끝 레코드 + 위치 정보 + (값이 모두 0xFFFF…인) 일반 끝 레코드
  - AppxBlockMap.xml: 파일별 64KB 블록 SHA-256, 압축 안 한 파일은 Block 에 Size 없음
  - [Content_Types].xml 은 deflate (서명 도구 osslsigncode 가 다시 쓰는 파일)
서명은 이어서 osslsigncode 로 한다 (AppxSignature.p7x 추가)."""
import sys, os, zlib, hashlib, base64, struct

src, out, version = sys.argv[1], sys.argv[2], sys.argv[3]
files = []  # (zip 이름, 데이터)
man = open(os.path.join(src, 'AppxManifest.xml'), encoding='utf-8').read().replace('@VERSION@', version)
files.append(('AppxManifest.xml', man.encode('utf-8')))
for root, _, names in sorted(os.walk(os.path.join(src, 'Assets'))):
    for n in sorted(names):
        p = os.path.join(root, n)
        files.append((os.path.relpath(p, src).replace(os.sep, '/'), open(p, 'rb').read()))

DOS_TIME, DOS_DATE = 0, (2026 - 1980) << 9 | 1 << 5 | 1
buf = bytearray()
central = []  # (이름, crc, 압축 크기, 원래 크기, 오프셋, 방식)

def add(name, data, deflate=False):
    off = len(buf)
    nb = name.encode('utf-8')
    if deflate:
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        body = c.compress(data) + c.flush()
        method = 8
    else:
        body, method = data, 0
    crc = zlib.crc32(data) & 0xFFFFFFFF
    buf.extend(struct.pack('<IHHHHHIIIHH', 0x04034b50, 45, 0x0008, method, DOS_TIME, DOS_DATE, 0, 0, 0, len(nb), 0))
    buf.extend(nb)
    buf.extend(body)
    buf.extend(struct.pack('<IIQQ', 0x08074b50, crc, len(body), len(data)))
    central.append((nb, crc, len(body), len(data), off, method))
    return 30 + len(nb)  # LfhSize

def blockmap(entries):
    x = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
         '<BlockMap xmlns="http://schemas.microsoft.com/appx/2010/blockmap" HashMethod="http://www.w3.org/2001/04/xmlenc#sha256">']
    for name, data, lfh in entries:
        x.append(f'<File Name="{name.replace("/", chr(92))}" Size="{len(data)}" LfhSize="{lfh}">')
        for i in range(0, len(data), 65536):
            x.append(f'<Block Hash="{base64.b64encode(hashlib.sha256(data[i:i + 65536]).digest()).decode()}"/>')
        x.append('</File>')
    x.append('</BlockMap>')
    return '\r\n'.join(x).encode('utf-8')

ctypes = ('<?xml version="1.0" encoding="UTF-8"?>\r\n'
          '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="png" ContentType="image/png"/>'
          '<Override PartName="/AppxManifest.xml" ContentType="application/vnd.ms-appx.manifest+xml"/>'
          '<Override PartName="/AppxBlockMap.xml" ContentType="application/vnd.ms-appx.blockmap+xml"/>'
          '</Types>').encode('utf-8')

entries = [(name, data, add(name, data)) for name, data in files]
add('AppxBlockMap.xml', blockmap(entries))
add('[Content_Types].xml', ctypes, deflate=True)

cd_off = len(buf)
for nb, crc, cs, us, off, method in central:
    buf.extend(struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 45, 45, 0x0008, method, DOS_TIME, DOS_DATE,
                           crc, cs, us, len(nb), 0, 0, 0, 0, 0, off))
    buf.extend(nb)
cd_size = len(buf) - cd_off
z64 = len(buf)
buf.extend(struct.pack('<IQHHIIQQQQ', 0x06064b50, 44, 45, 45, 0, 0, len(central), len(central), cd_size, cd_off))
buf.extend(struct.pack('<IIQI', 0x07064b50, 0, z64, 1))
buf.extend(struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 0xFFFF, 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0))
open(out, 'wb').write(buf)
print('OK', out, len(files), 'files')

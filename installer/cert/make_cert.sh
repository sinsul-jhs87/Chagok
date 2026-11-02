#!/bin/sh
# 사내용 자체 서명 코드 서명 인증서 만들기 (한 번만 실행. 이미 있으면 그대로 둔다)
#  - chagok_sign.pfx : 개인 키 포함 → 회사 안에서만 보관 (유출 금지)
#  - chagok_sign.cer : 공개 인증서 → 설치 프로그램이 PC 의 '신뢰할 수 있는 사용자' 저장소에 등록
# 패키지 매니페스트의 Publisher 는 인증서 주체(CN=Sinsul Chagok)와 정확히 같아야 한다.
set -e
cd "$(dirname "$0")"
[ -f chagok_sign.pfx ] && { echo "이미 있음: chagok_sign.pfx"; exit 0; }
cat > cert.cnf <<'CNF'
[req]
distinguished_name = dn
prompt = no
x509_extensions = ext
[dn]
CN = Sinsul Chagok
[ext]
basicConstraints = critical, CA:FALSE
keyUsage = critical, digitalSignature
extendedKeyUsage = codeSigning
subjectKeyIdentifier = hash
CNF
openssl req -x509 -newkey rsa:3072 -sha256 -days 3650 -nodes -keyout chagok_sign.key -out chagok_sign.pem -config cert.cnf
openssl pkcs12 -export -inkey chagok_sign.key -in chagok_sign.pem -out chagok_sign.pfx -passout pass:${CHAGOK_PFX_PASS:-chagok} -keypbe AES-256-CBC -certpbe AES-256-CBC -macalg sha256
openssl x509 -in chagok_sign.pem -outform der -out chagok_sign.cer
rm -f chagok_sign.key cert.cnf
openssl x509 -in chagok_sign.pem -noout -subject -enddate -fingerprint -sha1

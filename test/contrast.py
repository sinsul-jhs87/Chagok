# themes.h 의 색상표에서 글자/배경 조합의 대비(WCAG)를 검사한다
import re,sys
src=open(sys.argv[1] if len(sys.argv)>1 else 'src/themes.h',encoding='utf-8').read()
fields=re.search(r'COLORREF(.*?);',src,re.S).group(1)
fields=[f.strip() for f in fields.replace('\n',' ').split(',') if f.strip()]
pals={}
for m in re.finditer(r'static const Palette (\w+) = \{(.*?)\};',src,re.S):
    vals=re.findall(r'H\(0x([0-9a-f]{6})\)|\bNO\b',m.group(2))
    toks=re.findall(r'H\(0x[0-9a-f]{6}\)|\bNO\b',m.group(2))
    pals[m.group(1)]=dict(zip(fields,[None if t=='NO' else int(t[4:10],16) for t in toks]))
    assert len(toks)==len(fields),(m.group(1),len(toks))
def lum(c):
    r,g,b=[(c>>s&255)/255 for s in (16,8,0)]
    f=lambda x:x/12.92 if x<=0.03928 else ((x+0.055)/1.055)**2.4
    return 0.2126*f(r)+0.7152*f(g)+0.0722*f(b)
def cr(a,b):
    la,lb=lum(a),lum(b); return (max(la,lb)+0.05)/(min(la,lb)+0.05)
# (글자, 배경, 최소) — 본문 4.5, 굵은 버튼 글자 3.5
pairs=[('text','panel',4.5),('dim','panel',4.5),('text','listBg',4.5),('dim','listBg',4.5),('text','bg',4.5),('dim','bg',4.0),
       ('text','sel',4.5),('dim','sel',3.5),('text','hover',4.5),('sideText','side',4.5),('sideDim','side',3.5),('sideSelText','sideSel',4.5),
       ('sideText','sideHover',4.5),('onAccent','accent',4.5),('onAccent','accent2',4.0),('onPrimary','primary',4.5),('onPrimary','primary2',4.0),
       ('onBand','band',4.5),('accentFg','panel',4.5),('accentFg','listBg',4.0),('text','btn',4.5),('sideText','bg',4.5),('sideDim','bg',3.5),('sideText','bg2',4.5)]
bad=0
for n,p in pals.items():
    for a,b,mn in pairs:
        if p[a] is None or p[b] is None: continue
        if b in ('bg','bg2') and a.startswith('side') and p['side'] is not None: continue
        v=cr(p[a],p[b])
        if v<mn: bad+=1; print(f'{n:12s} {a:12s} on {b:9s} {v:4.2f} < {mn}')
print('검사', len(pals), '팔레트, 미달', bad)

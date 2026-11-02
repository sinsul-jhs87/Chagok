# 테마 시안 5종 HTML → PNG (Playwright)
import asyncio
from playwright.async_api import async_playwright
ROWS=[("dir","도면","","","2026-10-25 09:12"),("dir","현장사진","","","2026-10-26 17:40"),
("file","제안서_최종.hwp","2.4 MB","1.9 MB","2026-10-27 14:32"),("file","견적서.xlsx","86 KB","41 KB","2026-10-27 11:05"),
("file","회의록_1028.docx","152 KB","98 KB","2026-10-28 08:50"),("file","구성도.pdf","3.2 MB","2.9 MB","2026-10-24 16:21"),
("file","장비목록.xlsx","64 KB","29 KB","2026-10-25 10:15"),("file","readme.txt","3 KB","1 KB","2026-10-20 10:00")]
SEL=2
ICO={
"zip":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><rect x="4" y="3" width="16" height="18" rx="2.5"/><path d="M12 3v2.5M12 7.5v2M12 11.5V13"/><rect x="10" y="15" width="4" height="3.5" rx=".8"/></svg>',
"split":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><rect x="3" y="4" width="7" height="16" rx="1.5"/><rect x="14" y="4" width="7" height="16" rx="1.5"/></svg>',
"ext":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M4 8v12h16V8M4 8l2-4h12l2 4H4M12 11v6M9 14l3 3 3-3"/></svg>',
"plus":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 5v14M5 12h14"/></svg>',
"down":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>',
"theme":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="8"/><path d="M12 4a8 8 0 0 0 0 16z" fill="currentColor"/></svg>',
"back":'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M19 12H5M11 6l-6 6 6 6"/></svg>',
}
def folder(c1,c2): return f'<svg viewBox="0 0 24 24"><path d="M3 6.5A1.5 1.5 0 0 1 4.5 5h5l2 2h8A1.5 1.5 0 0 1 21 8.5v9a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 17.5z" fill="{c1}" stroke="{c2}" stroke-width="1"/></svg>'
def fileico(c1,c2): return f'<svg viewBox="0 0 24 24"><path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z" fill="{c1}" stroke="{c2}" stroke-width="1.2"/><path d="M14 3v5h5" fill="none" stroke="{c2}" stroke-width="1.2"/></svg>'
def rows(fc,ff):
    out=[]
    for i,(k,n,s,c,d) in enumerate(ROWS):
        ic=folder(*fc) if k=="dir" else fileico(*ff)
        out.append(f'<div class="row{" sel" if i==SEL else ""}"><span class="nm"><i class="ic">{ic}</i>{n}</span><span class="r">{s}</span><span class="r">{c}</span><span class="dt">{d}</span></div>')
    return "\n".join(out)
BASE='''<html><head><meta charset="utf-8"><style>
*{box-sizing:border-box;margin:0;padding:0} body{width:1100px;height:700px;overflow:hidden;font-family:"NanumGothic","Malgun Gothic",sans-serif}
svg{width:100%;height:100%;display:block} i.ic{display:inline-block;width:20px;height:20px;flex:none}
.row{display:grid;grid-template-columns:1fr 100px 100px 150px;align-items:center}
.row .nm{display:flex;align-items:center;gap:10px;min-width:0} .r{text-align:right} .dt{padding-left:24px}
.b{display:inline-flex;align-items:center;gap:8px} .b i{width:18px;height:18px;display:inline-block}
%CSS%</style></head><body>%BODY%</body></html>'''
T={}
# 1. 미드나잇 네온 (다크 + 유리 패널, 청록·보라 강조)
T["1_미드나잇"]=dict(css='''
body{background:radial-gradient(1200px 700px at 85% -10%,#2b1d5a 0,#0e1426 55%) ,#0e1426;color:#e7ecf7}
.top{height:46px;display:flex;align-items:center;padding:0 18px;gap:10px;font-size:13px;color:#9aa7c7}
.top b{color:#e7ecf7;font-size:14px}.wrap{display:flex;gap:16px;padding:0 16px 16px;height:654px}
.side{width:210px;display:flex;flex-direction:column;gap:8px}
.nb{height:48px;border-radius:12px;display:flex;align-items:center;gap:12px;padding:0 16px;color:#cdd6ee;font-size:14px}
.nb i{width:20px;height:20px;display:inline-block}.nb.on{background:linear-gradient(90deg,#14b8a6,#6d5df6);color:#fff;box-shadow:0 6px 24px #6d5df655}
.card{margin-top:auto;border-radius:14px;padding:16px;background:#ffffff0d;border:1px solid #ffffff1a;font-size:12px;color:#9aa7c7;line-height:1.8}
.card b{color:#fff;font-size:14px}.bar{height:5px;border-radius:3px;background:#ffffff1a;margin:6px 0}.bar div{width:78%;height:5px;border-radius:3px;background:linear-gradient(90deg,#14b8a6,#6d5df6)}
.main{flex:1;border-radius:16px;background:#ffffff0a;border:1px solid #ffffff17;backdrop-filter:blur(8px);display:flex;flex-direction:column}
.cmd{height:60px;display:flex;align-items:center;padding:0 18px;gap:10px;border-bottom:1px solid #ffffff12}
.crumb{flex:1;font-size:14px;color:#9aa7c7}.crumb b{color:#fff}.ob{height:34px;padding:0 14px;border-radius:10px;border:1px solid #ffffff22;color:#e7ecf7;font-size:13px}
.ob.ac{border:none;background:#14b8a6;color:#03211e;font-weight:700}
.hd{color:#7f8bab;font-size:12px;padding:12px 26px 6px}.list{padding:0 12px}
.row{height:40px;padding:0 14px;border-radius:10px;font-size:14px;color:#dfe5f3}.row .r,.row .dt{color:#8e9bbd}.row.sel{background:#6d5df633;outline:1px solid #6d5df699}
.drop{margin:auto 18px 16px;height:54px;border:1px dashed #ffffff30;border-radius:12px;display:flex;align-items:center;justify-content:center;color:#8e9bbd;font-size:13px;gap:8px}
''',body=lambda:f'''<div class="top"><b>차곡</b> · 제안서_자료.zip</div><div class="wrap"><div class="side">
<div class="nb on"><i>{ICO["zip"]}</i>새로 압축</div><div class="nb"><i>{ICO["split"]}</i>분할 압축</div><div class="nb"><i>{ICO["ext"]}</i>압축 해제</div>
<div class="card"><b>제안서_자료.zip</b><br>항목 33개 · ZIP · 암호<br>31.6 MB → 24.8 MB<div class="bar"><div></div></div>압축률 22%</div></div>
<div class="main"><div class="cmd"><span class="b crumb"><i>{ICO["back"]}</i>제안서_자료.zip › <b>제안서_자료</b></span><span class="b ob"><i>{ICO["plus"]}</i>파일 추가</span><span class="b ob ac"><i>{ICO["down"]}</i>선택 추출</span><span class="b ob"><i>{ICO["theme"]}</i></span></div>
<div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div><div class="list">{rows(("#5eead4","#14b8a6"),("#1b2440","#7f8bab"))}</div>
<div class="drop">여기로 파일을 끌어다 놓으면 압축 파일에 추가됩니다</div></div></div>''')
# 2. 한지 (따뜻한 미색 + 먹색, 명조 제목)
T["2_한지"]=dict(css='''
body{background:#f5f0e6;color:#2b2622}.top{height:64px;display:flex;align-items:flex-end;padding:0 32px 10px;gap:14px;border-bottom:2px solid #2b2622}
.top h1{font-family:"NanumMyeongjo",serif;font-size:28px;font-weight:800;letter-spacing:2px}.top span{color:#8a7f72;font-size:13px;padding-bottom:6px}
.tb{display:flex;gap:28px;padding:16px 32px;font-size:15px;align-items:center;border-bottom:1px solid #d9cfbd}
.tb .b{color:#2b2622}.tb .b i{color:#a0522d}.tb .sp{flex:1}.tb .seal{background:#a83a2a;color:#fff7ee;padding:8px 16px;border-radius:2px;font-weight:700}
.path{padding:12px 32px;font-family:"NanumMyeongjo",serif;font-size:15px;color:#6b6157}.path b{color:#2b2622}
.hd{color:#8a7f72;font-size:12px;padding:6px 32px;border-bottom:1px solid #d9cfbd}.list{padding:0 20px}
.row{height:42px;padding:0 12px;font-size:15px;border-bottom:1px dotted #d9cfbd}.row .r,.row .dt{color:#8a7f72;font-size:13px}
.row.sel{background:#efe2c8;border-left:4px solid #a83a2a}
.st{position:absolute;bottom:0;left:0;right:0;height:38px;display:flex;align-items:center;padding:0 32px;font-size:12px;color:#8a7f72;border-top:2px solid #2b2622;justify-content:space-between}
''',body=lambda:f'''<div class="top"><h1>차곡</h1><span>제안서_자료.zip</span></div>
<div class="tb"><span class="b"><i>{ICO["zip"]}</i>새로 압축</span><span class="b"><i>{ICO["split"]}</i>분할 압축</span><span class="b"><i>{ICO["ext"]}</i>압축 해제</span><span class="sp"></span><span class="b"><i>{ICO["plus"]}</i>파일 추가</span><span class="b seal"><i>{ICO["down"]}</i>선택 추출</span><span class="b"><i>{ICO["theme"]}</i></span></div>
<div class="path">제안서_자료.zip ／ <b>제안서_자료</b></div><div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>
<div class="list">{rows(("#e7c98f","#a0522d"),("#fffaf0","#8a7f72"))}</div><div class="st"><span>1개 선택 (2.4 MB) · 33개 항목 · 31.6 MB → 24.8 MB</span><span>ZIP · 압축률 22%</span></div>''')
# 3. 레트로 터미널
T["3_터미널"]=dict(css='''
body{background:#0b0f0b;color:#7CFC9A;font-family:"NanumGothicCoding","D2Coding",monospace;font-size:14px}
.top{height:34px;background:#7CFC9A;color:#0b0f0b;display:flex;align-items:center;padding:0 14px;font-weight:700;gap:20px}
.tb{display:flex;gap:6px;padding:12px 14px;border-bottom:1px solid #1f3d24}.k{border:1px solid #2f6b3a;padding:6px 12px;color:#9dffb4}.k u{text-decoration:none;color:#0b0f0b;background:#7CFC9A;padding:0 4px;margin-right:6px}
.k.on{background:#123a1c}.sp{flex:1}.path{padding:10px 14px;color:#4fbf6a}.path b{color:#d6ffe0}
.hd{color:#3f8f52;padding:4px 14px;border-bottom:1px dashed #1f3d24}.row{height:30px;padding:0 14px}.row .r,.row .dt{color:#4fbf6a}
.row.sel{background:#7CFC9A;color:#0b0f0b}.row.sel .r,.row.sel .dt{color:#0b0f0b}.row i.ic{width:16px;height:16px}
.st{position:absolute;bottom:0;left:0;right:0;height:34px;border-top:1px solid #1f3d24;display:flex;align-items:center;padding:0 14px;gap:18px;color:#4fbf6a}
.st .p{flex:1;height:10px;border:1px solid #2f6b3a}.st .p div{width:54%;height:8px;background:repeating-linear-gradient(90deg,#7CFC9A 0 6px,transparent 6px 8px)}
''',body=lambda:f'''<div class="top">CHAGOK v1.8 <span>제안서_자료.zip</span></div>
<div class="tb"><span class="k on"><u>F1</u>압축</span><span class="k"><u>F2</u>분할 압축</span><span class="k"><u>F3</u>압축 해제</span><span class="sp"></span><span class="k"><u>F5</u>추가</span><span class="k"><u>F6</u>추출</span><span class="k"><u>F9</u>테마</span></div>
<div class="path">&gt; 제안서_자료.zip/<b>제안서_자료/</b>_</div><div class="row hd"><span>NAME</span><span class="r">SIZE</span><span class="r">PACKED</span><span class="dt">MODIFIED</span></div>
{rows(("#0b0f0b","#7CFC9A"),("#0b0f0b","#4fbf6a"))}
<div class="st"><span>분할 압축 중 part 3/5</span><span class="p"><div></div></span><span>54%</span><span>[ESC] 취소</span></div>''')
# 4. 파스텔
T["4_파스텔"]=dict(css='''
body{background:linear-gradient(135deg,#fdf2f8,#eef2ff 50%,#ecfeff);color:#3b3355}
.wrap{display:flex;height:700px;padding:20px;gap:20px}.side{width:96px;background:#ffffffcc;border-radius:28px;display:flex;flex-direction:column;align-items:center;padding:22px 0;gap:14px;box-shadow:0 10px 30px #c4b5fd33}
.logo{width:48px;height:48px;border-radius:16px;background:linear-gradient(135deg,#f9a8d4,#a5b4fc);color:#fff;display:flex;align-items:center;justify-content:center;font-weight:800;font-size:20px;margin-bottom:10px}
.nb{width:72px;padding:10px 0;border-radius:18px;display:flex;flex-direction:column;align-items:center;gap:6px;font-size:11px;color:#6b6385}.nb i{width:26px;height:26px;display:block}
.nb.on{background:#ede9fe;color:#6d28d9}.main{flex:1;background:#ffffffd9;border-radius:28px;box-shadow:0 10px 30px #c4b5fd33;display:flex;flex-direction:column;padding:22px 26px}
.cmd{display:flex;align-items:center;gap:10px}.ttl{flex:1}.ttl h2{font-size:22px}.ttl span{font-size:13px;color:#8b84a6}
.pill{height:38px;padding:0 18px;border-radius:19px;background:#f1f5f9;font-size:13px;color:#3b3355}.pill.ac{background:linear-gradient(90deg,#f472b6,#818cf8);color:#fff;font-weight:700}
.chips{display:flex;gap:8px;margin:14px 0 8px}.chip{padding:6px 12px;border-radius:12px;background:#fdf4ff;font-size:12px;color:#a21caf}.chip.b{background:#ecfeff;color:#0e7490}.chip.c{background:#f0fdf4;color:#15803d}
.hd{color:#a39cbd;font-size:12px;padding:8px 14px}.row{height:44px;padding:0 14px;border-radius:14px;font-size:14px}.row .r,.row .dt{color:#8b84a6}
.row.sel{background:#ede9fe}.row:nth-child(even):not(.sel){background:#fafaff}
''',body=lambda:f'''<div class="wrap"><div class="side"><div class="logo">차</div><div class="nb on"><i>{ICO["zip"]}</i>압축</div><div class="nb"><i>{ICO["split"]}</i>분할</div><div class="nb"><i>{ICO["ext"]}</i>풀기</div><div class="nb" style="margin-top:auto"><i>{ICO["theme"]}</i>테마</div></div>
<div class="main"><div class="cmd"><div class="ttl"><h2>제안서_자료</h2><span>제안서_자료.zip 안</span></div><span class="b pill"><i>{ICO["plus"]}</i>파일 추가</span><span class="b pill ac"><i>{ICO["down"]}</i>선택 추출</span></div>
<div class="chips"><span class="chip">항목 33개</span><span class="chip b">31.6 MB → 24.8 MB</span><span class="chip c">압축률 22%</span></div>
<div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>{rows(("#fde68a","#f59e0b"),("#f5f3ff","#a78bfa"))}</div></div>''')
# 5. 타일(그리드) 보기 + 오피스 리본
TILES="".join(f'<div class="tile{" sel" if i==SEL else ""}"><i class="big">{folder("#ffd36b","#e0a800") if k=="dir" else fileico("#ffffff","#8a94a6")}</i><b>{n}</b><span>{s or "폴더"}</span></div>' for i,(k,n,s,c,d) in enumerate(ROWS))
T["5_리본_타일"]=dict(css='''
body{background:#f3f6fb;color:#1f2937}.title{height:34px;background:#1d4ed8;color:#fff;display:flex;align-items:center;padding:0 14px;font-size:13px;gap:10px}
.tabs{background:#1d4ed8;display:flex;gap:2px;padding:0 10px}.tab{padding:7px 16px;color:#dbeafe;font-size:13px}.tab.on{background:#f8fafc;color:#1d4ed8;border-radius:6px 6px 0 0;font-weight:700}
.ribbon{background:#f8fafc;border-bottom:1px solid #d6dde8;display:flex;padding:8px 10px;gap:4px;height:96px}
.grp{display:flex;flex-direction:column;border-right:1px solid #e2e8f0;padding:0 10px}.grp .btns{display:flex;gap:4px;flex:1}.grp .lbl{font-size:11px;color:#94a3b8;text-align:center}
.big{width:68px;display:flex;flex-direction:column;align-items:center;gap:6px;font-size:12px;padding-top:4px}.big i{width:30px;height:30px;color:#1d4ed8}
.sel2{display:flex;flex-direction:column;gap:6px;font-size:12px;justify-content:center}.sel2 span{border:1px solid #cbd5e1;background:#fff;padding:4px 10px;border-radius:4px}
.path{padding:10px 18px;font-size:13px;color:#64748b}.path b{color:#1f2937}
.grid{display:grid;grid-template-columns:repeat(6,1fr);gap:14px;padding:6px 18px}.tile{background:#fff;border:1px solid #e2e8f0;border-radius:10px;height:138px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;font-size:13px}
.tile i.big{width:54px;height:54px;padding:0}.tile span{color:#94a3b8;font-size:11px}.tile.sel{border:2px solid #1d4ed8;background:#eff6ff}
.st{position:absolute;bottom:0;left:0;right:0;height:30px;background:#1d4ed8;color:#dbeafe;display:flex;align-items:center;padding:0 14px;font-size:12px;justify-content:space-between}
''',body=lambda:f'''<div class="title"><b>차곡</b> — 제안서_자료.zip</div><div class="tabs"><span class="tab on">홈</span><span class="tab">보기</span><span class="tab">설정</span></div>
<div class="ribbon"><div class="grp"><div class="btns"><span class="big"><i>{ICO["zip"]}</i>새로 압축</span><span class="big"><i>{ICO["split"]}</i>분할 압축</span><span class="big"><i>{ICO["ext"]}</i>압축 해제</span></div><div class="lbl">압축</div></div>
<div class="grp"><div class="btns"><span class="big"><i>{ICO["plus"]}</i>파일 추가</span><span class="big"><i>{ICO["down"]}</i>선택 추출</span></div><div class="lbl">항목</div></div>
<div class="grp"><div class="btns sel2"><span>▦ 큰 아이콘</span><span>☰ 자세히</span></div><div class="lbl">보기</div></div></div>
<div class="path">제안서_자료.zip ▸ <b>제안서_자료</b></div><div class="grid">{TILES}</div><div class="st"><span>1개 선택 (2.4 MB) · 33개 항목</span><span>ZIP · 31.6 MB → 24.8 MB</span></div>''')
async def main():
    async with async_playwright() as p:
        b=await p.chromium.launch(); pg=await b.new_page(viewport={"width":1100,"height":700})
        for k,v in T.items():
            await pg.set_content(BASE.replace("%CSS%",v["css"]).replace("%BODY%",v["body"]()))
            await pg.screenshot(path=f"{k}.png")
        await b.close()
if __name__ == "__main__": asyncio.run(main())

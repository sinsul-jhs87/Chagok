# 테마 시안 6~10: 왼쪽 탐색기(폴더 트리) 포함
import asyncio
from gen import ROWS, SEL, ICO, folder, fileico, rows, BASE
from playwright.async_api import async_playwright
CH='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 6l6 6-6 6"/></svg>'
CD='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>'
ZIPI='<svg viewBox="0 0 24 24"><rect x="4" y="3" width="16" height="18" rx="2.5" fill="#fbbf24" stroke="#b45309"/><path d="M12 3v2.5M12 7.5v2M12 11.5V13" stroke="#7c2d12" stroke-width="1.6"/></svg>'
DRV='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><rect x="3" y="7" width="18" height="10" rx="2"/><circle cx="17" cy="12" r="1" fill="currentColor"/></svg>'
PC='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><rect x="3" y="4" width="18" height="12" rx="1.5"/><path d="M8 20h8M12 16v4"/></svg>'
def tree(fc, items=None):
    # (깊이, 종류, 이름, 펼침, 선택)
    items = items or [(0,"zip","제안서_자료.zip",1,0),(1,"dir","제안서_자료",1,1),(2,"dir","도면",1,0),(3,"dir","평면도",0,0),(2,"dir","현장사진",0,0)]
    out=[]
    for d,k,n,o,s in items:
        ic = ZIPI if k=="zip" else folder(*fc) if k=="dir" else DRV if k=="drv" else PC
        ch = CD if o==1 else CH if o==0 else ""
        out.append(f'<div class="tn{" on" if s else ""}" style="padding-left:{10+d*16}px"><i class="cv">{ch}</i><i class="ic">{ic}</i>{n}</div>')
    return "\n".join(out)
TREECSS='.tn{height:30px;display:flex;align-items:center;gap:4px;font-size:13px;white-space:nowrap}.tn i.cv{width:14px;height:14px;display:inline-block;opacity:.6}.tn i.ic{width:18px;height:18px}'
T={}
# 6. 윈도우 11 미카 (윈도우 탐색기와 같은 구성)
T["6_윈도우11"]=dict(css=TREECSS+'''
body{background:#eef1f6;color:#1b1b1b}.tabs{height:40px;display:flex;align-items:flex-end;padding:0 10px;gap:4px;background:#e3e8f0}
.tabb{background:#f9fafc;border-radius:8px 8px 0 0;padding:9px 16px;font-size:12px;display:flex;gap:8px;align-items:center}.tabb i{width:16px;height:16px;display:inline-block}
.bar{background:#f9fafc;height:48px;display:flex;align-items:center;gap:8px;padding:0 12px;border-bottom:1px solid #e5e7eb}
.nav i{width:16px;height:16px;display:inline-block;color:#555}.addr{flex:1;height:32px;border:1px solid #e5e7eb;border-radius:6px;background:#fff;display:flex;align-items:center;padding:0 10px;font-size:13px;gap:6px;color:#444}
.srch{width:220px;height:32px;border:1px solid #e5e7eb;border-radius:6px;background:#fff;font-size:12px;color:#999;display:flex;align-items:center;padding:0 10px}
.cmd{background:#f9fafc;height:46px;display:flex;align-items:center;gap:4px;padding:0 12px;border-bottom:1px solid #e5e7eb;font-size:13px}
.cb{padding:7px 12px;border-radius:6px}.cb.p{background:#005fb8;color:#fff}.sep{width:1px;height:24px;background:#e5e7eb;margin:0 6px}
.body{display:flex;height:494px;background:#f9fafc}.tree{width:240px;border-right:1px solid #e5e7eb;padding:8px 6px}.tn.on{background:#dbe8f6;border-radius:4px}
.list{flex:1;padding:4px 12px}.hd{color:#666;font-size:12px;height:32px;padding:0 10px;border-bottom:1px solid #eee}
.row{height:30px;padding:0 10px;font-size:13px}.row .r,.row .dt{color:#666}.row.sel{background:#cce4f7;border-radius:4px}.row i.ic{width:18px;height:18px}
.st{height:28px;background:#f9fafc;border-top:1px solid #e5e7eb;font-size:12px;color:#555;display:flex;align-items:center;padding:0 14px;gap:16px}
''',body=lambda:f'''<div class="tabs"><div class="tabb"><i>{ZIPI}</i>제안서_자료.zip — 차곡</div></div>
<div class="bar"><span class="b nav"><i>{ICO["back"]}</i></span><div class="addr">{PC[:0]}이 PC › 문서 › <b>제안서_자료.zip</b> › 제안서_자료</div><div class="srch">제안서_자료 검색</div><span class="b nav"><i>{ICO["theme"]}</i></span></div>
<div class="cmd"><span class="b cb p"><i>{ICO["zip"]}</i>새로 압축</span><span class="b cb"><i>{ICO["split"]}</i>분할 압축</span><span class="b cb"><i>{ICO["ext"]}</i>압축 해제</span><span class="sep"></span><span class="b cb"><i>{ICO["plus"]}</i>파일 추가</span><span class="b cb"><i>{ICO["down"]}</i>선택 추출</span></div>
<div class="body"><div class="tree">{tree(("#ffd36b","#e0a800"))}</div><div class="list"><div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>{rows(("#ffd36b","#e0a800"),("#fff","#8a8a8a"))}</div></div>
<div class="st"><span>항목 8개</span><span>1개 항목 선택함 2.4 MB</span><span style="margin-left:auto">ZIP · 압축률 22%</span></div>''')
# 7. 포레스트 (짙은 녹색 다크, 촘촘)
T["7_포레스트"]=dict(css=TREECSS+'''
body{background:#0f1a15;color:#d7e4dc}.top{height:52px;display:flex;align-items:center;gap:6px;padding:0 14px;background:#13221b;border-bottom:1px solid #22382d}
.logo{font-weight:800;color:#86efac;margin-right:14px;font-size:16px}.tb{padding:8px 12px;border-radius:8px;font-size:13px;color:#b9cfc2}.tb.on{background:#1f3a2c;color:#bbf7d0}
.sp{flex:1}.ac{background:#22c55e;color:#052e16;font-weight:700}.body{display:flex;height:616px}
.tree{width:250px;background:#111d17;border-right:1px solid #22382d;padding:10px 6px}.tt{font-size:11px;color:#6b8f7a;padding:6px 12px;letter-spacing:1px}
.tn{color:#b9cfc2}.tn.on{background:#1f3a2c;color:#ecfdf5;border-left:3px solid #22c55e}
.main{flex:1;display:flex;flex-direction:column}.path{padding:12px 18px;font-size:13px;color:#7fa391}.path b{color:#ecfdf5}
.hd{color:#6b8f7a;font-size:12px;padding:6px 18px;border-bottom:1px solid #22382d}.row{height:34px;padding:0 18px;font-size:13px}.row .r,.row .dt{color:#7fa391}
.row.sel{background:#166534;color:#fff}.row.sel .r,.row.sel .dt{color:#bbf7d0}
.st{margin-top:auto;height:30px;border-top:1px solid #22382d;display:flex;align-items:center;padding:0 18px;font-size:12px;color:#6b8f7a;gap:14px}
.pg{flex:1;height:6px;background:#22382d;border-radius:3px}.pg div{width:62%;height:6px;background:#22c55e;border-radius:3px}
''',body=lambda:f'''<div class="top"><span class="logo">차곡</span><span class="b tb on"><i>{ICO["zip"]}</i>압축</span><span class="b tb"><i>{ICO["split"]}</i>분할 압축</span><span class="b tb"><i>{ICO["ext"]}</i>압축 해제</span><span class="sp"></span><span class="b tb"><i>{ICO["plus"]}</i>추가</span><span class="b tb ac"><i>{ICO["down"]}</i>추출</span><span class="b tb"><i>{ICO["theme"]}</i></span></div>
<div class="body"><div class="tree"><div class="tt">압축 파일</div>{tree(("#4ade80","#16a34a"))}<div class="tt" style="margin-top:14px">최근</div>{tree(("#4ade80","#16a34a"),[(0,"zip","현장_사진_1027.7z",2,0),(0,"zip","도면_백업.tar.gz",2,0),(0,"zip","견적_v3.zip",2,0)])}</div>
<div class="main"><div class="path">제안서_자료.zip / <b>제안서_자료</b></div><div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>{rows(("#4ade80","#16a34a"),("#13221b","#7fa391"))}
<div class="st"><span>압축 중 · 현장_004.jpg</span><div class="pg"><div></div></div><span>62%</span></div></div></div>''')
# 8. 노르딕 (흰색·슬레이트, 여백 넓게)
T["8_노르딕"]=dict(css=TREECSS+'''
body{background:#fff;color:#1e293b}.side{position:absolute;left:0;top:0;bottom:0;width:260px;background:#f8fafc;border-right:1px solid #e2e8f0;padding:22px 14px}
.brand{font-size:20px;font-weight:800;color:#334155;padding:0 10px 18px;display:flex;gap:10px;align-items:center}.brand i{width:28px;height:28px;display:inline-block}
.act{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px;margin-bottom:18px}.act span{display:flex;flex-direction:column;align-items:center;gap:6px;font-size:11px;color:#475569;padding:10px 0;border:1px solid #e2e8f0;border-radius:10px;background:#fff}.act i{width:20px;height:20px}
.act span.on{border-color:#475569;color:#0f172a}.tt{font-size:11px;color:#94a3b8;padding:6px 10px;letter-spacing:.5px}
.tn{color:#334155}.tn.on{background:#e2e8f0;border-radius:8px;font-weight:700}
.main{position:absolute;left:260px;right:0;top:0;bottom:0;padding:26px 34px;display:flex;flex-direction:column}
.cmd{display:flex;align-items:center;gap:10px}.cmd h2{flex:1;font-size:24px;font-weight:700}.cmd small{display:block;font-size:13px;color:#94a3b8;font-weight:400;margin-top:4px}
.ob{height:36px;padding:0 16px;border:1px solid #cbd5e1;border-radius:8px;font-size:13px}.ob.ac{background:#334155;color:#fff;border-color:#334155}
.hd{color:#94a3b8;font-size:12px;padding:18px 10px 8px;border-bottom:1px solid #f1f5f9}.row{height:42px;padding:0 10px;font-size:14px;border-bottom:1px solid #f1f5f9}.row .r,.row .dt{color:#94a3b8}
.row.sel{background:#f1f5f9;box-shadow:inset 3px 0 #334155}
.st{margin-top:auto;font-size:12px;color:#94a3b8;display:flex;justify-content:space-between}
''',body=lambda:f'''<div class="side"><div class="brand"><i>{ZIPI}</i>차곡</div><div class="act"><span class="on"><i>{ICO["zip"]}</i>압축</span><span><i>{ICO["split"]}</i>분할</span><span><i>{ICO["ext"]}</i>풀기</span></div>
<div class="tt">이 압축 파일</div>{tree(("#cbd5e1","#64748b"))}<div class="tt" style="margin-top:14px">내 PC</div>{tree(("#cbd5e1","#64748b"),[(0,"drv","로컬 디스크 (C:)",0,0),(0,"drv","작업 (D:)",0,0),(0,"dir","바탕 화면",0,0),(0,"dir","다운로드",0,0)])}</div>
<div class="main"><div class="cmd"><h2>제안서_자료<small>제안서_자료.zip · 항목 33개 · 31.6 MB → 24.8 MB</small></h2><span class="b ob"><i>{ICO["plus"]}</i>파일 추가</span><span class="b ob ac"><i>{ICO["down"]}</i>선택 추출</span><span class="b ob"><i>{ICO["theme"]}</i></span></div>
<div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>{rows(("#cbd5e1","#64748b"),("#fff","#94a3b8"))}<div class="st"><span>1개 선택 · 2.4 MB</span><span>ZIP · 압축률 22%</span></div></div>''')
# 9. 선셋 (따뜻한 그라데이션 헤더)
T["9_선셋"]=dict(css=TREECSS+'''
body{background:#fff7f2;color:#3b2620}.hdr{height:120px;background:linear-gradient(100deg,#fb7185,#fb923c 55%,#fbbf24);color:#fff;padding:18px 24px;position:relative}
.hdr h1{font-size:22px}.hdr p{font-size:13px;opacity:.9;margin-top:4px}.acts{position:absolute;left:24px;bottom:-22px;display:flex;gap:10px}
.ab{height:44px;padding:0 18px;border-radius:22px;background:#fff;color:#c2410c;font-size:14px;font-weight:700;box-shadow:0 6px 18px #fb923c44}.ab.on{background:#7c2d12;color:#fff}
.rt{position:absolute;right:24px;top:18px;display:flex;gap:8px}.rb{height:34px;padding:0 14px;border-radius:17px;background:#ffffff33;font-size:13px}
.body{display:flex;margin-top:38px;height:532px;padding:0 18px 18px;gap:16px}.tree{width:240px;background:#fff;border-radius:16px;padding:12px 6px;box-shadow:0 4px 16px #fb923c1f}
.tn{color:#5b3a30}.tn.on{background:#ffedd5;border-radius:8px;color:#9a3412;font-weight:700}
.list{flex:1;background:#fff;border-radius:16px;padding:10px 12px;box-shadow:0 4px 16px #fb923c1f}.hd{color:#b08a7e;font-size:12px;padding:6px 12px}
.row{height:40px;padding:0 12px;font-size:14px;border-radius:10px}.row .r,.row .dt{color:#a07d72}.row.sel{background:#fff1e6;outline:1px solid #fdba74}
''',body=lambda:f'''<div class="hdr"><h1>제안서_자료.zip</h1><p>항목 33개 · 31.6 MB → 24.8 MB · 압축률 22%</p>
<div class="rt"><span class="b rb"><i>{ICO["plus"]}</i>파일 추가</span><span class="b rb"><i>{ICO["down"]}</i>선택 추출</span><span class="b rb"><i>{ICO["theme"]}</i></span></div>
<div class="acts"><span class="b ab on"><i>{ICO["zip"]}</i>새로 압축</span><span class="b ab"><i>{ICO["split"]}</i>분할 압축</span><span class="b ab"><i>{ICO["ext"]}</i>압축 해제</span></div></div>
<div class="body"><div class="tree">{tree(("#fdba74","#ea580c"))}</div><div class="list"><div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>{rows(("#fdba74","#ea580c"),("#fff7f2","#c49a8c"))}</div></div>''')
# 10. 듀얼 패널 (왼쪽 내 PC 탐색기 ↔ 오른쪽 압축 파일)
LOCAL=[("dir","제안서_자료","","","2026-10-27"),("dir","견적_원본","","","2026-10-26"),("file","제안서_자료.zip","","24.8 MB","2026-10-28"),("file","현장_사진.7z","","88.1 MB","2026-10-27"),("file","메모.txt","","2 KB","2026-10-28")]
def lrows():
    o=[]
    for i,(k,n,s,c,d) in enumerate(LOCAL):
        ic=folder("#ffd36b","#e0a800") if k=="dir" else (ZIPI if n.endswith(("zip","7z")) else fileico("#fff","#8a94a6"))
        o.append(f'<div class="lr{" sel" if i==2 else ""}"><i class="ic">{ic}</i><span>{n}</span><span class="r">{c}</span></div>')
    return "".join(o)
T["10_듀얼패널"]=dict(css=TREECSS+'''
body{background:#e9edf3;color:#111827}.top{height:44px;background:#111827;color:#e5e7eb;display:flex;align-items:center;gap:6px;padding:0 12px;font-size:13px}
.top b{color:#fff;margin-right:12px}.tb{padding:6px 12px;border-radius:6px;background:#1f2937}.tb.on{background:#2563eb;color:#fff}.sp{flex:1}
.panes{display:flex;gap:8px;padding:8px;height:620px}.pane{background:#fff;border:1px solid #d1d5db;border-radius:8px;display:flex;flex-direction:column;overflow:hidden}
.ph{height:36px;background:#f3f4f6;border-bottom:1px solid #e5e7eb;display:flex;align-items:center;padding:0 12px;font-size:12px;color:#374151;gap:6px}.ph b{color:#111827}
.left{width:430px}.lt{display:flex;flex:1}.tree{width:180px;border-right:1px solid #eee;padding:6px 2px;font-size:12px}.tn.on{background:#dbeafe;border-radius:4px}
.ll{flex:1;padding:6px}.lr{display:grid;grid-template-columns:20px 1fr 70px;gap:8px;align-items:center;height:30px;padding:0 8px;font-size:13px}.lr .r{text-align:right;color:#6b7280;font-size:12px}.lr.sel{background:#dbeafe;border-radius:4px}
.mid{display:flex;flex-direction:column;justify-content:center;gap:10px}.arrow{width:44px;height:44px;border-radius:22px;background:#2563eb;color:#fff;display:flex;align-items:center;justify-content:center;font-size:20px}.arrow.g{background:#16a34a}
.right{flex:1}.hd{color:#6b7280;font-size:12px;padding:6px 12px;border-bottom:1px solid #eee}.row{height:32px;padding:0 12px;font-size:13px}.row .r,.row .dt{color:#6b7280}.row.sel{background:#dbeafe}
.st{height:26px;background:#111827;color:#9ca3af;font-size:12px;display:flex;align-items:center;padding:0 12px;justify-content:space-between}
''',body=lambda:f'''<div class="top"><b>차곡</b><span class="b tb on"><i>{ICO["zip"]}</i>압축</span><span class="b tb"><i>{ICO["split"]}</i>분할 압축</span><span class="b tb"><i>{ICO["ext"]}</i>압축 해제</span><span class="sp"></span><span class="b tb"><i>{ICO["theme"]}</i></span></div>
<div class="panes"><div class="pane left"><div class="ph"><i class="ic" style="width:16px;height:16px">{PC}</i>내 PC › 문서 › <b>제안</b></div><div class="lt"><div class="tree">{tree(("#ffd36b","#e0a800"),[(0,"pc","내 PC",1,0),(1,"drv","C:",1,0),(2,"dir","문서",1,0),(3,"dir","제안",0,1),(2,"dir","바탕 화면",0,0),(1,"drv","D:",0,0)])}</div><div class="ll">{lrows()}</div></div></div>
<div class="mid"><div class="arrow" title="추가">→</div><div class="arrow g" title="추출">←</div></div>
<div class="pane right"><div class="ph"><i class="ic" style="width:16px;height:16px">{ZIPI}</i><b>제안서_자료.zip</b> › 제안서_자료</div><div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>{rows(("#ffd36b","#e0a800"),("#fff","#8a94a6"))}</div></div>
<div class="st"><span>왼쪽 파일을 → 로 압축 파일에 추가 · 오른쪽 항목을 ← 로 왼쪽 폴더에 추출</span><span>ZIP · 31.6 MB → 24.8 MB</span></div>''')
async def main():
    async with async_playwright() as p:
        b=await p.chromium.launch(); pg=await b.new_page(viewport={"width":1100,"height":700})
        for k,v in T.items():
            await pg.set_content(BASE.replace("%CSS%",v["css"]).replace("%BODY%",v["body"]()))
            await pg.screenshot(path=f"{k}.png")
        await b.close()
if __name__ == "__main__": asyncio.run(main())

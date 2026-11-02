# 테마 시안 11~15 (왼쪽 탐색기 포함)
import asyncio
from gen import ROWS, SEL, ICO, folder, fileico, rows, BASE
from gen2 import tree, TREECSS, ZIPI, PC, DRV
from playwright.async_api import async_playwright
H='<div class="row hd"><span>이름</span><span class="r">크기</span><span class="r">압축 후</span><span class="dt">수정한 날짜</span></div>'
PCTREE=[(0,"drv","로컬 디스크 (C:)",0,0),(0,"drv","작업 (D:)",0,0),(0,"dir","바탕 화면",0,0),(0,"dir","다운로드",0,0)]
T={}
# 11. 오로라 글래스
T["11_오로라글래스"]=dict(css=TREECSS+'''
body{background:radial-gradient(600px 400px at 10% 10%,#a5f3fc,transparent),radial-gradient(700px 500px at 90% 30%,#c4b5fd,transparent),radial-gradient(600px 400px at 50% 100%,#fbcfe8,transparent),#eef2ff;color:#1e1b4b}
.win{position:absolute;inset:18px;border-radius:22px;background:#ffffff8c;border:1px solid #ffffffcc;box-shadow:0 20px 60px #6366f133;display:flex;overflow:hidden}
.side{width:250px;background:#ffffff66;border-right:1px solid #ffffffaa;padding:20px 12px}.brand{font-size:18px;font-weight:800;padding:0 8px 16px;color:#4338ca}
.nb{height:40px;border-radius:12px;display:flex;align-items:center;gap:10px;padding:0 12px;font-size:14px}.nb i{width:18px;height:18px;display:inline-block}.nb.on{background:#fff;box-shadow:0 4px 14px #6366f126;color:#4338ca;font-weight:700}
.tt{font-size:11px;color:#6b7280;padding:14px 10px 4px}.tn.on{background:#ffffffcc;border-radius:8px}
.main{flex:1;padding:20px 24px;display:flex;flex-direction:column}.cmd{display:flex;align-items:center;gap:10px}.crumb{flex:1;font-size:15px;color:#6366f1}.crumb b{color:#1e1b4b}
.ob{height:36px;padding:0 14px;border-radius:12px;background:#ffffffb3;font-size:13px}.ob.ac{background:#4f46e5;color:#fff}
.hd{color:#6b7280;font-size:12px;padding:14px 12px 6px}.row{height:40px;padding:0 12px;font-size:14px;border-radius:12px}.row .r,.row .dt{color:#6b7280}.row.sel{background:#fff;box-shadow:0 4px 14px #6366f126}
''',body=lambda:f'''<div class="win"><div class="side"><div class="brand">차곡</div><div class="nb on"><i>{ICO["zip"]}</i>새로 압축</div><div class="nb"><i>{ICO["split"]}</i>분할 압축</div><div class="nb"><i>{ICO["ext"]}</i>압축 해제</div>
<div class="tt">압축 파일</div>{tree(("#a5b4fc","#6366f1"))}</div><div class="main"><div class="cmd"><span class="crumb">제안서_자료.zip › <b>제안서_자료</b></span><span class="b ob"><i>{ICO["plus"]}</i>파일 추가</span><span class="b ob ac"><i>{ICO["down"]}</i>선택 추출</span><span class="b ob"><i>{ICO["theme"]}</i></span></div>
{H}{rows(("#c7d2fe","#6366f1"),("#ffffffcc","#818cf8"))}</div></div>''')
# 12. 브루탈 (굵은 검정 테두리 + 노랑)
T["12_브루탈"]=dict(css=TREECSS+'''
body{background:#fffbea;color:#000;font-weight:600}.top{height:64px;border-bottom:3px solid #000;display:flex;align-items:center;gap:12px;padding:0 18px;background:#ffde59}
.top h1{font-size:26px;font-weight:900;margin-right:12px}.bb{height:40px;padding:0 14px;border:3px solid #000;background:#fff;box-shadow:4px 4px 0 #000;font-size:14px}.bb.on{background:#ff6b6b}.sp{flex:1}.bb.k{background:#4ade80}
.body{display:flex;height:636px}.tree{width:250px;border-right:3px solid #000;padding:12px 8px;background:#fff}.tt{font-size:12px;font-weight:900;padding:6px 10px}.tn.on{background:#ffde59;border:2px solid #000}
.main{flex:1;padding:16px}.path{font-size:15px;margin-bottom:10px}.tbl{border:3px solid #000;background:#fff;box-shadow:6px 6px 0 #000}
.hd{background:#000;color:#fff;font-size:12px;height:32px;padding:0 12px}.row{height:38px;padding:0 12px;font-size:14px;border-bottom:2px solid #000}.row:last-child{border-bottom:none}.row.sel{background:#a5f3fc}
''',body=lambda:f'''<div class="top"><h1>차곡</h1><span class="b bb on"><i>{ICO["zip"]}</i>새로 압축</span><span class="b bb"><i>{ICO["split"]}</i>분할 압축</span><span class="b bb"><i>{ICO["ext"]}</i>압축 해제</span><span class="sp"></span><span class="b bb"><i>{ICO["plus"]}</i>추가</span><span class="b bb k"><i>{ICO["down"]}</i>추출</span><span class="b bb"><i>{ICO["theme"]}</i></span></div>
<div class="body"><div class="tree"><div class="tt">압축 파일</div>{tree(("#ffde59","#000"))}<div class="tt" style="margin-top:12px">내 PC</div>{tree(("#ffde59","#000"),PCTREE)}</div>
<div class="main"><div class="path">제안서_자료.zip / 제안서_자료</div><div class="tbl">{H}{rows(("#ffde59","#000"),("#fff","#000"))}</div></div></div>''')
# 13. 코드 에디터 스타일 (그레이 다크 + 활동 막대)
T["13_에디터"]=dict(css=TREECSS+'''
body{background:#1e1e1e;color:#cccccc}.act{position:absolute;left:0;top:0;bottom:24px;width:48px;background:#2c2c2c;display:flex;flex-direction:column;align-items:center;padding-top:10px;gap:6px}
.act i{width:24px;height:24px;display:block;color:#858585;padding:10px;box-sizing:content-box}.act i.on{color:#fff;border-left:2px solid #0e7aca}
.side{position:absolute;left:48px;top:0;bottom:24px;width:240px;background:#252526}.st2{font-size:11px;letter-spacing:1px;color:#bbb;padding:12px 16px}.tn{color:#ccc}.tn.on{background:#37373d}
.tabs{position:absolute;left:288px;right:0;top:0;height:35px;background:#252526;display:flex}.tab{padding:0 16px;display:flex;align-items:center;gap:8px;font-size:13px;color:#969696;border-right:1px solid #1e1e1e}.tab.on{background:#1e1e1e;color:#fff;border-top:1px solid #0e7aca}.tab i{width:14px;height:14px;display:inline-block}
.main{position:absolute;left:288px;right:0;top:35px;bottom:24px;padding:6px 14px}.tool{display:flex;gap:8px;padding:8px 0}.tb{padding:5px 12px;background:#3a3d41;font-size:12px;border-radius:2px}.tb.p{background:#0e639c;color:#fff}
.hd{color:#858585;font-size:12px;height:28px;padding:0 10px;border-bottom:1px solid #333}.row{height:26px;padding:0 10px;font-size:13px}.row .r,.row .dt{color:#858585}.row.sel{background:#04395e;outline:1px solid #007fd4}.row i.ic{width:16px;height:16px}
.sb{position:absolute;left:0;right:0;bottom:0;height:24px;background:#007acc;color:#fff;font-size:12px;display:flex;align-items:center;gap:18px;padding:0 10px}
''',body=lambda:f'''<div class="act"><i class="on">{ICO["zip"]}</i><i>{ICO["split"]}</i><i>{ICO["ext"]}</i><i style="margin-top:auto">{ICO["theme"]}</i></div>
<div class="side"><div class="st2">탐색기</div>{tree(("#dcb67a","#c09553"))}<div class="st2">내 PC</div>{tree(("#dcb67a","#c09553"),PCTREE)}</div>
<div class="tabs"><div class="tab on"><i>{ZIPI}</i>제안서_자료.zip</div><div class="tab"><i>{ZIPI}</i>현장_사진.7z</div></div>
<div class="main"><div class="tool"><span class="b tb"><i>{ICO["plus"]}</i>파일 추가</span><span class="b tb p"><i>{ICO["down"]}</i>선택 추출</span></div>{H}{rows(("#dcb67a","#c09553"),("#1e1e1e","#858585"))}</div>
<div class="sb"><span>ZIP</span><span>항목 33개</span><span>31.6 MB → 24.8 MB</span><span style="margin-left:auto">UTF-8 · 압축률 22%</span></div>''')
# 14. 네이비 & 골드
T["14_네이비골드"]=dict(css=TREECSS+'''
body{background:#f7f5f0;color:#1c2541}.side{position:absolute;left:0;top:0;bottom:0;width:260px;background:#1c2541;color:#d6dbe8;padding:22px 14px}
.brand{font-size:22px;font-weight:800;color:#e9c46a;padding:0 8px 4px;letter-spacing:2px}.sub{font-size:11px;color:#8d99b5;padding:0 8px 18px}
.nb{height:42px;display:flex;align-items:center;gap:12px;padding:0 12px;font-size:14px;border-radius:6px}.nb i{width:18px;height:18px;display:inline-block}.nb.on{background:#e9c46a;color:#1c2541;font-weight:700}
.tt{font-size:11px;color:#8d99b5;padding:16px 10px 4px;letter-spacing:1px}.tn{color:#d6dbe8}.tn.on{background:#2e3a5f;border-radius:6px;color:#fff}
.main{position:absolute;left:260px;right:0;top:0;bottom:0;padding:24px 30px;display:flex;flex-direction:column}
.cmd{display:flex;align-items:center;gap:10px;border-bottom:2px solid #e9c46a;padding-bottom:14px}.cmd h2{flex:1;font-size:22px}.cmd small{display:block;font-size:12px;color:#7a8299;font-weight:400}
.ob{height:36px;padding:0 16px;border:1px solid #1c2541;font-size:13px;border-radius:4px}.ob.ac{background:#1c2541;color:#e9c46a}
.hd{color:#7a8299;font-size:12px;padding:12px 12px 6px}.row{height:40px;padding:0 12px;font-size:14px;border-bottom:1px solid #ebe6da}.row .r,.row .dt{color:#7a8299}.row.sel{background:#fff7e0}
''',body=lambda:f'''<div class="side"><div class="brand">차곡</div><div class="sub">사내 압축 도구</div><div class="nb on"><i>{ICO["zip"]}</i>새로 압축</div><div class="nb"><i>{ICO["split"]}</i>분할 압축</div><div class="nb"><i>{ICO["ext"]}</i>압축 해제</div>
<div class="tt">압축 파일</div>{tree(("#e9c46a","#b8902f"))}</div><div class="main"><div class="cmd"><h2>제안서_자료<small>제안서_자료.zip · 항목 33개 · 압축률 22%</small></h2><span class="b ob"><i>{ICO["plus"]}</i>파일 추가</span><span class="b ob ac"><i>{ICO["down"]}</i>선택 추출</span><span class="b ob"><i>{ICO["theme"]}</i></span></div>
{H}{rows(("#e9c46a","#b8902f"),("#fff","#7a8299"))}</div>''')
# 15. 민트 + 오른쪽 정보 패널
T["15_민트_정보패널"]=dict(css=TREECSS+'''
body{background:#f4fbf8;color:#12332b}.top{height:56px;display:flex;align-items:center;gap:8px;padding:0 18px;background:#fff;border-bottom:1px solid #d7ede5}
.logo{font-weight:800;color:#0f9f7a;font-size:18px;margin-right:16px}.tb{height:36px;padding:0 14px;border-radius:18px;font-size:13px;color:#2f5c50}.tb.on{background:#0f9f7a;color:#fff}.sp{flex:1}
.body{display:flex;height:644px}.tree{width:230px;background:#fff;border-right:1px solid #d7ede5;padding:12px 6px}.tt{font-size:11px;color:#6d9a8d;padding:6px 10px}.tn.on{background:#dff5ec;border-radius:8px;color:#0b6e55;font-weight:700}
.list{flex:1;padding:12px 16px}.path{font-size:13px;color:#6d9a8d;margin-bottom:8px}.path b{color:#12332b}.hd{color:#6d9a8d;font-size:12px;padding:6px 10px}
.row{height:36px;padding:0 10px;font-size:13px;border-radius:8px}.row .r,.row .dt{color:#6d9a8d}.row.sel{background:#dff5ec}
.info{width:240px;background:#fff;border-left:1px solid #d7ede5;padding:20px 18px;font-size:13px}.big{width:84px;height:84px;margin:10px auto 14px}.info h3{text-align:center;font-size:15px;margin-bottom:16px}
.kv{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px dashed #d7ede5;color:#2f5c50}.kv span:first-child{color:#6d9a8d}
.btn{margin-top:16px;height:40px;border-radius:10px;background:#0f9f7a;color:#fff;display:flex;align-items:center;justify-content:center;gap:8px;font-weight:700}.btn i{width:18px;height:18px;display:inline-block}
.btn.o{background:#fff;color:#0f9f7a;border:1px solid #0f9f7a;margin-top:8px}
''',body=lambda:f'''<div class="top"><span class="logo">차곡</span><span class="b tb on"><i>{ICO["zip"]}</i>새로 압축</span><span class="b tb"><i>{ICO["split"]}</i>분할 압축</span><span class="b tb"><i>{ICO["ext"]}</i>압축 해제</span><span class="sp"></span><span class="b tb"><i>{ICO["plus"]}</i>파일 추가</span><span class="b tb"><i>{ICO["theme"]}</i></span></div>
<div class="body"><div class="tree"><div class="tt">압축 파일</div>{tree(("#86e3c3","#0f9f7a"))}<div class="tt" style="margin-top:10px">내 PC</div>{tree(("#86e3c3","#0f9f7a"),PCTREE)}</div>
<div class="list"><div class="path">제안서_자료.zip › <b>제안서_자료</b></div>{H}{rows(("#86e3c3","#0f9f7a"),("#fff","#6d9a8d"))}</div>
<div class="info"><div class="big">{fileico("#effaf6","#0f9f7a")}</div><h3>제안서_최종.hwp</h3><div class="kv"><span>원본 크기</span><span>2.4 MB</span></div><div class="kv"><span>압축 후</span><span>1.9 MB</span></div><div class="kv"><span>압축률</span><span>21%</span></div><div class="kv"><span>수정한 날짜</span><span>2026-10-27</span></div><div class="kv"><span>암호</span><span>없음</span></div>
<div class="btn"><i>{ICO["down"]}</i>이 파일 추출</div><div class="btn o">바로 열기</div></div></div>''')
async def main():
    async with async_playwright() as p:
        b=await p.chromium.launch(); pg=await b.new_page(viewport={"width":1100,"height":700})
        for k,v in T.items():
            await pg.set_content(BASE.replace("%CSS%",v["css"]).replace("%BODY%",v["body"]()))
            await pg.screenshot(path=f"{k}.png")
        await b.close()
asyncio.run(main())

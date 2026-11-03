// main.cpp - 차곡 UI (Win32 + GDI/GDI+ 커스텀 드로잉)
// 테마 6종(기본·미드나잇·파스텔·노르딕·네이비골드·민트) × 색상: 시스템 / 라이트 / 다크
#include "archive.h"
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <bcrypt.h>
#include <dwmapi.h>
#include <wininet.h>
#include <wintrust.h>
#include <softpub.h>
#include <algorithm>
#include <map>
#include <cmath>
#include <cwchar>
using std::max;
using std::min;
#include <gdiplus.h>
#include "resource.h"

using namespace Gdiplus;

// 프로그램 이름 (창 제목·우클릭 메뉴에 사용). 이름이 정해지면 여기만 바꾸면 된다.
#define APP_NAME L"차곡"

// ============================================================ 설정/테마
#include "themes.h"
// 테마 = 화면 배치 + 색상표. 배치는 TH_BASIC~TH_MINT 6가지와 고유 배치 TH_CHAGOK~TH_BYEOL 5가지.
// 배치 비교는 반드시 g_ly 로 한다(g_theme 아님)
enum { TH_BASIC, TH_MIDNIGHT, TH_PASTEL, TH_NORDIC, TH_NAVY, TH_MINT,
       TH_FOREST, TH_CORAL, TH_GRAPHITE, TH_OCEAN, TH_BURGUNDY, TH_SAND,
       TH_CHAGOK, TH_SHIM, TH_BOM, TH_MUL, TH_BYEOL, TH_COUNT };
static const wchar_t* TH_NAMES[TH_COUNT] = {L"기본", L"미드나잇", L"파스텔", L"노르딕", L"네이비골드", L"민트",
                                            L"포레스트", L"선셋 코랄", L"그라파이트", L"오션", L"버건디", L"샌드",
                                            L"차곡", L"쉼", L"봄빛", L"물결", L"별밤"};
static const int TH_LAYOUT[TH_COUNT] = {TH_BASIC, TH_MIDNIGHT, TH_PASTEL, TH_NORDIC, TH_NAVY, TH_MINT,
                                        TH_NORDIC, TH_BASIC, TH_MIDNIGHT, TH_PASTEL, TH_NAVY, TH_MINT,
                                        TH_CHAGOK, TH_SHIM, TH_BOM, TH_MUL, TH_BYEOL};
static const Palette* PALS[TH_COUNT][2] = {{&PAL_BASIC_L, &PAL_BASIC_D}, {&PAL_MID_L, &PAL_MID_D}, {&PAL_PAS_L, &PAL_PAS_D},
                                           {&PAL_NOR_L, &PAL_NOR_D}, {&PAL_NAVY_L, &PAL_NAVY_D}, {&PAL_MINT_L, &PAL_MINT_D},
                                           {&PAL_FOR_L, &PAL_FOR_D}, {&PAL_COR_L, &PAL_COR_D}, {&PAL_GRA_L, &PAL_GRA_D},
                                           {&PAL_OCE_L, &PAL_OCE_D}, {&PAL_BUR_L, &PAL_BUR_D}, {&PAL_SAND_L, &PAL_SAND_D},
                                           {&PAL_CHA_L, &PAL_CHA_D}, {&PAL_SHIM_L, &PAL_SHIM_D}, {&PAL_BOM_L, &PAL_BOM_D},
                                           {&PAL_MUL_L, &PAL_MUL_D}, {&PAL_BYEOL_L, &PAL_BYEOL_D}};
enum { MODE_SYSTEM = 0, MODE_LIGHT = 1, MODE_DARK = 2 };
static const COLORREF NONE = 0xFFFFFFFF;

static int g_theme = TH_BASIC, g_mode = MODE_SYSTEM;
static int g_ly = TH_BASIC;  // 현재 테마의 화면 배치 (TH_LAYOUT[g_theme])
static int g_view = 0;          // 기본 테마: 0 큰 아이콘, 1 자세히
static int g_opacity = 100;     // 창 불투명도 % (40~100)
static int g_iconSel = 1;       // 프로그램 아이콘: 0 가지런한 더미 1 갠 이불(기본) 2 정리함
static HICON g_iconBig, g_iconSmall;
static const wchar_t* ICON_NAMES[3] = {L"가지런한 더미", L"갠 이불", L"정리함"};
static int IconResId(int sel) { return sel == 0 ? IDI_APP_A : sel == 2 ? IDI_APP_C : IDI_APP; }
static int IconIndex(int sel) { return sel == 0 ? 1 : sel == 2 ? 2 : 0; }  // 실행 파일 안 아이콘 순서
static void LoadAppIcons() {
    HINSTANCE hi = GetModuleHandleW(nullptr);
    // 이전 아이콘은 다른 창 클래스가 아직 쓰고 있을 수 있어 지우지 않는다 (바꿀 때만 몇 KB)
    g_iconBig = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IconResId(g_iconSel)), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    g_iconSmall = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IconResId(g_iconSel)), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
}
static void ApplyIcon(HWND h) {
    if (!h) return;
    SendMessageW(h, WM_SETICON, ICON_BIG, (LPARAM)g_iconBig);
    SendMessageW(h, WM_SETICON, ICON_SMALL, (LPARAM)g_iconSmall);
}
static int g_preset = 0;        // 0 빠름(ZIP) 1 보통(7z HC4) 2 최대(7z LZMA2)
static bool g_dark = false;
static Palette P = PAL_BASIC_L;
static uint64_t g_splitMB = 100;
static int g_dpi = 96;
static HWND g_hwnd;
static HFONT g_font, g_fontBold, g_fontSmall, g_fontTitle, g_fontBig, g_fontLogo, g_fontTiny;
static ULONG_PTR g_gdiplusToken;

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static float SF(float v) { return v * g_dpi / 96.0f; }

static const wchar_t* REG_KEY = L"Software\\Sinsul\\Chagok";
static DWORD RegGet(const wchar_t* name, DWORD def) {
    DWORD v = def, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, REG_KEY, name, RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v;
}
static void RegSet(const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(k);
    }
}
static bool SystemIsDark() {
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS) return false;
    return v == 0;
}
static COLORREF Mix(COLORREF a, COLORREF b, float t) {
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t), (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}
static COLORREF SideBg() { return P.side == NONE ? P.bg : P.side; }

static void MakeFonts() {
    HFONT* all[] = {&g_font, &g_fontBold, &g_fontSmall, &g_fontTitle, &g_fontBig, &g_fontLogo, &g_fontTiny};
    for (auto f : all) if (*f) { DeleteObject(*f); *f = nullptr; }
    int body = g_ly == TH_BASIC ? 13 : 14;
    auto mk = [](int px, int w) {
        return CreateFontW(-S(px), 0, 0, 0, w, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    };
    g_font = mk(body, FW_NORMAL);
    g_fontBold = mk(body, FW_SEMIBOLD);
    g_fontSmall = mk(12, FW_NORMAL);
    g_fontTitle = mk(15, FW_BOLD);
    g_fontBig = mk(22, FW_BOLD);
    g_fontLogo = mk(20, FW_BOLD);
    g_fontTiny = mk(11, FW_NORMAL);
}

static void ApplyTitleBar(HWND h) {
    BOOL dark = g_dark;
    bool band = g_ly == TH_BASIC || g_ly == TH_BOM;  // 위쪽 색 띠와 이어지는 제목 표시줄
    if (band) dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(h, 20, &dark, sizeof(dark)))) DwmSetWindowAttribute(h, 19, &dark, sizeof(dark));
    COLORREF cap = P.caption, txt = band ? P.onBand : P.text;
    DwmSetWindowAttribute(h, 35, &cap, sizeof(cap));  // Windows 11: 캡션 색
    DwmSetWindowAttribute(h, 36, &txt, sizeof(txt));
    DwmSetWindowAttribute(h, 34, &cap, sizeof(cap));  // 테두리 색
}
// 창 투명도 (설정 창의 슬라이더). 100% 이면 레이어드 창을 끈다(그리기 비용 없음)
static void ApplyOpacity(HWND h) {
    if (!h) return;
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    if (g_opacity >= 100) {
        if (ex & WS_EX_LAYERED) SetWindowLongW(h, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
    } else {
        if (!(ex & WS_EX_LAYERED)) SetWindowLongW(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        SetLayeredWindowAttributes(h, 0, (BYTE)(g_opacity * 255 / 100), LWA_ALPHA);
    }
}

static void ApplyTheme(bool save) {
    g_dark = g_mode == MODE_DARK || (g_mode == MODE_SYSTEM && SystemIsDark());
    g_ly = TH_LAYOUT[g_theme];
    P = *PALS[g_theme][g_dark ? 1 : 0];
    MakeFonts();
    if (save) { RegSet(L"Theme", g_theme); RegSet(L"ColorMode", g_mode); }
    if (g_hwnd) {
        ApplyTitleBar(g_hwnd);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

// ============================================================ 그리기 도우미
static Color GC(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static void RoundPath(GraphicsPath& p, RectF r, float rad) {
    if (rad <= 0.5f) { p.AddRectangle(r); return; }
    rad = min(rad, min(r.Width, r.Height) / 2);
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}
static RectF RF(RECT r) { return RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top)); }
static void FillR(Graphics& g, RECT r, float rad, COLORREF c) {
    if (c == NONE || r.right <= r.left || r.bottom <= r.top) return;
    SolidBrush b(GC(c));
    if (rad <= 0) { g.FillRectangle(&b, RF(r)); return; }
    GraphicsPath p;
    RoundPath(p, RF(r), rad);
    g.FillPath(&b, &p);
}
// 반투명 채우기 (alpha 0~255)
static void FillRA(Graphics& g, RECT r, float rad, COLORREF c, BYTE alpha) {
    if (c == NONE || r.right <= r.left || r.bottom <= r.top) return;
    SolidBrush b(GC(c, alpha));
    GraphicsPath p;
    RoundPath(p, RF(r), rad);
    g.FillPath(&b, &p);
}
// 그라데이션 채우기 (c2 가 NONE 이면 단색). angle: 0 가로, 90 세로, 135 대각
static void FillG(Graphics& g, RECT r, float rad, COLORREF c1, COLORREF c2, float angle = 0) {
    if (c2 == NONE) { FillR(g, r, rad, c1); return; }
    if (r.right <= r.left || r.bottom <= r.top) return;
    RectF rf = RF(r);
    RectF big(rf.X - 1, rf.Y - 1, rf.Width + 2, rf.Height + 2);
    LinearGradientBrush b(big, GC(c1), GC(c2), angle, FALSE);
    GraphicsPath p;
    RoundPath(p, rf, rad);
    g.FillPath(&b, &p);
}
// 세 가지 색 그라데이션 (봄빛 머리 띠)
static void FillG3(Graphics& g, RECT r, float rad, COLORREF c1, COLORREF c2, COLORREF c3, float angle = 0) {
    if (r.right <= r.left || r.bottom <= r.top) return;
    RectF rf = RF(r);
    RectF big(rf.X - 1, rf.Y - 1, rf.Width + 2, rf.Height + 2);
    LinearGradientBrush b(big, GC(c1), GC(c3), angle, FALSE);
    Color cs[3] = {GC(c1), GC(c2), GC(c3)};
    REAL pos[3] = {0.0f, 0.55f, 1.0f};
    b.SetInterpolationColors(cs, pos, 3);
    GraphicsPath p;
    RoundPath(p, rf, rad);
    g.FillPath(&b, &p);
}
static void StrokeR(Graphics& g, RECT r, float rad, COLORREF c, float w = 1, bool dashed = false) {
    if (c == NONE) return;
    Pen pen(GC(c), w);
    if (dashed) { REAL d[] = {4, 3}; pen.SetDashPattern(d, 2); }
    GraphicsPath p;
    float h = w / 2;
    RoundPath(p, RectF(r.left + h, r.top + h, (REAL)(r.right - r.left) - w, (REAL)(r.bottom - r.top) - w), rad);
    g.DrawPath(&pen, &p);
}
static void HLine(Graphics& g, int x1, int x2, int y, COLORREF c, int th = 1) {
    if (c == NONE) return;
    SolidBrush b(GC(c));
    g.FillRectangle(&b, (REAL)x1, (REAL)y, (REAL)(x2 - x1), (REAL)max(th, S(th)));
}
static void VLine(Graphics& g, int x, int y1, int y2, COLORREF c) {
    if (c == NONE) return;
    SolidBrush b(GC(c));
    g.FillRectangle(&b, (REAL)x, (REAL)y1, (REAL)max(1, S(1)), (REAL)(y2 - y1));
}
static void Txt(HDC dc, RECT r, const std::wstring& s, COLORREF c, HFONT f, UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), (int)s.size(), &r, fmt | DT_NOPREFIX | (fmt & DT_WORDBREAK ? 0 : DT_END_ELLIPSIS));
}
static HDC g_mdc;  // 측정용
static int TextW(const std::wstring& s, HFONT f) {
    SelectObject(g_mdc, f);
    SIZE sz{};
    GetTextExtentPoint32W(g_mdc, s.c_str(), (int)s.size(), &sz);
    return sz.cx;
}

enum Icon { IC_NONE, IC_ARCHIVE, IC_SPLIT, IC_EXTRACT, IC_PLUS, IC_DOWNLOAD, IC_UPLOAD, IC_BACK, IC_THEME,
            IC_CHEVDOWN, IC_CHECK, IC_FOLDER, IC_FILE, IC_OPEN, IC_GEAR, IC_GRID, IC_LIST, IC_CHEVRIGHT, IC_LOCK,
            IC_X, IC_SLIDERS, IC_SHIELD, IC_BOLT, IC_SCALE, IC_STACK, IC_EXTLINK, IC_SEARCH, IC_CHEVUP, IC_DRIVE };

static void DrawIco(Graphics& g, int ic, float x, float y, float size, COLORREF stroke, COLORREF fill = NONE, float sw = 1.8f) {
    float k = size / 24.0f;
    Pen pen(GC(stroke), max(1.0f, sw * k));
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    auto Pt = [&](float a, float b) { return PointF(x + a * k, y + b * k); };
    auto L = [&](float a, float b, float c, float d) { g.DrawLine(&pen, Pt(a, b), Pt(c, d)); };
    auto PL = [&](std::initializer_list<float> v, bool close = false, COLORREF f = NONE) {
        std::vector<PointF> pts;
        for (auto it = v.begin(); it != v.end(); it += 2) pts.push_back(Pt(*it, *(it + 1)));
        if (close) {
            if (f != NONE) { SolidBrush b(GC(f)); g.FillPolygon(&b, pts.data(), (INT)pts.size()); }
            g.DrawPolygon(&pen, pts.data(), (INT)pts.size());
        } else g.DrawLines(&pen, pts.data(), (INT)pts.size());
    };
    auto RR = [&](float a, float b, float w, float h, float r, COLORREF f = NONE) {
        GraphicsPath p;
        RoundPath(p, RectF(x + a * k, y + b * k, w * k, h * k), r * k);
        if (f != NONE) { SolidBrush br(GC(f)); g.FillPath(&br, &p); }
        g.DrawPath(&pen, &p);
    };
    switch (ic) {
    case IC_ARCHIVE: RR(4, 3, 16, 18, 2.5f, fill); L(12, 3, 12, 5.5f); L(12, 7.5f, 12, 9.5f); L(12, 11.5f, 12, 13); RR(10, 15, 4, 3.5f, 0.8f); break;
    case IC_SPLIT: RR(3, 4, 7, 16, 1.5f); RR(14, 4, 7, 16, 1.5f); L(6.5f, 8, 6.5f, 10); L(17.5f, 8, 17.5f, 10); break;
    case IC_EXTRACT: PL({4, 8, 4, 20, 20, 20, 20, 8}); PL({4, 8, 6, 4, 18, 4, 20, 8}); L(4, 8, 20, 8); L(12, 11, 12, 17); PL({9, 14, 12, 17, 15, 14}); break;
    case IC_PLUS: L(12, 5, 12, 19); L(5, 12, 19, 12); break;
    case IC_DOWNLOAD: L(12, 4, 12, 15); PL({7, 10, 12, 15, 17, 10}); L(5, 20, 19, 20); break;
    case IC_UPLOAD: L(12, 16, 12, 4); PL({7, 9, 12, 4, 17, 9}); L(5, 20, 19, 20); break;
    case IC_BACK: L(19, 12, 5, 12); PL({11, 6, 5, 12, 11, 18}); break;
    case IC_CHEVDOWN: PL({7, 10, 12, 15, 17, 10}); break;
    case IC_CHEVRIGHT: PL({10, 7, 15, 12, 10, 17}); break;
    case IC_CHECK: PL({5, 12.5f, 10, 17, 19, 7.5f}); break;
    case IC_X: L(6, 6, 18, 18); L(18, 6, 6, 18); break;
    case IC_SEARCH: g.DrawEllipse(&pen, x + 4 * k, y + 4 * k, 12 * k, 12 * k); L(14.5f, 14.5f, 20, 20); break;
    case IC_CHEVUP: PL({7, 14, 12, 9, 17, 14}); break;
    case IC_OPEN: PL({3, 19, 3, 5, 10, 5, 12, 7, 19, 7, 19, 10}); PL({3, 19, 6, 11, 22, 11, 19, 19}, true); break;
    case IC_EXTLINK: PL({13, 4, 20, 4, 20, 11}); L(20, 4, 11, 13); PL({17, 14, 17, 20, 4, 20, 4, 7, 10, 7}); break;
    case IC_THEME: {
        g.DrawEllipse(&pen, x + 4 * k, y + 4 * k, 16 * k, 16 * k);
        SolidBrush b(GC(stroke));
        g.FillPie(&b, x + 4 * k, y + 4 * k, 16 * k, 16 * k, 90, 180);
        break;
    }
    case IC_GEAR: {
        std::vector<PointF> pts;
        for (int i = 0; i < 8; i++) {
            float a0 = i * 45.0f;
            float as[4] = {a0 - 10, a0 + 10, a0 + 19, a0 + 26};
            float rs[4] = {9.5f, 9.5f, 7.0f, 7.0f};
            for (int q = 0; q < 4; q++) { float r = as[q] * 3.14159265f / 180; pts.push_back(Pt(12 + rs[q] * cosf(r), 12 + rs[q] * sinf(r))); }
        }
        g.DrawPolygon(&pen, pts.data(), (INT)pts.size());
        g.DrawEllipse(&pen, x + 9 * k, y + 9 * k, 6 * k, 6 * k);
        break;
    }
    case IC_GRID: RR(4, 4, 7, 7, 1.5f); RR(13, 4, 7, 7, 1.5f); RR(4, 13, 7, 7, 1.5f); RR(13, 13, 7, 7, 1.5f); break;
    case IC_LIST: L(9, 6, 20, 6); L(9, 12, 20, 12); L(9, 18, 20, 18); L(4, 6, 5, 6); L(4, 12, 5, 12); L(4, 18, 5, 18); break;
    case IC_LOCK: RR(5, 11, 14, 10, 2, fill); PL({8, 11, 8, 7.5f, 9.2f, 4.8f, 12, 3.8f, 14.8f, 4.8f, 16, 7.5f, 16, 11}); L(12, 15, 12, 17); break;
    case IC_SLIDERS: L(4, 7, 20, 7); L(4, 17, 20, 17); g.DrawEllipse(&pen, x + 12 * k, y + 4.5f * k, 5 * k, 5 * k); g.DrawEllipse(&pen, x + 7 * k, y + 14.5f * k, 5 * k, 5 * k); break;
    case IC_SHIELD: PL({12, 3, 20, 6, 20, 12, 16, 18.5f, 12, 21, 8, 18.5f, 4, 12, 4, 6}, true, fill); PL({8.5f, 12, 11, 14.5f, 15.5f, 9.5f}); break;
    case IC_BOLT: PL({13, 3, 5, 13.5f, 11.5f, 13.5f, 10.5f, 21, 19, 10.5f, 12.5f, 10.5f}, true, fill); break;
    case IC_SCALE: L(12, 4, 12, 20); L(5, 7, 19, 7); PL({5, 7, 2.5f, 13, 7.5f, 13}, true); PL({19, 7, 16.5f, 13, 21.5f, 13}, true); L(8, 20, 16, 20); break;
    case IC_STACK: PL({12, 3, 21, 8, 12, 13, 3, 8}, true, fill); PL({3, 12, 12, 17, 21, 12}); PL({3, 16, 12, 21, 21, 16}); break;
    case IC_FOLDER: PL({3, 5.5f, 9.5f, 5.5f, 11.5f, 7.5f, 21, 7.5f, 21, 19, 3, 19}, true, fill); break;
    case IC_FILE: PL({6, 3, 14, 3, 19, 8, 19, 21, 6, 21}, true, fill); L(14, 3, 14, 8); L(14, 8, 19, 8); break;
    case IC_DRIVE: RR(3, 6, 18, 12, 2.5f, fill); L(6.5f, 14, 11, 14); L(16, 14, 16.6f, 14); break;
    }
}

// ============================================================ 압축 파일 모델
struct DirNode {
    std::wstring name, path;  // path: "a/b" (루트는 "")
    int parent = -1, entry = -1;
    std::vector<int> dirs, files;
};
struct Row {
    int kind;  // 1 폴더, 2 파일
    int idx;
    std::wstring name;
    uint64_t us = 0, cs = 0;
    uint16_t d = 0, t = 0;
    bool hasDate = false;
};

static AnyArchive g_arc;
static bool g_hasArc = false;
static std::vector<DirNode> g_dirs;
static std::vector<char> g_expand;     // 폴더 트리 펼침 상태
static int g_cur = 0, g_treeScroll = 0, g_hoverTree = -1;
static std::vector<Row> g_rows;
static std::vector<char> g_sel;
static int g_focus = -1, g_anchor = -1, g_scrollY = 0, g_hoverRow = -1;
static uint64_t g_totU = 0, g_totC = 0;
static size_t g_fileCount = 0;

static void BuildTree() {
    g_dirs.clear();
    g_dirs.push_back(DirNode());
    std::map<std::wstring, int> idx;
    idx[L""] = 0;
    std::function<int(const std::wstring&)> ensure = [&](const std::wstring& p) -> int {
        auto it = idx.find(p);
        if (it != idx.end()) return it->second;
        size_t s = p.find_last_of(L'/');
        int par = ensure(s == std::wstring::npos ? L"" : p.substr(0, s));
        DirNode n;
        n.name = s == std::wstring::npos ? p : p.substr(s + 1);
        n.path = p;
        n.parent = par;
        int id = (int)g_dirs.size();
        g_dirs.push_back(n);
        g_dirs[(size_t)par].dirs.push_back(id);
        idx[p] = id;
        return id;
    };
    g_totU = g_totC = 0;
    g_fileCount = 0;
    for (size_t i = 0; i < g_arc.entries.size(); i++) {
        auto& e = g_arc.entries[i];
        if (e.isDir) { g_dirs[(size_t)ensure(e.name)].entry = (int)i; continue; }
        size_t s = e.name.find_last_of(L'/');
        int par = ensure(s == std::wstring::npos ? L"" : e.name.substr(0, s));
        g_dirs[(size_t)par].files.push_back((int)i);
        g_totU += e.usize; if (e.csize != UINT64_MAX) g_totC += e.csize; g_fileCount++;
    }
    for (auto& d : g_dirs)
        std::sort(d.dirs.begin(), d.dirs.end(), [](int a, int b) { return StrCmpLogicalW(g_dirs[(size_t)a].name.c_str(), g_dirs[(size_t)b].name.c_str()) < 0; });
    g_expand.assign(g_dirs.size(), 0);
    g_expand[0] = 1;
    g_treeScroll = 0;
    if (!g_arc.isZip) {  // 7z(솔리드)·rar 등은 항목별 압축 크기가 없을 수 있어 파일 크기 합계로
        g_totC = 0;
        for (auto& v : g_arc.volumes) {
            WIN32_FILE_ATTRIBUTE_DATA d;
            if (GetFileAttributesExW(LongPath(v).c_str(), GetFileExInfoStandard, &d)) g_totC += ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
        }
    }
}

// ============================================================ 내 PC 둘러보기 (탐색기)
// 압축 파일이 열려 있지 않을 때 g_fs 이면 폴더 내용을 목록에 보여 준다. 행(Row)의 idx 는 g_fsItems 번호.
struct FsItem { std::wstring name, path, kind; bool dir = false, arc = false, drive = false; uint64_t size = 0; uint16_t d = 0, t = 0; bool hasDate = false; };
struct Place { std::wstring name, path; int icon; };
static bool g_fs = false;
static std::wstring g_fsPath;                 // 지금 폴더 ("" = 내 PC)
static std::vector<FsItem> g_fsItems;
static std::vector<Place> g_places;           // 왼쪽 목록: 내 PC, 바탕 화면, 문서, 다운로드, 사진, 드라이브
static std::vector<std::wstring> g_fsCrumbs;  // 경로 단추별 경로
static HANDLE g_fsWatch = INVALID_HANDLE_VALUE;
static bool HasList() { return g_hasArc || g_fs; }
static bool CanBack() { return g_fs || g_hasArc; }  // 압축 파일 맨 위에서 뒤로 → 그 압축 파일이 있는 폴더
static std::wstring FsNorm(std::wstring p) {
    if (p.size() == 2 && p[1] == L':') p += L'\\';
    while (p.size() > 3 && p.back() == L'\\') p.pop_back();
    return p;
}
static std::wstring FsParent(const std::wstring& p) {  // C:\a\b → C:\a, C:\a → C:\, C:\ → "" (내 PC)
    if (p.size() <= 3) return L"";
    size_t s = p.find_last_of(L'\\');
    if (s == std::wstring::npos || s < 2) return L"";
    return FsNorm(p.substr(0, s));
}
static std::wstring FsJoin(const std::wstring& dir, const std::wstring& name) { return dir + (!dir.empty() && dir.back() == L'\\' ? L"" : L"\\") + name; }
static std::wstring FsLabel(const std::wstring& p) {
    if (p.empty()) return L"내 PC";
    if (p.size() <= 3) {
        SHFILEINFOW fi{};
        if (SHGetFileInfoW(p.c_str(), 0, &fi, sizeof(fi), SHGFI_DISPLAYNAME) && fi.szDisplayName[0]) return fi.szDisplayName;
        return p.substr(0, 2);
    }
    for (auto& pl : g_places) if (!pl.path.empty() && !lstrcmpiW(pl.path.c_str(), p.c_str())) return pl.name;
    size_t s = p.find_last_of(L'\\');
    return s == std::wstring::npos ? p : p.substr(s + 1);
}
static std::wstring FsKind(const std::wstring& name, bool dir) {
    if (dir) return L"폴더";
    size_t d = name.find_last_of(L'.');
    std::wstring e = d == std::wstring::npos || d == 0 ? L"" : name.substr(d + 1);
    if (!e.empty()) CharUpperBuffW(&e[0], (DWORD)e.size());
    if (fmt::IsArchivePath(name)) return L"압축 파일";
    return e.empty() ? L"파일" : e + L" 파일";
}
static void FsDos(const FILETIME& ft, uint16_t& d, uint16_t& t) {
    FILETIME lt; WORD dd = 0, tt = 0;
    FileTimeToLocalFileTime(&ft, &lt);
    if (FileTimeToDosDateTime(&lt, &dd, &tt)) { d = dd; t = tt; } else d = t = 0;
}
static const FsItem* FsAt(int row) { return g_fs && row >= 0 && row < (int)g_rows.size() ? &g_fsItems[(size_t)g_rows[(size_t)row].idx] : nullptr; }

// 정렬(열 머리글) / 검색(하위 폴더 전체)
static int g_sortCol = 0;          // 0 이름 1 크기 2 압축 후 3 날짜
static bool g_sortDesc = false;
static std::wstring g_search;      // 비어 있지 않으면 압축 파일 전체에서 이름 검색
static HWND g_searchEdit = nullptr;
static HFONT g_searchFont = nullptr;
static bool g_searchSilent = false;  // 코드에서 글자를 바꿀 때 검색 다시 하지 않기
static void SortRows() {
    std::stable_sort(g_rows.begin(), g_rows.end(), [](const Row& a, const Row& b) {
        if (a.kind != b.kind) return a.kind < b.kind;  // 폴더 먼저
        int c = 0;
        switch (g_sortCol) {
        case 1: c = a.us < b.us ? -1 : a.us > b.us ? 1 : 0; break;
        case 2:
            if (g_fs) { c = StrCmpLogicalW(g_fsItems[(size_t)a.idx].kind.c_str(), g_fsItems[(size_t)b.idx].kind.c_str()); break; }
            { uint64_t x = a.cs == UINT64_MAX ? 0 : a.cs, y = b.cs == UINT64_MAX ? 0 : b.cs; c = x < y ? -1 : x > y ? 1 : 0; break; }
        case 3: { uint32_t x = (uint32_t)a.d << 16 | a.t, y = (uint32_t)b.d << 16 | b.t; c = x < y ? -1 : x > y ? 1 : 0; break; }
        }
        if (c == 0) c = StrCmpLogicalW(a.name.c_str(), b.name.c_str());
        return g_sortDesc ? c > 0 : c < 0;
    });
}
static void ShowDir(int n, int focusDir = -1) {
    g_cur = n;
    for (int a = n; a >= 0; a = g_dirs[(size_t)a].parent) g_expand[(size_t)a] = 1;
    g_rows.clear();
    auto fname = [](int i) { const auto& n = g_arc.entries[(size_t)i].name; size_t s = n.find_last_of(L'/'); return s == std::wstring::npos ? n : n.substr(s + 1); };
    auto addDir = [&](int i, const std::wstring& shown) {
        Row r{1, i, shown};
        if (g_dirs[(size_t)i].entry >= 0) { auto& e = g_arc.entries[(size_t)g_dirs[(size_t)i].entry]; r.d = e.dosDate; r.t = e.dosTime; r.hasDate = true; }
        g_rows.push_back(r);
    };
    auto addFile = [&](int i, const std::wstring& shown) {
        auto& e = g_arc.entries[(size_t)i];
        g_rows.push_back({2, i, shown, e.usize, e.csize, e.dosDate, e.dosTime, true});
    };
    if (!g_search.empty()) {  // 이름에 검색어가 들어간 항목 (경로를 함께 보여 줌)
        for (size_t i = 1; i < g_dirs.size(); i++)
            if (StrStrIW(g_dirs[i].name.c_str(), g_search.c_str())) addDir((int)i, g_dirs[i].path);
        for (size_t i = 0; i < g_arc.entries.size(); i++)
            if (!g_arc.entries[i].isDir && StrStrIW(fname((int)i).c_str(), g_search.c_str())) addFile((int)i, g_arc.entries[i].name);
    } else {
        auto& d = g_dirs[(size_t)n];
        for (int i : d.dirs) addDir(i, g_dirs[(size_t)i].name);
        for (int i : d.files) addFile(i, fname(i));
    }
    SortRows();
    g_sel.assign(g_rows.size(), 0);
    g_focus = g_anchor = -1;
    g_scrollY = 0;
    g_hoverRow = -1;
    if (focusDir >= 0)
        for (size_t i = 0; i < g_rows.size(); i++)
            if (g_rows[i].kind == 1 && g_rows[i].idx == focusDir) { g_focus = g_anchor = (int)i; g_sel[i] = 1; }
}
// 선택 항목 추출 등에서 잘라 낼 경로 앞부분 (검색 결과는 전체 경로 유지)
static std::wstring CurStrip() { return g_cur == 0 || !g_search.empty() ? L"" : g_dirs[(size_t)g_cur].path + L"/"; }

static void BuildPlaces() {
    g_places.clear();
    g_places.push_back({L"내 PC", L"", IC_DRIVE});
    struct { const KNOWNFOLDERID* id; const wchar_t* n; } kf[] = {
        {&FOLDERID_Desktop, L"바탕 화면"}, {&FOLDERID_Documents, L"문서"}, {&FOLDERID_Downloads, L"다운로드"}, {&FOLDERID_Pictures, L"사진"}};
    for (auto& k : kf) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*k.id, 0, nullptr, &p)) && p && DirExists(p)) g_places.push_back({k.n, FsNorm(p), IC_FOLDER});
        if (p) CoTaskMemFree(p);
    }
    wchar_t buf[512];
    DWORD n = GetLogicalDriveStringsW(511, buf);
    for (wchar_t* d = buf; n && *d; d += wcslen(d) + 1) g_places.push_back({FsLabel(d), d, IC_DRIVE});
}
// 지금 폴더(g_fsPath) 읽기. 열 수 없으면 false
static bool FsLoad() {
    std::vector<FsItem> v;
    if (g_fsPath.empty()) {  // 내 PC: 자주 쓰는 폴더 + 드라이브
        for (auto& pl : g_places) {
            if (pl.path.empty()) continue;
            FsItem it; it.name = pl.name; it.path = pl.path; it.dir = true; it.drive = pl.icon == IC_DRIVE;
            if (it.drive) {
                UINT ty = GetDriveTypeW(pl.path.c_str());
                it.kind = ty == DRIVE_REMOVABLE ? L"이동식 디스크" : ty == DRIVE_REMOTE ? L"네트워크 드라이브" : ty == DRIVE_CDROM ? L"CD/DVD" : L"로컬 디스크";
                ULARGE_INTEGER fr{}, tot{};
                if ((ty == DRIVE_FIXED || ty == DRIVE_REMOVABLE) && GetDiskFreeSpaceExW(pl.path.c_str(), &fr, &tot, nullptr)) it.size = tot.QuadPart;
            } else it.kind = L"폴더";
            v.push_back(it);
        }
        g_fsItems.swap(v);
        return true;
    }
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(LongPath(FsJoin(g_fsPath, L"*")).c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) return false;
        g_fsItems.clear();
        return true;
    }
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;  // 숨김·시스템 파일은 탐색기처럼 감춤
        FsItem it;
        it.name = fd.cFileName;
        it.path = FsJoin(g_fsPath, it.name);
        it.dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        it.arc = !it.dir && fmt::IsArchivePath(it.name);
        it.size = it.dir ? 0 : ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        it.kind = FsKind(it.name, it.dir);
        FsDos(fd.ftLastWriteTime, it.d, it.t);
        it.hasDate = it.d != 0;
        v.push_back(it);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    g_fsItems.swap(v);
    return true;
}
// 목록 행 만들기 (검색어가 있으면 이 폴더에서 이름으로 거른다)
static void FsShow(const std::wstring& focus = L"") {
    g_rows.clear();
    for (size_t i = 0; i < g_fsItems.size(); i++) {
        const FsItem& it = g_fsItems[i];
        if (!g_search.empty() && !StrStrIW(it.name.c_str(), g_search.c_str())) continue;
        g_rows.push_back({it.dir ? 1 : 2, (int)i, it.name, it.size, UINT64_MAX, it.d, it.t, it.hasDate});
    }
    if (!g_fsPath.empty()) SortRows();  // 내 PC 는 자주 쓰는 폴더 → 드라이브 순서 그대로
    g_sel.assign(g_rows.size(), 0);
    g_focus = g_anchor = -1;
    g_scrollY = 0;
    g_hoverRow = -1;
    if (!focus.empty())
        for (size_t i = 0; i < g_rows.size(); i++) {
            const FsItem& it = g_fsItems[(size_t)g_rows[i].idx];
            if (!lstrcmpiW(it.name.c_str(), focus.c_str()) || !lstrcmpiW(it.path.c_str(), focus.c_str())) { g_focus = g_anchor = (int)i; g_sel[i] = 1; break; }
        }
}
static void ShowCur() { if (g_fs) FsShow(); else if (g_hasArc) ShowDir(g_cur); }
static std::vector<std::wstring> FsSelPaths(bool filesOnly = false, bool arcsOnly = false) {
    std::vector<std::wstring> v;
    if (!g_fs || g_fsPath.empty()) return v;  // 내 PC 의 드라이브·바로 가기는 대상이 아님
    for (size_t i = 0; i < g_rows.size(); i++) {
        if (!g_sel[i]) continue;
        const FsItem& it = g_fsItems[(size_t)g_rows[i].idx];
        if ((filesOnly && it.dir) || (arcsOnly && !it.arc)) continue;
        v.push_back(it.path);
    }
    return v;
}

static void CollectEntries(int node, std::vector<int>& out) {
    auto& d = g_dirs[(size_t)node];
    if (d.entry >= 0) out.push_back(d.entry);
    for (int f : d.files) out.push_back(f);
    for (int c : d.dirs) CollectEntries(c, out);
}
static void DirTotals(int node, uint64_t& us, size_t& files) {
    auto& d = g_dirs[(size_t)node];
    for (int f : d.files) { us += g_arc.entries[(size_t)f].usize; files++; }
    for (int c : d.dirs) DirTotals(c, us, files);
}

static std::wstring FmtSize(uint64_t b) {
    if (b == 0) return L"0 KB";
    if (b < 1024) return L"1 KB";
    double v = b / 1024.0;
    const wchar_t* u[] = {L"KB", L"MB", L"GB", L"TB"};
    int i = 0;
    while (v >= 1024 && i < 3) { v /= 1024; i++; }
    wchar_t s[32];
    if (i == 0) swprintf(s, 32, L"%.0f %ls", std::ceil(v), u[i]);
    else swprintf(s, 32, v < 100 ? L"%.1f %ls" : L"%.0f %ls", v, u[i]);
    return s;
}
static std::wstring FmtDate(uint16_t d, uint16_t t) {
    wchar_t s[32];
    swprintf(s, 32, L"%04u-%02u-%02u %02u:%02u", 1980 + (d >> 9), (d >> 5) & 15, d & 31, t >> 11, (t >> 5) & 63);
    return s;
}
static std::wstring FmtRatio(uint64_t us, uint64_t cs) {
    if (!us) return L"";
    double r = std::floor(100.0 * (1.0 - (double)cs / (double)us));
    if (r < 0) r = 0;
    wchar_t s[16];
    swprintf(s, 16, L"%.0f%%", r);
    return s;
}
static std::wstring FormatLabelBase();
static std::wstring FormatLabel() { return FormatLabelBase() + (g_hasArc && AnyHasPassword(g_arc) ? L" · 암호" : L""); }
static std::wstring FormatLabelBase() {
    std::wstring f = g_arc.format;
    if (f == L"ZIP") return L"ZIP";
    std::wstring u = f;
    CharUpperBuffW(&u[0], (DWORD)u.size());
    if (u == L"RAR5") return L"RAR";
    if (u == L"ISO" || u == L"UDF") return L"ISO 이미지";
    if (u == L"FAT" || u == L"NTFS" || u == L"MBR" || u == L"GPT") return L"디스크 이미지 (" + f + L")";
    if (u == L"GZIP") return L"GZ";
    if (u == L"BZIP2") return L"BZ2";
    return u;
}
static std::wstring SplitLabel(uint64_t mb) {
    if (mb >= 1024 && mb % 1024 == 0) return std::to_wstring(mb / 1024) + L" GB";
    if (mb == 4095) return L"4 GB";
    return std::to_wstring(mb) + L" MB";
}
static std::wstring FsLabel(const std::wstring& p);
static std::wstring DirTitle() {
    if (g_fs) return FsLabel(g_fsPath);
    if (!g_hasArc) return L"열린 압축 파일 없음";
    return g_cur == 0 ? FileNameOf(g_arc.path) : g_dirs[(size_t)g_cur].name;
}
static std::wstring ArcSummary() {  // "제안서.zip · 항목 33개 · 31.6 MB → 24.8 MB"
    std::wstring s = L"항목 " + std::to_wstring(g_fileCount) + L"개 · " + FormatLabel() + L" · " + FmtSize(g_totU) + L" → " + FmtSize(g_totC);
    if (g_arc.isSplit) s += L" · 분할 " + std::to_wstring(g_arc.volumes.size()) + L"개";
    return s;
}
static std::wstring SelSummary(int* count = nullptr) {
    int n = 0; uint64_t sz = 0;
    for (size_t i = 0; i < g_rows.size(); i++) if (g_sel[i]) { n++; sz += g_rows[i].us; }
    if (count) *count = n;
    if (!n) return L"";
    return std::to_wstring(n) + L"개 선택" + (sz ? L" (" + FmtSize(sz) + L")" : L"");
}

// ============================================================ 작업(스레드)
enum JobKind { JK_NONE, JK_COMPRESS, JK_ADD, JK_EXTRACT, JK_TEST, JK_MODIFY, JK_HASH };
struct JobState {
    JobKind kind = JK_NONE;
    Job job;
    std::vector<std::wstring> sources;
    std::wstring target, dest, strip, prefix, err, firstError, cur, arcPath, password, openAfter;
    uint64_t split = 0;
    int level = 5;
    std::vector<int> idx;
    bool ok = false, collecting = false;
    int failed = 0;
    std::atomic<uint32_t> filesDone{0}, filesTotal{0};
    std::vector<uint64_t> cum;   // 항목별 누적 크기 → 처리한 바이트로 '몇 개째'를 계산
    std::vector<std::wstring> names;
    std::wstring reopenPw;       // 압축 후 바로 열 때 암호를 다시 묻지 않도록
    bool sfx = false;            // 자동 실행 압축 파일(.exe)로 만들기
    std::map<int, std::wstring> rename;  // JK_MODIFY: 이름 바꾸기
    std::vector<int> del;                // JK_MODIFY: 삭제
    std::wstring modifyWhat;             // 완료 메시지용 ("이름 바꾸기" / "삭제")
    std::vector<std::wstring> hashFiles; // JK_HASH: 해시를 구할 디스크 파일 (비어 있으면 idx 항목을 임시 폴더에 풀어서)
    std::vector<std::pair<std::wstring, std::wstring>> hashes;  // 결과 (이름, SHA-256)
    CRITICAL_SECTION cs;
    JobState() { InitializeCriticalSection(&cs); }
    ~JobState() { DeleteCriticalSection(&cs); }
};
static JobState* g_job = nullptr;
static std::vector<JobState*> g_queue;  // 대기 중인 작업 (앞에서부터 실행)
static HANDLE g_jobThread = nullptr;
static bool g_quitAfterJob = false;
static bool g_menuCompress = false;  // 우클릭 '압축'으로 실행됨
#define WM_APP_JOBDONE (WM_APP + 1)
#define WM_APP_ASK (WM_APP + 2)
#define WM_APP_PASSWORD (WM_APP + 3)
#define WM_APP_UPDATE (WM_APP + 20)

// SHA-256 (Windows CNG). 진행은 job.done 에 더한다
static bool Sha256File(const std::wstring& path, std::wstring& hex, Job& job) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
    bool ok = BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0;
    HANDLE f = ok ? CreateFileW(LongPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr) : INVALID_HANDLE_VALUE;
    if (f == INVALID_HANDLE_VALUE) ok = false;
    std::vector<uint8_t> buf(1 << 20);
    while (ok) {
        if (job.cancel) { ok = false; break; }
        DWORD got = 0;
        if (!ReadFile(f, buf.data(), (DWORD)buf.size(), &got, nullptr)) { ok = false; break; }
        if (!got) break;
        BCryptHashData(h, buf.data(), got, 0);
        job.done += got;
    }
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    uint8_t d[32];
    if (ok && BCryptFinishHash(h, d, 32, 0) == 0) {
        wchar_t b[65];
        for (int i = 0; i < 32; i++) swprintf(b + i * 2, 3, L"%02x", d[i]);
        hex = b;
    } else ok = false;
    if (h) BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}
static std::wstring ExePath();
static std::wstring TempWorkDir(const wchar_t* tag) {  // %TEMP%\Chagok\<tag>-<번호>
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"Chagok\\" + tag + L"-" + std::to_wstring(GetTickCount64());
    MakeDirs(dir);
    return dir;
}

static DWORD WINAPI JobThread(LPVOID) {
    JobState* j = g_job;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    j->job.onFile = [j](const std::wstring& n) {
        if (n.empty()) return;  // 기존 항목 복사
        j->filesDone++;
        EnterCriticalSection(&j->cs); j->cur = n; LeaveCriticalSection(&j->cs);
    };
    j->job.askOverwrite = [](const std::wstring& p) { return (int)SendMessageW(g_hwnd, WM_APP_ASK, 0, (LPARAM)&p); };
    j->job.askPassword = [](std::wstring& pw) { return SendMessageW(g_hwnd, WM_APP_PASSWORD, 0, (LPARAM)&pw) != 0; };
    g_zipLevel = j->level;
    switch (j->kind) {
    case JK_COMPRESS: {
        std::vector<AddItem> items;
        std::vector<std::wstring> ex{j->target};
        std::wstring stem = j->target.size() > 4 ? j->target.substr(0, j->target.size() - 4) : j->target;
        for (int i = 1; i < 100; i++) { wchar_t e[8]; wsprintfW(e, L".z%02d", i); ex.push_back(stem + e); }
        j->collecting = true;
        CollectItems(j->sources, L"", ex, items);
        j->collecting = false;
        if (items.empty()) { j->err = L"압축할 파일이 없습니다."; break; }
        { uint64_t c = 0; for (auto& it : items) { c += it.size; j->cum.push_back(c); j->names.push_back(it.arcName); } }
        j->filesTotal = (uint32_t)items.size();
        if (j->sfx) {
            // 자동 실행 파일: 풀기 프로그램(ChagokSFX.bin) + ZIP 을 이어 붙인다
            std::wstring stub = ParentOf(ExePath()) + L"\\ChagokSFX.bin", tmp = j->target + L".~sfx.zip";
            if (!FileExists(stub)) { j->err = L"자동 실행 파일을 만드는 데 필요한 ChagokSFX.bin 이 없습니다. 차곡을 다시 설치하세요."; break; }
            j->ok = AnyCreate(tmp, items, 0, j->job, j->err, j->password);
            if (j->ok) {
                HANDLE out = CreateFileW(LongPath(j->target).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                j->ok = out != INVALID_HANDLE_VALUE;
                std::vector<uint8_t> buf(1 << 20);
                for (const std::wstring& part : {stub, tmp}) {
                    HANDLE in = j->ok ? CreateFileW(LongPath(part).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr) : INVALID_HANDLE_VALUE;
                    if (in == INVALID_HANDLE_VALUE) { j->ok = false; break; }
                    DWORD got = 0, wr = 0;
                    while (j->ok && ReadFile(in, buf.data(), (DWORD)buf.size(), &got, nullptr) && got)
                        if (!WriteFile(out, buf.data(), got, &wr, nullptr) || wr != got) j->ok = false;
                    CloseHandle(in);
                }
                if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
                if (!j->ok) { j->err = L"자동 실행 파일을 쓸 수 없습니다: " + j->target; DeleteFileW(LongPath(j->target).c_str()); }
            }
            DeleteFileW(LongPath(tmp).c_str());
        } else
            j->ok = AnyCreate(j->target, items, j->split, j->job, j->err, j->password);
        if (!j->password.empty()) SecureZeroMemory(&j->password[0], j->password.size() * sizeof(wchar_t));
        break;
    }
    case JK_ADD: {
        std::vector<AddItem> items;
        j->collecting = true;
        CollectItems(j->sources, j->prefix, {g_arc.path}, items);
        j->collecting = false;
        j->filesTotal = (uint32_t)items.size();
        j->ok = AnyAdd(g_arc, items, j->job, j->err);
        break;
    }
    case JK_EXTRACT:
        { uint64_t c = 0; for (int i : j->idx) { c += g_arc.entries[(size_t)i].usize; j->cum.push_back(c); j->names.push_back(g_arc.entries[(size_t)i].name); } }
        j->filesTotal = (uint32_t)j->idx.size();
        j->ok = AnyExtract(g_arc, j->idx, j->strip, j->dest, j->job, j->err, j->failed, j->firstError);
        break;
    case JK_TEST:  // 압축 파일 검사: 모든 항목을 풀어 보기만 (파일은 쓰지 않음)
        { uint64_t c = 0; for (int i : j->idx) { c += g_arc.entries[(size_t)i].usize; j->cum.push_back(c); j->names.push_back(g_arc.entries[(size_t)i].name); } }
        j->filesTotal = (uint32_t)j->idx.size();
        j->job.testOnly = true;
        j->ok = AnyExtract(g_arc, j->idx, L"", L"", j->job, j->err, j->failed, j->firstError);
        break;
    case JK_MODIFY:
        j->filesTotal = 0;
        j->ok = AnyModify(g_arc, j->rename, j->del, j->job, j->err);
        break;
    case JK_HASH: {
        std::vector<std::wstring> files = j->hashFiles;
        std::vector<std::wstring> shown;
        if (files.empty()) {  // 압축 파일 안의 파일: 임시 폴더에 풀어서
            std::wstring dir = TempWorkDir(L"hash");
            j->ok = AnyExtract(g_arc, j->idx, j->strip, dir, j->job, j->err, j->failed, j->firstError);
            if (!j->ok || j->failed) { if (j->err.empty()) j->err = j->firstError; j->ok = false; break; }
            for (int i : j->idx) {
                std::wstring rel = g_arc.entries[(size_t)i].name;
                if (!j->strip.empty() && rel.compare(0, j->strip.size(), j->strip) == 0) rel = rel.substr(j->strip.size());
                files.push_back(dir + L"\\" + SanitizeRel(rel));
                shown.push_back(FileNameOf(g_arc.entries[(size_t)i].name));
            }
            j->dest = dir;  // 끝나면 지운다
        } else for (auto& f : files) shown.push_back(FileNameOf(f));
        uint64_t total = 0;
        for (auto& f : files) { WIN32_FILE_ATTRIBUTE_DATA d; if (GetFileAttributesExW(LongPath(f).c_str(), GetFileExInfoStandard, &d)) total += ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow; }
        j->job.total = total ? total : 1; j->job.done = 0;
        j->filesTotal = (uint32_t)files.size();
        j->ok = true;
        for (size_t k = 0; k < files.size() && j->ok; k++) {
            EnterCriticalSection(&j->cs); j->cur = shown[k]; LeaveCriticalSection(&j->cs);
            j->filesDone++;
            std::wstring hex;
            if (!Sha256File(files[k], hex, j->job)) { j->ok = false; j->err = j->job.cancel ? L"취소되었습니다." : L"파일을 읽을 수 없습니다: " + files[k]; }
            else j->hashes.push_back({shown[k], hex});
        }
        break;
    }
    default: break;
    }
    CoUninitialize();
    PostMessageW(g_hwnd, WM_APP_JOBDONE, 0, 0);
    return 0;
}
static bool Busy() { return g_job != nullptr; }
static const wchar_t* JobName() {
    if (!g_job) return L"";
    switch (g_job->kind) {
    case JK_COMPRESS: return g_job->split ? L"분할 압축" : L"압축";
    case JK_ADD: return L"파일 추가";
    case JK_EXTRACT: return g_job->openAfter.empty() ? L"압축 해제" : L"파일 여는";
    case JK_TEST: return L"압축 파일 검사";
    case JK_MODIFY: return g_job->modifyWhat.c_str();
    case JK_HASH: return L"SHA-256 계산";
    default: return L"";
    }
}

// ============================================================ 레이아웃
enum BtnStyle { BS_LABEL, BS_PRIMARY, BS_SIDE, BS_OUTLINE, BS_ICONBTN, BS_ACCENT, BS_CRUMB, BS_SEP, BS_RIBBON, BS_TAB,
                BS_VIEWTOG, BS_RAIL, BS_GRID, BS_PILL, BS_TOOL, BS_OUTLINE2, BS_PROGPILL, BS_RECENT,
                BS_LAYER, BS_DOCK, BS_DOCKACC, BS_HERO, BS_HEROW, BS_HEROICON, BS_ORB, BS_ORBSM };
enum CmdId { ID_NONE, ID_COMPRESS = 100, ID_SPLIT, ID_EXTRACT_ALL, ID_ADD, ID_EXTRACT_SEL, ID_THEME, ID_BACK,
             ID_CANCEL, ID_OPEN, ID_SETTINGS, ID_VIEW_TILES, ID_VIEW_LIST, ID_OPENFILE, ID_SHOWPROG, ID_SEARCHCLR, ID_BROWSE, ID_FSCRUMB = 600, ID_RECENT = 900, ID_CRUMB = 1000 };
struct Btn {
    int id;
    RECT rc;
    std::wstring text;
    int icon, style;
    bool enabled;
    bool current = false;
    bool small = false;
};
struct TreeRow { int node, depth; RECT rc; bool kids; };
struct Group { RECT rc; std::wstring label; };
struct Layout {
    std::vector<Btn> btns;
    std::vector<TreeRow> trows;
    std::vector<Group> groups;
    RECT side{}, top{}, ribbon{}, crumb{}, header{}, list{}, status{}, card{}, zone{}, info{}, tree{}, panel{}, mid{};
    RECT title{}, sub{}, chips{}, logo{}, search{}, dock{};
    RECT sbTrack{}, sbThumb{};
    int rowH = 24, padTop = 0, inset = 0, treeRowH = 28, emptyY = 0;
    bool hasSb = false, tiles = false;
    int tileW = 0, tileH = 0, cols = 1, gap = 0;
    float rowRad = 0;
} L;
static bool g_progHidden = false;  // 진행 창을 '백그라운드로' 숨김

static int ContentH() {
    if (L.tiles) { int rows = ((int)g_rows.size() + L.cols - 1) / max(1, L.cols); return L.padTop * 2 + rows * (L.tileH + L.gap); }
    return (int)g_rows.size() * L.rowH + L.padTop * 2;
}
static int ListH() { return L.list.bottom - L.list.top; }
static void ClampScroll() {
    int mx = max(0, ContentH() - ListH());
    g_scrollY = max(0, min(g_scrollY, mx));
}
// 항목 i 의 화면 위치 (스크롤 반영)
static RECT ItemRect(int i) {
    if (L.tiles) {
        int c = i % L.cols, r = i / L.cols;
        int x = L.list.left + L.gap + c * (L.tileW + L.gap), y = L.list.top + L.padTop + r * (L.tileH + L.gap) - g_scrollY;
        return {x, y, x + L.tileW, y + L.tileH};
    }
    int y = L.list.top + L.padTop + i * L.rowH - g_scrollY;
    return {L.list.left + L.inset, y, L.list.right - L.inset - (L.hasSb ? S(10) : 0), y + L.rowH};
}
static int ItemAt(POINT p) {
    if (!PtInRect(&L.list, p) || g_rows.empty()) return -1;
    if (L.tiles) {
        int x = p.x - L.list.left - L.gap, y = p.y - L.list.top - L.padTop + g_scrollY;
        if (x < 0 || y < 0) return -1;
        int c = x / (L.tileW + L.gap), r = y / (L.tileH + L.gap);
        if (c >= L.cols || x % (L.tileW + L.gap) > L.tileW || y % (L.tileH + L.gap) > L.tileH) return -1;
        int i = r * L.cols + c;
        return i < (int)g_rows.size() ? i : -1;
    }
    int y = p.y - L.list.top - L.padTop + g_scrollY;
    if (y < 0) return -1;
    int r = y / L.rowH;
    return r < (int)g_rows.size() ? r : -1;
}

static Btn& AddB(int id, RECT rc, const std::wstring& t, int ic, int st, bool en, bool cur = false) {
    Btn b{id, rc, t, ic, st, en, cur, false};
    L.btns.push_back(b);
    return L.btns.back();
}
static int BtnW(const std::wstring& t, HFONT f, bool icon) { return S(14) + (icon ? S(16) + S(7) : 0) + TextW(t, f) + S(16); }

static void AddCrumbs(int x1, int x2, int y1, int y2, bool small) {
    if (!HasList()) return;
    std::vector<int> ids;            // 단추 id
    std::vector<std::wstring> names;
    if (g_fs) {
        g_fsCrumbs.clear();
        for (std::wstring p = g_fsPath;; p = FsParent(p)) { g_fsCrumbs.insert(g_fsCrumbs.begin(), p); if (p.empty()) break; }
        for (size_t i = 0; i < g_fsCrumbs.size(); i++) { ids.push_back(ID_FSCRUMB + (int)i); names.push_back(FsLabel(g_fsCrumbs[i])); }
    } else {
        std::vector<int> chain;
        for (int n = g_cur; n >= 0; n = g_dirs[(size_t)n].parent) chain.push_back(n);
        std::reverse(chain.begin(), chain.end());
        for (int n : chain) { ids.push_back(ID_CRUMB + n); names.push_back(n == 0 ? FileNameOf(g_arc.path) : g_dirs[(size_t)n].name); }
    }
    size_t last = ids.size() - 1;
    std::vector<int> chain(ids.size());
    HFONT fb = small ? g_fontSmall : g_fontBold, fn = small ? g_fontSmall : g_font;
    std::wstring sep = L"›";
    int sepW = TextW(sep, fn) + S(10);
    std::vector<int> widths;
    for (size_t i = 0; i < names.size(); i++) widths.push_back(TextW(names[i], i == last ? fb : fn) + S(8));
    int avail = x2 - x1, used = 0;
    size_t start = chain.size();
    while (start > 0) {
        int w = widths[start - 1] + (start < chain.size() ? sepW : 0);
        if (used + w > avail && start != chain.size()) break;
        used += w;
        start--;
    }
    int x = x1;
    auto add = [&](int id, RECT r, const std::wstring& t, int st, bool en, bool cur) {
        Btn& b = AddB(id, r, t, IC_NONE, st, en, cur);
        b.small = small;
    };
    if (start > 0) {
        int w = TextW(L"…", fn) + S(8);
        add(0, {x, y1, x + w, y2}, L"…", BS_LABEL, false, false);
        x += w;
        add(0, {x, y1, x + sepW, y2}, sep, BS_SEP, false, false);
        x += sepW;
    }
    for (size_t i = start; i < chain.size(); i++) {
        int w = min(widths[i], x2 - x);
        if (w <= 0) break;
        add(ids[i], {x, y1, x + w, y2}, names[i], BS_CRUMB, i != last, i == last);
        x += w;
        if (i + 1 < chain.size()) { add(0, {x, y1, x + sepW, y2}, sep, BS_SEP, false, false); x += sepW; }
    }
}

static int SelCount() { int c = 0; for (char s : g_sel) c += s; return c; }
static bool HasRealSel() { return SelCount() > 0; }
static int SingleSel() {  // 하나만 선택되어 있으면 그 행
    int r = -1;
    for (size_t i = 0; i < g_sel.size(); i++) if (g_sel[i]) { if (r >= 0) return -1; r = (int)i; }
    return r;
}

// 정보 패널(민트)에 표시할 내용
struct InfoView { int icon; std::wstring name; std::vector<std::pair<std::wstring, std::wstring>> rows; int b1, b2; std::wstring t1, t2; int ic1 = IC_DOWNLOAD; };
static InfoView GetInfo() {
    InfoView v{IC_ARCHIVE, L"", {}, 0, 0, L"", L""};
    if (g_fs) {  // 내 PC 둘러보기
        int n = SelCount(), one = SingleSel();
        const FsItem* it = FsAt(one);
        bool root = g_fsPath.empty();
        if (it) {
            v.icon = it->drive ? IC_DRIVE : it->dir ? IC_FOLDER : it->arc ? IC_ARCHIVE : IC_FILE;
            v.name = it->name;
            v.rows = {{L"종류", it->kind}};
            if (!it->dir || it->drive) v.rows.push_back({L"크기", it->size ? FmtSize(it->size) : L"-"});
            if (it->hasDate) v.rows.push_back({L"수정한 날짜", FmtDate(it->d, it->t).substr(0, 10)});
            if (it->arc) { v.b1 = ID_EXTRACT_ALL; v.t1 = L"압축 풀기"; v.ic1 = IC_EXTRACT; v.b2 = ID_OPENFILE; v.t2 = L"차곡으로 열기"; }
            else if (!root) { v.b1 = ID_COMPRESS; v.t1 = L"압축하기"; v.ic1 = IC_ARCHIVE; v.b2 = ID_OPENFILE; v.t2 = L"열기"; }
            else { v.b2 = ID_OPENFILE; v.t2 = L"열기"; }
        } else if (n > 1) {
            uint64_t us = 0; int files = 0, dirs = 0;
            for (size_t i = 0; i < g_rows.size(); i++) if (g_sel[i]) { if (g_rows[i].kind == 2) { us += g_rows[i].us; files++; } else dirs++; }
            v.icon = IC_STACK; v.name = std::to_wstring(n) + L"개 항목 선택";
            v.rows = {{L"폴더", std::to_wstring(dirs) + L"개"}, {L"파일", std::to_wstring(files) + L"개"}, {L"파일 크기", FmtSize(us)}};
            if (!root) { v.b1 = ID_COMPRESS; v.t1 = L"선택 항목 압축"; v.ic1 = IC_ARCHIVE; }
        } else {
            int files = 0, dirs = 0, arcs = 0;
            for (auto& it2 : g_fsItems) { if (it2.dir) dirs++; else files++; if (it2.arc) arcs++; }
            v.icon = root ? IC_DRIVE : IC_FOLDER; v.name = FsLabel(g_fsPath);
            v.rows = {{L"폴더", std::to_wstring(dirs) + L"개"}, {L"파일", std::to_wstring(files) + L"개"}};
            if (!root) v.rows.push_back({L"압축 파일", std::to_wstring(arcs) + L"개"});
        }
        return v;
    }
    if (!g_hasArc) { v.name = L"열린 파일 없음"; return v; }
    int n = SelCount(), one = SingleSel();
    if (one >= 0 && g_rows[(size_t)one].kind == 2) {
        const Row& r = g_rows[(size_t)one];
        const auto& e = g_arc.entries[(size_t)r.idx];
        v.icon = IC_FILE; v.name = r.name;
        v.rows = {{L"원본 크기", FmtSize(r.us)}};
        if (r.cs != UINT64_MAX) { v.rows.push_back({L"압축 후", FmtSize(r.cs)}); v.rows.push_back({L"압축률", FmtRatio(r.us, r.cs)}); }
        if (r.hasDate) v.rows.push_back({L"수정한 날짜", FmtDate(r.d, r.t).substr(0, 10)});
        v.rows.push_back({L"암호", e.Encrypted() ? L"있음" : L"없음"});
        v.b1 = ID_EXTRACT_SEL; v.t1 = L"이 파일 추출"; v.b2 = ID_OPENFILE; v.t2 = L"바로 열기";
    } else if (one >= 0) {
        const Row& r = g_rows[(size_t)one];
        uint64_t us = 0; size_t files = 0;
        DirTotals(r.idx, us, files);
        v.icon = IC_FOLDER; v.name = r.name;
        v.rows = {{L"종류", L"폴더"}, {L"파일", std::to_wstring(files) + L"개"}, {L"원본 크기", FmtSize(us)}};
        if (r.hasDate) v.rows.push_back({L"수정한 날짜", FmtDate(r.d, r.t).substr(0, 10)});
        v.b1 = ID_EXTRACT_SEL; v.t1 = L"이 폴더 추출";
    } else if (n > 1) {
        uint64_t us = 0; size_t files = 0;
        for (size_t i = 0; i < g_rows.size(); i++) {
            if (!g_sel[i]) continue;
            if (g_rows[i].kind == 2) { us += g_rows[i].us; files++; } else DirTotals(g_rows[i].idx, us, files);
        }
        v.icon = IC_STACK; v.name = std::to_wstring(n) + L"개 항목 선택";
        v.rows = {{L"파일", std::to_wstring(files) + L"개"}, {L"원본 크기", FmtSize(us)}};
        v.b1 = ID_EXTRACT_SEL; v.t1 = L"선택 항목 추출";
    } else {
        v.icon = IC_ARCHIVE; v.name = FileNameOf(g_arc.path);
        v.rows = {{L"형식", FormatLabel()}, {L"파일", std::to_wstring(g_fileCount) + L"개"}, {L"원본 크기", FmtSize(g_totU)},
                  {L"압축 크기", FmtSize(g_totC)}, {L"압축률", FmtRatio(g_totU, g_totC)}};
        if (g_arc.isSplit) v.rows.push_back({L"분할", std::to_wstring(g_arc.volumes.size()) + L"개"});
        v.b1 = ID_EXTRACT_ALL; v.t1 = L"전체 압축 해제";
    }
    return v;
}

static void LayoutBasic(int W, int H, bool busy) {
    int tabH = S(34);
    L.top = {0, 0, W, tabH};
    int x = S(10);
    struct { int id; const wchar_t* t; } tabs[] = {{0, L"홈"}, {ID_SETTINGS, L"설정"}, {ID_THEME, L"테마"}};
    for (auto& t : tabs) {
        int w = TextW(t.t, g_fontBold) + S(30);
        AddB(t.id, {x, S(6), x + w, tabH}, t.t, IC_NONE, BS_TAB, t.id != 0, t.id == 0);
        x += w + S(2);
    }
    L.ribbon = {0, tabH, W, tabH + S(98)};
    int by = tabH + S(6), bh = S(66);
    x = S(12);
    auto group = [&](const wchar_t* label, std::initializer_list<std::tuple<int, const wchar_t*, int, bool>> bs) {
        int gx = x;
        for (auto& b : bs) {
            int w = max(S(68), TextW(std::get<1>(b), g_font) + S(18));
            AddB(std::get<0>(b), {x, by, x + w, by + bh}, std::get<1>(b), std::get<2>(b), BS_RIBBON, std::get<3>(b));
            x += w + S(2);
        }
        x += S(10);
        L.groups.push_back({{gx, by, x - S(4), L.ribbon.bottom - S(4)}, label});
        x += S(12);
    };
    group(L"압축", {{ID_COMPRESS, L"새로 압축", IC_ARCHIVE, !busy}, {ID_SPLIT, L"분할 압축", IC_SPLIT, !busy},
                   {ID_EXTRACT_ALL, L"압축 해제", IC_EXTRACT, !busy}});
    group(L"탐색", {{ID_BROWSE, L"내 PC", IC_DRIVE, true}});
    group(L"항목", {{ID_ADD, L"파일 추가", IC_PLUS, g_hasArc && !busy && !g_arc.readOnly},
                   {ID_EXTRACT_SEL, L"선택 추출", IC_DOWNLOAD, g_hasArc && !busy && HasRealSel()}});
    {
        int gx = x, w = max(TextW(L"큰 아이콘", g_fontSmall), TextW(L"자세히", g_fontSmall)) + S(42);
        AddB(ID_VIEW_TILES, {x, by + S(4), x + w, by + S(30)}, L"큰 아이콘", IC_GRID, BS_VIEWTOG, true, g_view == 0);
        AddB(ID_VIEW_LIST, {x, by + S(36), x + w, by + S(62)}, L"자세히", IC_LIST, BS_VIEWTOG, true, g_view == 1);
        x += w + S(12);
        L.groups.push_back({{gx, by, x - S(2), L.ribbon.bottom - S(4)}, L"보기"});
    }
    L.crumb = {0, L.ribbon.bottom, W, L.ribbon.bottom + S(36)};
    AddB(ID_BACK, {S(10), L.crumb.top + S(5), S(36), L.crumb.bottom - S(5)}, L"", IC_BACK, BS_ICONBTN, CanBack());
    if (HasList()) L.search = {W - S(16) - S(240), L.crumb.top + S(4), W - S(16), L.crumb.bottom - S(4)};
    AddCrumbs(S(42), HasList() ? L.search.left - S(10) : W - S(16), L.crumb.top, L.crumb.bottom, false);
    L.status = {0, H - S(28), W, H};
    if (g_view == 0) {
        L.tiles = true;
        L.list = {0, L.crumb.bottom, W, L.status.top};
        L.gap = S(16);
        L.padTop = S(2);
        int avail = W - L.gap - S(12);
        L.cols = max(1, avail / (S(150) + L.gap));
        L.tileW = min(S(200), avail / L.cols - L.gap);
        L.tileH = S(132);
    } else {
        L.header = {0, L.crumb.bottom, W, L.crumb.bottom + S(30)};
        L.list = {0, L.header.bottom, W, L.status.top};
        L.rowH = S(32); L.inset = S(10); L.rowRad = SF(4); L.padTop = S(2);
    }
}

static void LayoutMidnight(int W, int H, bool busy) {
    L.rowH = S(40); L.padTop = S(4); L.inset = S(10); L.rowRad = SF(6);
    L.side = {0, 0, S(232), H};
    int y = S(14);
    AddB(ID_COMPRESS, {S(14), y, S(218), y + S(46)}, L"새로 압축", IC_ARCHIVE, BS_PRIMARY, !busy); y += S(54);
    AddB(ID_SPLIT, {S(14), y, S(218), y + S(46)}, L"분할 압축", IC_SPLIT, BS_SIDE, !busy); y += S(54);
    AddB(ID_EXTRACT_ALL, {S(14), y, S(218), y + S(46)}, L"압축 해제", IC_EXTRACT, BS_SIDE, !busy); y += S(54);
    AddB(ID_BROWSE, {S(14), y, S(218), y + S(46)}, L"내 PC", IC_DRIVE, BS_SIDE, true, g_fs);
    L.card = {S(14), H - S(16) - S(140), S(218), H - S(16)};
    L.panel = {S(232), S(12), W - S(14), H - S(14)};
    RECT& p = L.panel;
    int cy = p.top + S(14), ch = S(36);
    L.top = {p.left, p.top, p.right, cy + ch + S(14)};
    AddB(ID_BACK, {p.left + S(12), cy + S(2), p.left + S(44), cy + ch - S(2)}, L"", IC_BACK, BS_ICONBTN, CanBack());
    int x = p.right - S(16);
    AddB(ID_THEME, {x - ch - S(8), cy, x, cy + ch}, L"", IC_THEME, BS_OUTLINE, true); x -= ch + S(8) + S(8);
    AddB(ID_SETTINGS, {x - ch - S(8), cy, x, cy + ch}, L"", IC_GEAR, BS_OUTLINE, true); x -= ch + S(8) + S(10);
    int w = BtnW(L"선택 추출", g_fontBold, true);
    AddB(ID_EXTRACT_SEL, {x - w, cy, x, cy + ch}, L"선택 추출", IC_DOWNLOAD, BS_ACCENT, g_hasArc && !busy && HasRealSel()); x -= w + S(8);
    w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {x - w, cy, x, cy + ch}, L"파일 추가", IC_PLUS, BS_OUTLINE, g_hasArc && !busy && !g_arc.readOnly); x -= w + S(8);
    if (HasList()) { int sw = min(S(200), max(S(120), (int)(x - p.left) / 3)); L.search = {x - sw, cy, x, cy + ch}; x -= sw + S(8); }
    AddCrumbs(p.left + S(52), x - S(8), cy, cy + ch, false);
    if (!HasList()) L.title = {p.left + S(52), cy, x - S(8), cy + ch};  // 아무것도 안 열었을 때 제목
    L.zone = {p.left + S(18), p.bottom - S(16) - S(54), p.right - S(18), p.bottom - S(16)};
    if (HasList()) {
        L.header = {p.left, L.top.bottom + 1, p.right, L.top.bottom + 1 + S(34)};
        L.list = {p.left, L.header.bottom, p.right, L.zone.top - S(10)};
    } else L.list = {p.left, L.top.bottom + 1, p.right, L.zone.top - S(10)};
}

static void LayoutPastel(int W, int H, bool busy) {
    int pad = S(20);
    L.side = {pad, pad, pad + S(92), H - pad};
    RECT& r = L.side;
    int cx = (r.left + r.right) / 2;
    int bw = S(66), bh = S(64), y = r.top + S(22);
    struct { int id; const wchar_t* t; int ic; } rb[] = {{ID_COMPRESS, L"압축", IC_ARCHIVE}, {ID_SPLIT, L"분할", IC_SPLIT}, {ID_EXTRACT_ALL, L"풀기", IC_EXTRACT}, {ID_BROWSE, L"내 PC", IC_DRIVE}};
    int gapR = S(10), avail = (r.bottom - S(26) - 2 * bh) - S(10) - y;  // 아래 설정 버튼 위까지
    int rbh = max(S(48), min(bh, (avail - 3 * gapR) / 4));
    for (auto& b : rb) { AddB(b.id, {cx - bw / 2, y, cx + bw / 2, y + rbh}, b.t, b.ic, BS_RAIL, b.id == ID_BROWSE || !busy, b.id == ID_BROWSE && g_fs); y += rbh + gapR; }
    AddB(ID_THEME, {cx - bw / 2, r.bottom - S(16) - bh, cx + bw / 2, r.bottom - S(16)}, L"테마", IC_THEME, BS_RAIL, true);
    AddB(ID_SETTINGS, {cx - bw / 2, r.bottom - S(26) - 2 * bh, cx + bw / 2, r.bottom - S(26) - bh}, L"설정", IC_GEAR, BS_RAIL, true);
    L.panel = {r.right + pad, pad, W - pad, H - pad};
    RECT& p = L.panel;
    int ip = S(26), x = p.right - ip, by = p.top + S(24), bh2 = S(38);
    int w = BtnW(L"선택 추출", g_fontBold, true) + S(8);
    AddB(ID_EXTRACT_SEL, {x - w, by, x, by + bh2}, L"선택 추출", IC_DOWNLOAD, BS_ACCENT, g_hasArc && !busy && HasRealSel()); x -= w + S(10);
    w = BtnW(L"파일 추가", g_font, true) + S(8);
    AddB(ID_ADD, {x - w, by, x, by + bh2}, L"파일 추가", IC_PLUS, BS_OUTLINE, g_hasArc && !busy && !g_arc.readOnly); x -= w + S(12);
    int tx = p.left + ip;
    if (CanBack()) { AddB(ID_BACK, {tx - S(4), p.top + S(24), tx + S(28), p.top + S(56)}, L"", IC_BACK, BS_ICONBTN, true); tx += S(34); }
    L.title = {tx, p.top + S(20), x, p.top + S(52)};
    L.sub = {p.left + ip, p.top + S(52), x, p.top + S(72)};
    if (HasList()) AddCrumbs(L.sub.left - S(4), L.sub.right, L.sub.top, L.sub.bottom, true);
    L.chips = {p.left + ip, p.top + S(84), p.right - ip, p.top + S(108)};
    if (HasList()) { L.search = {p.right - ip - S(220), L.chips.top - S(4), p.right - ip, L.chips.bottom + S(4)}; L.chips.right = L.search.left - S(10); }
    L.inset = ip - S(6); L.rowH = S(44); L.rowRad = SF(10); L.padTop = 0;
    if (HasList()) {
        L.header = {p.left, p.top + S(118), p.right, p.top + S(148)};
        L.list = {p.left, L.header.bottom + S(2), p.right, p.bottom - S(18)};
    } else L.list = {p.left, p.top + S(118), p.right, p.bottom - S(18)};
}

static void LayoutTreeSide(int W, int H, bool busy) {
    bool navy = g_ly == TH_NAVY;
    int sw = S(260);
    L.side = {0, 0, sw, H};
    int y;
    if (!navy) {
        y = S(20);
        int gw = (sw - S(28) - S(18)) / 4;
        struct { int id; const wchar_t* t; int ic; } gb[] = {{ID_COMPRESS, L"압축", IC_ARCHIVE}, {ID_SPLIT, L"분할", IC_SPLIT}, {ID_EXTRACT_ALL, L"풀기", IC_EXTRACT}, {ID_BROWSE, L"내 PC", IC_DRIVE}};
        for (int i = 0; i < 4; i++) {
            int x = S(14) + i * (gw + S(6));
            AddB(gb[i].id, {x, y, x + gw, y + S(60)}, gb[i].t, gb[i].ic, BS_GRID, i == 3 || !busy, i == 0 || (i == 3 && g_fs));
        }
        y += S(60) + S(22);
    } else {
        y = S(20);
        AddB(ID_COMPRESS, {S(14), y, sw - S(14), y + S(42)}, L"새로 압축", IC_ARCHIVE, BS_PRIMARY, !busy); y += S(46);
        AddB(ID_SPLIT, {S(14), y, sw - S(14), y + S(42)}, L"분할 압축", IC_SPLIT, BS_SIDE, !busy); y += S(46);
        AddB(ID_EXTRACT_ALL, {S(14), y, sw - S(14), y + S(42)}, L"압축 해제", IC_EXTRACT, BS_SIDE, !busy); y += S(46);
        AddB(ID_BROWSE, {S(14), y, sw - S(14), y + S(42)}, L"내 PC", IC_DRIVE, BS_SIDE, true, g_fs); y += S(42) + S(22);
    }
    L.groups.push_back({{S(24), y - S(6), sw - S(10), y + S(14)}, g_fs ? L"내 PC" : navy ? L"압축 파일" : L"이 압축 파일"});
    L.tree = {S(10), y + S(18), sw - S(10), H - S(10)};
    L.treeRowH = S(30);
    int mx = sw + S(32), mr = W - S(30), by = S(26), bh = S(36);
    int x = mr;
    AddB(ID_THEME, {x - S(44), by, x, by + bh}, L"", IC_THEME, BS_OUTLINE, true); x -= S(44) + S(8);
    AddB(ID_SETTINGS, {x - S(44), by, x, by + bh}, L"", IC_GEAR, BS_OUTLINE, true); x -= S(44) + S(10);
    int w = BtnW(L"선택 추출", g_fontBold, true);
    AddB(ID_EXTRACT_SEL, {x - w, by, x, by + bh}, L"선택 추출", IC_DOWNLOAD, BS_ACCENT, g_hasArc && !busy && HasRealSel()); x -= w + S(8);
    w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {x - w, by, x, by + bh}, L"파일 추가", IC_PLUS, BS_OUTLINE, g_hasArc && !busy && !g_arc.readOnly); x -= w + S(14);
    if (HasList()) { int sw = min(S(200), max(S(120), (x - mx) / 3)); L.search = {x - sw, by, x, by + bh}; x -= sw + S(12); }
    int tx = mx;
    if (CanBack()) { AddB(ID_BACK, {tx - S(6), S(24), tx + S(26), S(56)}, L"", IC_BACK, BS_ICONBTN, true); tx += S(32); }
    L.title = {tx, S(20), x, S(54)};
    L.sub = {mx, S(54), x, S(72)};
    L.top = {mx, S(80), mr, S(80) + S(2)};  // 네이비골드: 금색 줄
    L.inset = 0; L.rowH = S(40); L.rowRad = 0;
    L.header = {mx, S(84), mr, S(116)};
    if (!navy) L.status = {mx, H - S(34), mr, H};
    L.list = {mx, L.header.bottom, mr, navy ? H - S(16) : L.status.top - S(4)};
    if (!HasList()) { L.header = {}; L.list.top = S(84); }
}

static void LayoutMint(int W, int H, bool busy) {
    L.top = {0, 0, W, S(58)};
    int by = S(11), bh = S(36), x = S(16);
    int w = BtnW(L"새로 압축", g_fontBold, true) + S(8);
    AddB(ID_COMPRESS, {x, by, x + w, by + bh}, L"새로 압축", IC_ARCHIVE, BS_PILL, !busy); x += w + S(8);
    w = BtnW(L"분할 압축", g_font, true);
    AddB(ID_SPLIT, {x, by, x + w, by + bh}, L"분할 압축", IC_SPLIT, BS_TOOL, !busy); x += w + S(4);
    w = BtnW(L"압축 해제", g_font, true);
    AddB(ID_EXTRACT_ALL, {x, by, x + w, by + bh}, L"압축 해제", IC_EXTRACT, BS_TOOL, !busy); x += w + S(4);
    w = BtnW(L"내 PC", g_font, true);
    AddB(ID_BROWSE, {x, by, x + w, by + bh}, L"내 PC", IC_DRIVE, BS_TOOL, true, g_fs);
    int rx = W - S(14);
    AddB(ID_THEME, {rx - bh, by, rx, by + bh}, L"", IC_THEME, BS_ICONBTN, true); rx -= bh + S(4);
    AddB(ID_SETTINGS, {rx - bh, by, rx, by + bh}, L"", IC_GEAR, BS_ICONBTN, true); rx -= bh + S(8);
    w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {rx - w, by, rx, by + bh}, L"파일 추가", IC_PLUS, BS_TOOL, g_hasArc && !busy && !g_arc.readOnly);
    int tw = S(230), iw = W >= S(1000) ? S(250) : S(220);
    L.side = {0, L.top.bottom, tw, H};
    L.groups.push_back({{S(18), L.side.top + S(10), tw - S(10), L.side.top + S(32)}, g_fs ? L"내 PC" : L"압축 파일"});
    L.tree = {S(6), L.side.top + S(36), tw - S(6), H - S(8)};
    L.treeRowH = S(30);
    L.info = {W - iw, L.top.bottom, W, H};
    L.mid = {tw, L.top.bottom, L.info.left, H};
    L.crumb = {L.mid.left + S(14), L.mid.top + S(4), L.mid.right - S(10), L.mid.top + S(38)};
    AddB(ID_BACK, {L.crumb.left, L.crumb.top + S(5), L.crumb.left + S(26), L.crumb.bottom - S(5)}, L"", IC_BACK, BS_ICONBTN, CanBack());
    if (HasList()) L.search = {L.crumb.right - S(200), L.crumb.top + S(4), L.crumb.right, L.crumb.bottom - S(4)};
    AddCrumbs(L.crumb.left + S(30), HasList() ? L.search.left - S(8) : L.crumb.right, L.crumb.top, L.crumb.bottom, false);
    L.inset = S(14); L.rowH = S(36); L.rowRad = SF(6); L.padTop = 0;
    if (HasList()) {
        L.header = {L.mid.left, L.crumb.bottom, L.mid.right, L.crumb.bottom + S(30)};
        L.list = {L.mid.left, L.header.bottom + S(2), L.mid.right, H - S(10)};
    } else L.list = {L.mid.left, L.crumb.bottom, L.mid.right, H - S(10)};
    // 정보 패널 버튼
    InfoView iv = GetInfo();
    int ix = L.info.left + S(18), ir = L.info.right - S(18);
    int iy = L.info.top + S(150) + (int)iv.rows.size() * S(30) + S(16);
    if (iv.b1) { AddB(iv.b1, {ix, iy, ir, iy + S(40)}, iv.t1, iv.ic1, BS_ACCENT, !busy && HasList()); iy += S(48); }
    if (iv.b2) AddB(iv.b2, {ix, iy, ir, iy + S(40)}, iv.t2, IC_NONE, BS_OUTLINE2, (!busy || g_fs) && HasList());
}

// ---------------------------------------------------------------- 고유 배치 5종 (1.14)
// 공통: 뒤로 + 제목 + 요약, 오른쪽에 찾기/파일 추가/선택 추출. 목록은 L.header / L.list
static void TitleRow(int left, int right, int y, int h, bool withSearch, int searchW, bool pills) {
    int x = right;
    (void)pills;
    if (withSearch && HasList()) { L.search = {x - searchW, y, x, y + h}; x -= searchW + S(10); }
    int tx = left;
    if (CanBack()) { AddB(ID_BACK, {tx - S(6), y + (h - S(32)) / 2, tx + S(26), y + (h + S(32)) / 2}, L"", IC_BACK, BS_ICONBTN, true); tx += S(32); }
    L.title = {tx, y - S(2), x, y + h + S(2)};
}
// 1) 차곡: 이불을 차곡차곡 쌓은 것처럼 층층이 놓인 버튼, 줄마다 색 띠, 쌓인 색 막대로 압축률
static void LayoutChagok(int W, int H, bool busy) {
    int sw = S(236);
    L.side = {0, 0, sw, H};
    int y = S(20);
    struct { int id; const wchar_t* t; int ic; } ly[] = {{ID_COMPRESS, L"새로 압축", IC_ARCHIVE}, {ID_SPLIT, L"분할 압축", IC_SPLIT},
                                                         {ID_EXTRACT_ALL, L"압축 해제", IC_EXTRACT}, {ID_BROWSE, L"내 PC", IC_DRIVE}};
    for (int i = 0; i < 4; i++) {
        int off = i * S(6);
        AddB(ly[i].id, {S(16) + off, y, sw - S(16), y + S(44)}, ly[i].t, ly[i].ic, BS_LAYER, ly[i].id == ID_BROWSE || !busy, ly[i].id == ID_BROWSE && g_fs);
        y += S(50);
    }
    y += S(14);
    L.groups.push_back({{S(20), y, sw - S(10), y + S(20)}, g_fs ? L"내 PC" : L"이 압축 파일"});
    L.tree = {S(10), y + S(24), sw - S(10), H - S(12)};
    L.treeRowH = S(30);
    int mx = sw + S(26), mr = W - S(26), by = S(18), bh = S(34), x = mr;
    AddB(ID_THEME, {x - bh, by, x, by + bh}, L"", IC_THEME, BS_OUTLINE, true); x -= bh + S(8);
    AddB(ID_SETTINGS, {x - bh, by, x, by + bh}, L"", IC_GEAR, BS_OUTLINE, true); x -= bh + S(10);
    int w = BtnW(L"선택 추출", g_fontBold, true);
    AddB(ID_EXTRACT_SEL, {x - w, by, x, by + bh}, L"선택 추출", IC_DOWNLOAD, BS_ACCENT, g_hasArc && !busy && HasRealSel()); x -= w + S(8);
    w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {x - w, by, x, by + bh}, L"파일 추가", IC_PLUS, BS_OUTLINE, g_hasArc && !busy && !g_arc.readOnly); x -= w + S(8);
    TitleRow(mx, x, by, bh, true, min(S(220), max(S(130), (x - mx) / 3)), false);
    L.sub = {L.title.left, by + bh + S(4), mr, by + bh + S(22)};
    L.card = {mx, S(84), mr, H - S(48)};
    L.status = {mx, H - S(36), mr, H - S(12)};
    L.inset = S(10); L.rowH = S(45); L.rowRad = SF(9); L.padTop = S(2);
    if (HasList()) {
        L.header = {L.card.left + S(10), L.card.top + S(6), L.card.right - S(10), L.card.top + S(38)};
        L.list = {L.card.left, L.header.bottom + S(2), L.card.right, L.card.bottom - S(6)};
    } else L.list = {L.card.left, L.card.top, L.card.right, L.card.bottom};
}
// 2) 쉼: 넉넉한 여백의 둥근 카드 하나, 자주 쓰는 버튼은 아래 떠 있는 작업 줄
static void LayoutShim(int W, int H, bool busy) {
    L.side = {S(18), S(18), S(18) + S(84), H - S(18)};
    int cx = (L.side.left + L.side.right) / 2, bw = S(66), bh = S(60);
    AddB(ID_BROWSE, {cx - bw / 2, L.side.top + S(18), cx + bw / 2, L.side.top + S(18) + bh}, L"내 PC", IC_DRIVE, BS_RAIL, true, g_fs);
    AddB(ID_THEME, {cx - bw / 2, L.side.bottom - S(16) - bh, cx + bw / 2, L.side.bottom - S(16)}, L"테마", IC_THEME, BS_RAIL, true);
    AddB(ID_SETTINGS, {cx - bw / 2, L.side.bottom - S(24) - 2 * bh, cx + bw / 2, L.side.bottom - S(24) - bh}, L"설정", IC_GEAR, BS_RAIL, true);
    L.panel = {L.side.right + S(18), S(18), W - S(18), H - S(18)};
    RECT& p = L.panel;
    int pad = S(34);
    TitleRow(p.left + pad, p.right - pad, p.top + S(26), S(40), true, min(S(260), max(S(150), (int)(p.right - p.left) / 3)), true);
    L.sub = {L.title.left, p.top + S(68), L.search.left > 0 ? L.search.left - S(10) : p.right - pad, p.top + S(88)};
    // 아래 작업 줄
    struct { int id; const wchar_t* t; int ic; bool en; } dk[] = {
        {ID_COMPRESS, L"새로 압축", IC_ARCHIVE, !busy}, {ID_SPLIT, L"분할", IC_SPLIT, !busy}, {ID_EXTRACT_ALL, L"압축 해제", IC_EXTRACT, !busy},
        {ID_ADD, L"파일 추가", IC_PLUS, g_hasArc && !busy && !g_arc.readOnly}, {ID_EXTRACT_SEL, L"선택 추출", IC_DOWNLOAD, g_hasArc && !busy && HasRealSel()}};
    int ws[5], total = 0;
    for (int i = 0; i < 5; i++) { ws[i] = BtnW(dk[i].t, g_fontBold, true) + S(4); total += ws[i]; }
    total += 4 * S(4);
    int dh = S(40), dy = p.bottom - S(30) - dh, dx = (p.left + p.right) / 2 - total / 2;
    L.dock = {dx - S(7), dy - S(7), dx + total + S(7), dy + dh + S(7)};
    for (int i = 0; i < 5; i++) { AddB(dk[i].id, {dx, dy, dx + ws[i], dy + dh}, dk[i].t, dk[i].ic, i == 0 ? BS_DOCKACC : BS_DOCK, dk[i].en); dx += ws[i] + S(4); }
    L.status = {L.dock.right + S(12), L.dock.top, p.right - pad, L.dock.bottom};
    L.inset = 0; L.rowH = S(50); L.rowRad = SF(16); L.padTop = 0;
    int lx = p.left + S(20), lr = p.right - S(20);
    if (HasList()) {
        L.header = {lx, p.top + S(100), lr, p.top + S(130)};
        L.list = {lx, L.header.bottom, lr, L.dock.top - S(10)};
    } else L.list = {lx, p.top + S(100), lr, L.dock.top - S(10)};
}
// 3) 봄빛: 밝은 그라데이션 머리 띠 위에 버튼, 목록 카드가 띠에 걸쳐 올라옴, 형식별 색 배지
static void LayoutBom(int W, int H, bool busy) {
    L.top = {0, 0, W, S(178)};
    int x = S(26), by = S(18), bh = S(34);
    struct { int id; const wchar_t* t; int ic; } pb[] = {{ID_COMPRESS, L"새로 압축", IC_ARCHIVE}, {ID_SPLIT, L"분할 압축", IC_SPLIT},
                                                         {ID_EXTRACT_ALL, L"압축 해제", IC_EXTRACT}, {ID_BROWSE, L"내 PC", IC_DRIVE}};
    for (int i = 0; i < 4; i++) {
        int w = BtnW(pb[i].t, g_fontBold, true) + S(4);
        AddB(pb[i].id, {x, by, x + w, by + bh}, pb[i].t, pb[i].ic, i == 0 ? BS_HEROW : BS_HERO, pb[i].id == ID_BROWSE || !busy, pb[i].id == ID_BROWSE && g_fs);
        x += w + S(8);
    }
    int rx = W - S(26);
    AddB(ID_THEME, {rx - bh, by, rx, by + bh}, L"", IC_THEME, BS_HEROW, true); rx -= bh + S(8);
    AddB(ID_SETTINGS, {rx - bh, by, rx, by + bh}, L"", IC_GEAR, BS_HEROW, true);
    int tx = S(26), ty = S(72);
    if (CanBack()) { AddB(ID_BACK, {tx - S(6), ty, tx + S(28), ty + S(34)}, L"", IC_BACK, BS_HEROICON, true); tx += S(34); }
    L.title = {tx, ty - S(2), W - S(26), ty + S(36)};
    L.card = {S(22), S(150), W - S(22), H - S(18)};
    RECT& c = L.card;
    int ty2 = c.top + S(14), th = S(34), cxr = c.right - S(20);
    int w = BtnW(L"선택 추출", g_fontBold, true);
    AddB(ID_EXTRACT_SEL, {cxr - w, ty2, cxr, ty2 + th}, L"선택 추출", IC_DOWNLOAD, BS_ACCENT, g_hasArc && !busy && HasRealSel()); cxr -= w + S(8);
    w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {cxr - w, ty2, cxr, ty2 + th}, L"파일 추가", IC_PLUS, BS_OUTLINE, g_hasArc && !busy && !g_arc.readOnly); cxr -= w + S(8);
    if (HasList()) { int sw = min(S(240), max(S(140), (int)(cxr - c.left) / 3)); L.search = {cxr - sw, ty2, cxr, ty2 + th}; cxr -= sw + S(10); }
    L.sub = {c.left + S(22), ty2, cxr, ty2 + th};
    L.inset = 0; L.rowH = S(45); L.rowRad = SF(12); L.padTop = S(2);
    if (HasList()) {
        L.header = {c.left + S(16), c.top + S(60), c.right - S(16), c.top + S(90)};
        L.list = {c.left + S(16), L.header.bottom, c.right - S(16), c.bottom - S(12)};
    } else L.list = {c.left + S(16), c.top + S(56), c.right - S(16), c.bottom - S(12)};
}
// 4) 물결: 물빛 배경 위 반투명 판. 위 한 줄 도구 막대 + 왼쪽 트리 + 목록
static void LayoutMul(int W, int H, bool busy) {
    L.top = {S(16), S(14), W - S(16), S(76)};
    int x = L.top.left + S(12), by = L.top.top + S(8), bh = S(46);
    struct { int id; const wchar_t* t; int ic; int st; } tb[] = {{ID_COMPRESS, L"새로 압축", IC_ARCHIVE, BS_ACCENT}, {ID_SPLIT, L"분할 압축", IC_SPLIT, BS_TOOL},
                                                                 {ID_EXTRACT_ALL, L"압축 해제", IC_EXTRACT, BS_TOOL}, {ID_BROWSE, L"내 PC", IC_DRIVE, BS_TOOL}};
    for (auto& b : tb) {
        int w = BtnW(b.t, b.st == BS_ACCENT ? g_fontBold : g_font, true) + S(4);
        AddB(b.id, {x, by, x + w, by + bh}, b.t, b.ic, b.st, b.id == ID_BROWSE || !busy, b.id == ID_BROWSE && g_fs);
        x += w + S(4);
    }
    L.groups.push_back({{x + S(6), by + S(9), x + S(7), by + bh - S(9)}, L""});  // 구분선
    x += S(16);
    int w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {x, by, x + w, by + bh}, L"파일 추가", IC_PLUS, BS_TOOL, g_hasArc && !busy && !g_arc.readOnly); x += w + S(4);
    w = BtnW(L"선택 추출", g_font, true);
    AddB(ID_EXTRACT_SEL, {x, by, x + w, by + bh}, L"선택 추출", IC_DOWNLOAD, BS_TOOL, g_hasArc && !busy && HasRealSel());
    int rx = L.top.right - S(10);
    AddB(ID_THEME, {rx - bh, by, rx, by + bh}, L"", IC_THEME, BS_TOOL, true); rx -= bh + S(2);
    AddB(ID_SETTINGS, {rx - bh, by, rx, by + bh}, L"", IC_GEAR, BS_TOOL, true);
    L.side = {S(16), S(88), S(236), H - S(16)};
    L.groups.push_back({{L.side.left + S(14), L.side.top + S(12), L.side.right - S(10), L.side.top + S(30)}, g_fs ? L"내 PC" : L"이 압축 파일"});
    L.tree = {L.side.left + S(8), L.side.top + S(34), L.side.right - S(8), L.side.bottom - S(8)};
    L.treeRowH = S(32);
    L.panel = {L.side.right + S(12), S(88), W - S(16), H - S(16)};
    RECT& p = L.panel;
    TitleRow(p.left + S(20), p.right - S(20), p.top + S(14), S(36), true, min(S(250), max(S(140), (int)(p.right - p.left) / 3)), false);
    L.sub = {L.title.left, p.top + S(52), p.right - S(20), p.top + S(70)};
    L.status = {p.left + S(20), p.bottom - S(32), p.right - S(20), p.bottom - S(8)};
    L.inset = S(10); L.rowH = S(40); L.rowRad = SF(10); L.padTop = 0;
    if (HasList()) {
        L.header = {p.left + S(10), p.top + S(80), p.right - S(10), p.top + S(110)};
        L.list = {p.left + S(10), L.header.bottom, p.right - S(10), L.status.top - S(4)};
    } else L.list = {p.left + S(10), p.top + S(80), p.right - S(10), L.status.top - S(4)};
}
// 5) 별밤: 밤하늘(라이트는 새벽 하늘)에 별, 동그란 버튼이 별자리처럼 이어지고 금빛 강조
static void LayoutByeol(int W, int H, bool busy) {
    L.side = {S(18), S(18), S(168), H - S(18)};
    int cx = (L.side.left + L.side.right) / 2, y = L.side.top + S(10);
    struct { int id; const wchar_t* t; int ic; } ob[] = {{ID_COMPRESS, L"새로 압축", IC_ARCHIVE}, {ID_SPLIT, L"분할 압축", IC_SPLIT},
                                                         {ID_EXTRACT_ALL, L"압축 해제", IC_EXTRACT}, {ID_BROWSE, L"내 PC", IC_DRIVE}};
    int step = min(S(100), max(S(80), (int)(L.side.bottom - L.side.top - S(150)) / 4));
    for (int i = 0; i < 4; i++) {
        AddB(ob[i].id, {cx - S(50), y, cx + S(50), y + S(54) + S(24)}, ob[i].t, ob[i].ic, BS_ORB, ob[i].id == ID_BROWSE || !busy,
             (i == 0 && !g_fs) || (ob[i].id == ID_BROWSE && g_fs));
        y += step;
    }
    int sb = S(40);
    AddB(ID_THEME, {cx - sb / 2, L.side.bottom - sb, cx + sb / 2, L.side.bottom}, L"", IC_THEME, BS_ORBSM, true);
    AddB(ID_SETTINGS, {cx - sb / 2, L.side.bottom - 2 * sb - S(10), cx + sb / 2, L.side.bottom - sb - S(10)}, L"", IC_GEAR, BS_ORBSM, true);
    L.panel = {L.side.right + S(12), S(18), W - S(18), H - S(18)};
    RECT& p = L.panel;
    int by = p.top + S(20), bh = S(34), x = p.right - S(26);
    int w = BtnW(L"선택 추출", g_fontBold, true);
    AddB(ID_EXTRACT_SEL, {x - w, by, x, by + bh}, L"선택 추출", IC_DOWNLOAD, BS_ACCENT, g_hasArc && !busy && HasRealSel()); x -= w + S(8);
    w = BtnW(L"파일 추가", g_font, true);
    AddB(ID_ADD, {x - w, by, x, by + bh}, L"파일 추가", IC_PLUS, BS_OUTLINE, g_hasArc && !busy && !g_arc.readOnly); x -= w + S(8);
    TitleRow(p.left + S(26), x, by, bh, true, min(S(220), max(S(130), (x - (int)p.left) / 3)), true);
    L.sub = {L.title.left, by + bh + S(4), p.right - S(26), by + bh + S(22)};
    L.status = {p.left + S(26), p.bottom - S(32), p.right - S(26), p.bottom - S(8)};
    L.inset = S(6); L.rowH = S(44); L.rowRad = SF(12); L.padTop = S(2);
    if (HasList()) {
        L.header = {p.left + S(20), p.top + S(98), p.right - S(20), p.top + S(128)};
        L.list = {p.left + S(20), L.header.bottom, p.right - S(20), L.status.top - S(4)};
    } else L.list = {p.left + S(20), p.top + S(98), p.right - S(20), L.status.top - S(4)};
}

static void BuildTreeRows() {
    L.trows.clear();
    if (g_fs && L.tree.right > L.tree.left) {
        int rh = L.treeRowH, total = (int)g_places.size();
        g_treeScroll = max(0, min(g_treeScroll, (int)(total * rh - (L.tree.bottom - L.tree.top))));
        int y = L.tree.top - g_treeScroll;
        for (int i = 0; i < total; i++, y += rh) L.trows.push_back({i, 0, {L.tree.left, y, L.tree.right, y + rh}, false});
        return;
    }
    if (!g_hasArc || L.tree.right <= L.tree.left) return;
    int rh = L.treeRowH, total = 0;
    std::function<void(int)> cnt = [&](int n) { total++; if (g_expand[(size_t)n]) for (int c : g_dirs[(size_t)n].dirs) cnt(c); };
    cnt(0);
    g_treeScroll = max(0, min(g_treeScroll, (int)(total * rh - (L.tree.bottom - L.tree.top))));
    int y = L.tree.top - g_treeScroll;
    std::function<void(int, int)> add = [&](int n, int d) {
        L.trows.push_back({n, d, {L.tree.left, y, L.tree.right, y + rh}, !g_dirs[(size_t)n].dirs.empty()});
        y += rh;
        if (g_expand[(size_t)n]) for (int c : g_dirs[(size_t)n].dirs) add(c, d + 1);
    };
    add(0, 0);
}

static bool QueueAllowed();
// 최근 연 파일 (레지스트리 Recent, 줄바꿈으로 구분)
static std::wstring RegReadStr(const std::wstring& key, const wchar_t* name);
static void RegStr(const std::wstring& key, const wchar_t* name, const std::wstring& val);
static std::vector<std::wstring> RecentList() {
    std::vector<std::wstring> v;
    std::wstring all = RegReadStr(REG_KEY, L"Recent"), cur;
    for (wchar_t c : all) { if (c == L'\n') { if (!cur.empty()) v.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) v.push_back(cur);
    return v;
}
static void AddRecent(const std::wstring& p) {
    std::vector<std::wstring> v = RecentList(), out{p};
    for (auto& x : v) if (lstrcmpiW(x.c_str(), p.c_str()) && out.size() < 8) out.push_back(x);
    std::wstring all;
    for (auto& x : out) all += x + L"\n";
    RegStr(REG_KEY, L"Recent", all);
}

static void DoLayout() {
    RECT cr;
    GetClientRect(g_hwnd, &cr);
    int W = cr.right, H = cr.bottom;
    L = Layout();
    bool busy = Busy() && !QueueAllowed();  // 대기열에 넣을 수 있으면 버튼을 살려 둔다
    switch (g_ly) {
    case TH_BASIC: LayoutBasic(W, H, busy); break;
    case TH_MIDNIGHT: LayoutMidnight(W, H, busy); break;
    case TH_PASTEL: LayoutPastel(W, H, busy); break;
    case TH_NORDIC: case TH_NAVY: LayoutTreeSide(W, H, busy); break;
    case TH_CHAGOK: LayoutChagok(W, H, busy); break;
    case TH_SHIM: LayoutShim(W, H, busy); break;
    case TH_BOM: LayoutBom(W, H, busy); break;
    case TH_MUL: LayoutMul(W, H, busy); break;
    case TH_BYEOL: LayoutByeol(W, H, busy); break;
    default: LayoutMint(W, H, busy); break;
    }
    BuildTreeRows();
    if (!HasList()) {
        // 빈 화면: 안내 + 열기·둘러보기 버튼 + 최근 연 파일
        std::vector<std::wstring> rec;
        for (auto& p : RecentList()) if (FileExists(p) && rec.size() < 6) rec.push_back(p);
        int extra = rec.empty() ? 0 : S(40) + (int)rec.size() * S(32);
        int cx = (L.list.left + L.list.right) / 2, cy = (L.list.top + L.list.bottom) / 2 - extra / 2;
        cy = max(cy, (int)L.list.top + S(90));
        L.emptyY = cy;
        int w = BtnW(L"압축 파일 열기", g_font, true), w2 = BtnW(L"내 PC 둘러보기", g_font, true), gap = S(10);
        int bx = cx - (w + gap + w2) / 2;
        AddB(ID_OPEN, {bx, cy + S(44), bx + w, cy + S(44) + S(36)}, L"압축 파일 열기", IC_OPEN, BS_OUTLINE, !Busy());
        AddB(ID_BROWSE, {bx + w + gap, cy + S(44), bx + w + gap + w2, cy + S(44) + S(36)}, L"내 PC 둘러보기", IC_FOLDER, BS_OUTLINE, true);
        int rw = min(S(440), (int)(L.list.right - L.list.left) - S(40)), y = cy + S(44) + S(36) + S(40);
        for (size_t i = 0; i < rec.size(); i++, y += S(32))
            if (y + S(30) < L.list.bottom) AddB(ID_RECENT + (int)i, {cx - rw / 2, y, cx + rw / 2, y + S(30)}, rec[i], IC_ARCHIVE, BS_RECENT, !Busy());
    }
    // 검색 상자 (실제 입력은 EDIT 컨트롤, 테두리·아이콘은 직접 그림)
    if (g_searchEdit) {
        // 안내 글: 상자 폭에 맞게 줄인다 (잘려 보이지 않게)
        static std::wstring cueNow;
        int ew = L.search.right - L.search.left - S(32) - S(30) - S(6);
        const wchar_t* cands[] = {g_fs ? L"이 폴더에서 찾기 (Ctrl+F)" : L"이 압축 파일에서 찾기 (Ctrl+F)", g_fs ? L"이 폴더에서 찾기" : L"이 압축 파일에서 찾기", L"찾기 (Ctrl+F)", L"찾기"};
        std::wstring cue = cands[3];
        for (auto c : cands) if (TextW(c, g_font) <= ew) { cue = c; break; }
        if (cue != cueNow) { cueNow = cue; SendMessageW(g_searchEdit, 0x1501 /*EM_SETCUEBANNER*/, FALSE, (LPARAM)cueNow.c_str()); }
        if (L.search.right > L.search.left) {
            RECT r = L.search;
            int eh = S(20);
            if (g_searchFont != g_font) { SendMessageW(g_searchEdit, WM_SETFONT, (WPARAM)g_font, FALSE); g_searchFont = g_font; }
            SetWindowPos(g_searchEdit, nullptr, r.left + S(32), (r.top + r.bottom - eh) / 2, r.right - r.left - S(32) - S(30), eh,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            if (!g_search.empty()) AddB(ID_SEARCHCLR, {r.right - S(28), r.top + S(3), r.right - S(4), r.bottom - S(3)}, L"", IC_X, BS_ICONBTN, true);
        } else ShowWindow(g_searchEdit, SW_HIDE);
    }
    if (Busy() && g_progHidden) {
        int w = S(300);
        AddB(ID_SHOWPROG, {L.list.right - S(16) - w, L.list.bottom - S(14) - S(44), L.list.right - S(16), L.list.bottom - S(14)}, L"", IC_NONE, BS_PROGPILL, true);
    }
    ClampScroll();
    int ch = ContentH(), lh = ListH();
    L.hasSb = HasList() && ch > lh && lh > 0;
    if (L.hasSb) {
        int inset = S(4);
        L.sbTrack = {L.list.right - S(10) - inset + S(2), L.list.top + S(2), L.list.right - inset, L.list.bottom - S(2)};
        int th = L.sbTrack.bottom - L.sbTrack.top;
        int thumb = max(S(28), (int)((int64_t)th * lh / ch));
        int pos = (int)((int64_t)(th - thumb) * g_scrollY / max(1, ch - lh));
        L.sbThumb = {L.sbTrack.left, L.sbTrack.top + pos, L.sbTrack.right, L.sbTrack.top + pos + thumb};
        if (L.tiles) {  // 스크롤바 자리만큼 타일 열 다시 계산
            int avail = L.list.right - L.list.left - L.gap - S(16);
            L.cols = max(1, avail / (S(150) + L.gap));
            L.tileW = min(S(200), avail / L.cols - L.gap);
        }
    }
}

// 열 위치: 이름/크기/압축/날짜
struct Cols { int name1, name2, size2, csize2, date1, date2; bool date; };
static Cols GetCols() {
    Cols c{};
    int right = L.list.right - L.inset - S(14) - (L.hasSb ? S(10) : 0);
    int w = right - L.list.left - L.inset;
    c.date = w > S(470);
    c.date2 = right; c.date1 = c.date ? right - S(128) : right;
    c.csize2 = c.date1 - S(14); c.size2 = c.csize2 - S(84);
    c.name1 = L.list.left + L.inset + S(12); c.name2 = c.size2 - S(84) - S(8);
    return c;
}

// ============================================================ 그리기
static int g_hoverId = 0, g_pressId = 0;
static bool g_dragThumb = false;
static int g_dragY0 = 0, g_dragScroll0 = 0;

static float BtnRad(const Btn& b) {
    int h = b.rc.bottom - b.rc.top;
    switch (g_ly) {
    case TH_BASIC: return SF(4);
    case TH_MIDNIGHT: return SF(8);
    case TH_PASTEL: return (float)h / 2;
    case TH_NORDIC: return SF(6);
    case TH_NAVY: return SF(4);
    case TH_CHAGOK: return SF(9);
    case TH_SHIM: case TH_BOM: case TH_BYEOL: return (float)h / 2;
    case TH_MUL: return SF(12);
    default: return b.style == BS_PILL ? (float)h / 2 : SF(8);
    }
}

static void DrawProgPill(Graphics& g, HDC dc, const Btn& b);
static void DrawBtn(Graphics& g, HDC dc, const Btn& b) {
    bool hov = b.enabled && g_hoverId == b.id && b.id;
    bool prs = hov && g_pressId == b.id;
    COLORREF fg = b.enabled ? P.text : P.dim;
    RECT r = b.rc;
    float rad = BtnRad(b);
    int ic = S(16);
    auto iconText = [&](COLORREF c, HFONT f, COLORREF icCol, bool left = false) {
        int tw = b.text.empty() ? 0 : TextW(b.text, f);
        int total = (b.icon ? ic + (tw ? S(7) : 0) : 0) + tw;
        int x = left ? r.left + S(14) : (r.left + r.right - total) / 2;
        if (b.icon) DrawIco(g, b.icon, (REAL)x, (REAL)(r.top + r.bottom - ic) / 2, (REAL)ic, icCol);
        x += b.icon ? ic + S(7) : 0;
        if (tw) Txt(dc, {x, r.top, r.right - S(4), r.bottom}, b.text, c, f);
    };
    switch (b.style) {
    case BS_LABEL: Txt(dc, r, b.text, P.dim, b.small ? g_fontSmall : g_font); break;
    case BS_SEP: Txt(dc, r, b.text, P.dim, b.small ? g_fontSmall : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE); break;
    case BS_CRUMB: {
        COLORREF c = b.current ? P.text : (hov ? P.accentFg : P.dim);
        Txt(dc, r, b.text, c, b.small ? g_fontSmall : (b.current ? g_fontBold : g_font), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case BS_PRIMARY: {
        if (!b.enabled) FillR(g, r, rad, Mix(SideBg(), P.sideDim, 0.25f));
        else if (hov) FillG(g, r, rad, Mix(P.primary, RGB(255, 255, 255), 0.12f), P.primary2 == NONE ? NONE : Mix(P.primary2, RGB(255, 255, 255), 0.12f));
        else FillG(g, r, rad, P.primary, P.primary2);
        COLORREF c = b.enabled ? P.onPrimary : P.sideDim;
        int isz = S(20);
        DrawIco(g, b.icon, (REAL)r.left + S(14), (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, c);
        Txt(dc, {r.left + S(14) + isz + S(12), r.top, r.right, r.bottom}, b.text, c, g_fontBold);
        break;
    }
    case BS_SIDE: {
        if (hov) FillR(g, r, rad, prs ? P.sideSel : P.sideHover);
        COLORREF c = b.enabled ? P.sideText : P.sideDim;
        int isz = S(20);
        DrawIco(g, b.icon, (REAL)r.left + S(14), (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, c, NONE, 1.6f);
        Txt(dc, {r.left + S(14) + isz + S(12), r.top, r.right, r.bottom}, b.text, c, g_font);
        break;
    }
    case BS_OUTLINE: case BS_OUTLINE2: {
        bool o2 = b.style == BS_OUTLINE2;
        COLORREF bg = hov ? (prs ? Mix(P.hover, P.text, 0.08f) : P.hover) : P.btn;
        if (o2) bg = hov ? P.hover : P.panel;
        FillR(g, r, rad, bg);
        StrokeR(g, r, rad, o2 ? (b.enabled ? P.accentFg : P.border) : P.btnBorder, o2 ? SF(1.2f) : 1);
        COLORREF c = o2 ? (b.enabled ? P.accentFg : P.dim) : fg;
        iconText(c, o2 ? g_fontBold : g_font, c);
        break;
    }
    case BS_ACCENT: case BS_PILL: {
        COLORREF a1 = P.accent, a2 = P.accent2;
        if (!b.enabled) { a1 = Mix(P.panel, P.dim, 0.22f); a2 = NONE; }
        else if (hov) { a1 = Mix(a1, g_dark ? RGB(255, 255, 255) : RGB(0, 0, 0), 0.1f); if (a2 != NONE) a2 = Mix(a2, g_dark ? RGB(255, 255, 255) : RGB(0, 0, 0), 0.1f); }
        FillG(g, r, rad, a1, a2);
        COLORREF c = b.enabled ? P.onAccent : P.dim;
        COLORREF icc = (b.enabled && g_ly == TH_NAVY && !g_dark) ? P.primary : c;  // 네이비 버튼의 금색 아이콘
        iconText(c, g_fontBold, icc);
        break;
    }
    case BS_ICONBTN: {
        if (hov) FillR(g, r, SF(6), prs ? Mix(P.hover, P.text, 0.08f) : P.hover);
        int isz = S(18);
        DrawIco(g, b.icon, (REAL)(r.left + r.right - isz) / 2, (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, fg);
        break;
    }
    case BS_TOOL: {
        if (hov) FillR(g, r, SF(8), prs ? Mix(P.hover, P.text, 0.08f) : P.hover);
        iconText(fg, g_font, b.enabled ? P.text : P.dim);
        break;
    }
    case BS_RIBBON: {
        if (hov) FillR(g, r, SF(4), prs ? P.sel : P.hover);
        int isz = S(26);
        DrawIco(g, b.icon, (REAL)(r.left + r.right - isz) / 2, (REAL)r.top + S(8), (REAL)isz, b.enabled ? P.accentFg : P.dim, NONE, 1.7f);
        Txt(dc, {r.left, r.top + S(38), r.right, r.bottom - S(4)}, b.text, fg, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case BS_TAB: {
        if (b.current) {
            RECT t = r; t.bottom += S(8);
            FillR(g, t, SF(6), P.panel);
            Txt(dc, r, b.text, P.accentFg, g_fontBold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            if (hov) { RECT t = r; t.top += S(2); t.bottom -= S(3); FillR(g, t, SF(5), Mix(P.band, RGB(255, 255, 255), 0.16f)); }
            Txt(dc, r, b.text, P.onBand, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        break;
    }
    case BS_VIEWTOG: {
        FillR(g, r, SF(4), b.current ? P.sel : (hov ? P.hover : P.btn));
        StrokeR(g, r, SF(4), b.current ? P.selBorder : P.btnBorder);
        int isz = S(14);
        DrawIco(g, b.icon, (REAL)r.left + S(9), (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, b.current ? P.accentFg : P.dim);
        Txt(dc, {r.left + S(9) + isz + S(6), r.top, r.right - S(4), r.bottom}, b.text, P.text, g_fontSmall);
        break;
    }
    case BS_RAIL: {
        if (b.current || hov) FillR(g, r, SF(16), prs || b.current ? P.sideSel : P.sideHover);
        COLORREF c = !b.enabled ? P.sideDim : (b.current || hov) ? P.sideSelText : P.sideText;
        int isz = S(22);
        DrawIco(g, b.icon, (REAL)(r.left + r.right - isz) / 2, (REAL)r.top + S(11), (REAL)isz, c);
        Txt(dc, {r.left, r.top + S(38), r.right, r.bottom - S(4)}, b.text, c, g_fontTiny, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case BS_GRID: {
        FillR(g, r, SF(8), hov ? P.sideHover : P.btn);
        StrokeR(g, r, SF(8), b.current ? P.accent : P.btnBorder, b.current ? SF(1.4f) : 1);
        COLORREF c = b.enabled ? P.sideText : P.sideDim;
        int isz = S(20);
        DrawIco(g, b.icon, (REAL)(r.left + r.right - isz) / 2, (REAL)r.top + S(10), (REAL)isz, c);
        Txt(dc, {r.left, r.top + S(34), r.right, r.bottom - S(4)}, b.text, c, g_fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case BS_PROGPILL: DrawProgPill(g, dc, b); break;
    case BS_LAYER: {  // 차곡: 층층이 쌓인 버튼 (왼쪽 색 띠)
        bool top = b.id == ID_COMPRESS;
        COLORREF bg = top ? (hov ? Mix(P.primary, RGB(255, 255, 255), 0.1f) : P.primary) : b.current ? P.sel : hov ? P.hover : P.panel;
        FillR(g, r, SF(10), bg);
        if (!top) StrokeR(g, r, SF(10), P.border);
        COLORREF band = b.id == ID_SPLIT ? P.accent : b.id == ID_EXTRACT_ALL ? P.primary : P.folderFill;
        GraphicsPath clip;
        RoundPath(clip, RF(r), SF(10));
        Region rg(&clip);
        g.SetClip(&rg);
        FillR(g, {r.left, r.top, r.left + S(6), r.bottom}, 0, band);
        g.ResetClip();
        COLORREF c = !b.enabled ? P.dim : top ? P.onPrimary : P.text;
        int isz = S(18);
        DrawIco(g, b.icon, (REAL)r.left + S(18), (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, c);
        Txt(dc, {r.left + S(18) + isz + S(10), r.top, r.right - S(6), r.bottom}, b.text, c, g_fontBold);
        break;
    }
    case BS_DOCK: case BS_DOCKACC: {  // 쉼: 아래 작업 줄
        bool acc = b.style == BS_DOCKACC;
        COLORREF bg = acc ? (hov ? Mix(P.accent, RGB(255, 255, 255), 0.12f) : P.accent) : hov ? Mix(P.primary, P.onPrimary, 0.14f) : NONE;
        FillR(g, r, (float)(r.bottom - r.top) / 2, bg);
        COLORREF c = acc ? P.onAccent : b.enabled ? P.onPrimary : Mix(P.primary, P.onPrimary, 0.45f);
        if (acc && !b.enabled) c = Mix(P.accent, P.onAccent, 0.5f);
        iconText(c, g_fontBold, c);
        break;
    }
    case BS_HERO: case BS_HEROW: {  // 봄빛: 그라데이션 띠 위의 단추
        float rr = (float)(r.bottom - r.top) / 2;
        if (b.style == BS_HEROW) FillR(g, r, rr, hov ? Mix(P.primary, P.accent, 0.08f) : P.primary);
        else FillRA(g, r, rr, RGB(0, 0, 0), b.current ? 70 : hov ? 56 : 34);
        COLORREF c = b.style == BS_HEROW ? P.onPrimary : P.onBand;
        if (!b.enabled) c = b.style == BS_HEROW ? Mix(P.primary, P.onPrimary, 0.5f) : Mix(P.accent, P.onBand, 0.6f);
        iconText(c, g_fontBold, c);
        break;
    }
    case BS_HEROICON: {
        if (hov) FillRA(g, r, SF(8), RGB(0, 0, 0), 40);
        int isz = S(20);
        DrawIco(g, b.icon, (REAL)(r.left + r.right - isz) / 2, (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, P.onBand, NONE, 2.0f);
        break;
    }
    case BS_ORB: case BS_ORBSM: {  // 별밤: 동그란 별 단추 (아래에 이름)
        bool sm = b.style == BS_ORBSM;
        int d = sm ? (int)(r.bottom - r.top) : S(54), cx = (r.left + r.right) / 2;
        RECT c{cx - d / 2, r.top, cx + d / 2, r.top + d};
        bool on = b.current && b.enabled;
        if (on) {
            FillRA(g, {c.left - S(6), c.top - S(6), c.right + S(6), c.bottom + S(6)}, (float)(d + S(12)) / 2, P.accent, 46);
            FillG(g, c, (float)d / 2, P.accent, P.accent2, 135);
        } else {
            FillR(g, c, (float)d / 2, hov ? P.sideHover : P.side);
            StrokeR(g, c, (float)d / 2, P.border);
        }
        COLORREF ic2 = !b.enabled ? P.sideDim : on ? P.onAccent : P.sideText;
        int isz = sm ? S(18) : S(22);
        DrawIco(g, b.icon, (REAL)cx - isz / 2, (REAL)(c.top + c.bottom - isz) / 2, (REAL)isz, ic2);
        if (!sm) Txt(dc, {r.left - S(10), c.bottom + S(4), r.right + S(10), r.bottom}, b.text, b.enabled ? P.text : P.dim, g_fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case BS_RECENT: {
        if (hov) FillR(g, r, SF(6), prs ? P.sel : P.hover);
        int isz = S(16);
        DrawIco(g, IC_ARCHIVE, (REAL)r.left + S(10), (REAL)(r.top + r.bottom - isz) / 2, (REAL)isz, P.folderStroke, P.folderFill, 1.6f);
        int nw = min(TextW(FileNameOf(b.text), g_font), (int)(r.right - r.left) / 2);
        Txt(dc, {r.left + S(34), r.top, r.left + S(34) + nw, r.bottom}, FileNameOf(b.text), P.text, g_font);
        Txt(dc, {r.left + S(44) + nw, r.top, r.right - S(10), r.bottom}, ParentOf(b.text), P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        break;
    }
    }
}

static void DrawTree(Graphics& g, HDC dc) {
    if (L.trows.empty()) return;
    HRGN clip = CreateRectRgn(L.tree.left, L.tree.top, L.tree.right, L.tree.bottom);
    SelectClipRgn(dc, clip);
    Region gr(Rect(L.tree.left, L.tree.top, L.tree.right - L.tree.left, L.tree.bottom - L.tree.top));
    g.SetClip(&gr);
    for (size_t i = 0; i < L.trows.size(); i++) {
        const TreeRow& t = L.trows[i];
        if (t.rc.bottom < L.tree.top || t.rc.top > L.tree.bottom) continue;
        RECT r = t.rc;
        r.top += S(1); r.bottom -= S(1);
        bool cur = g_fs ? !lstrcmpiW(g_places[(size_t)t.node].path.c_str(), g_fsPath.c_str()) : t.node == g_cur;
        if (cur) FillR(g, r, SF(6), P.sideSel);
        else if (g_hoverTree == (int)i) FillR(g, r, SF(6), P.sideHover);
        int x = r.left + S(8) + t.depth * S(16);
        int csz = S(14);
        if (t.kids) DrawIco(g, g_expand[(size_t)t.node] ? IC_CHEVDOWN : IC_CHEVRIGHT, (REAL)x, (REAL)(r.top + r.bottom - csz) / 2, (REAL)csz, P.sideDim, NONE, 2.0f);
        x += csz + S(4);
        int isz = S(16);
        REAL iy = (REAL)(r.top + r.bottom - isz) / 2;
        if (g_fs) {
            const Place& pl = g_places[(size_t)t.node];
            if (pl.icon == IC_DRIVE) DrawIco(g, IC_DRIVE, (REAL)x, iy, (REAL)isz, cur ? P.sideSelText : P.sideDim, NONE, 1.6f);
            else DrawIco(g, IC_FOLDER, (REAL)x, iy, (REAL)isz, P.folderStroke, P.folderFill, 1.4f);
        } else if (t.node == 0) DrawIco(g, IC_ARCHIVE, (REAL)x, iy, (REAL)isz, P.folderStroke, P.folderFill, 1.6f);
        else DrawIco(g, IC_FOLDER, (REAL)x, iy, (REAL)isz, P.folderStroke, P.folderFill, 1.4f);
        x += isz + S(8);
        std::wstring name = g_fs ? g_places[(size_t)t.node].name : t.node == 0 ? FileNameOf(g_arc.path) : g_dirs[(size_t)t.node].name;
        Txt(dc, {x, r.top, r.right - S(6), r.bottom}, name, cur ? P.sideSelText : P.sideText, cur ? g_fontBold : g_font);
    }
    g.ResetClip();
    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);
}

static void DrawRowIcon(Graphics& g, const Row& r, const FsItem* fi, REAL x, REAL y, REAL sz, float sw) {
    if (fi && fi->drive) DrawIco(g, IC_DRIVE, x, y, sz, P.fileStroke, P.fileFill, sw + 0.2f);
    else if (fi && fi->arc) DrawIco(g, IC_ARCHIVE, x, y, sz, P.folderStroke, P.folderFill, sw + 0.2f);
    else if (r.kind == 2) DrawIco(g, IC_FILE, x, y, sz, P.fileStroke, P.fileFill, sw + 0.05f);
    else DrawIco(g, IC_FOLDER, x, y, sz, P.folderStroke, P.folderFill, sw - 0.05f);
}
static void DrawRows(Graphics& g, HDC dc) {
    Cols c = GetCols();
    HRGN clip = CreateRectRgn(L.list.left, L.list.top, L.list.right, L.list.bottom);
    SelectClipRgn(dc, clip);
    Region gr(Rect(L.list.left, L.list.top, L.list.right - L.list.left, L.list.bottom - L.list.top));
    g.SetClip(&gr);
    bool lines = g_ly == TH_NORDIC || g_ly == TH_NAVY || g_ly == TH_MUL;
    bool zebra = P.zebra != NONE;
    for (int i = 0; i < (int)g_rows.size(); i++) {
        RECT rr = ItemRect(i);
        if (rr.bottom < L.list.top) continue;
        if (rr.top > L.list.bottom) break;
        const Row& r = g_rows[(size_t)i];
        bool sel = g_sel[(size_t)i] != 0;
        if (L.tiles) {
            COLORREF bg = sel ? P.sel : (g_hoverRow == i ? P.hover : P.panel);
            FillR(g, rr, SF(8), bg);
            StrokeR(g, rr, SF(8), sel ? P.selBorder : P.border, sel ? SF(1.6f) : 1);
            if (g_focus == i && GetFocus() == g_hwnd && !sel) StrokeR(g, rr, SF(8), P.dim, 1, true);
            int isz = S(42), cx = (rr.left + rr.right) / 2;
            const FsItem* fi = g_fs ? &g_fsItems[(size_t)r.idx] : nullptr;
            DrawRowIcon(g, r, fi, (REAL)cx - isz / 2, (REAL)rr.top + S(20), (REAL)isz, 1.25f);
            Txt(dc, {rr.left + S(8), rr.top + S(72), rr.right - S(8), rr.top + S(94)}, r.name, P.text, g_fontBold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            Txt(dc, {rr.left + S(8), rr.top + S(96), rr.right - S(8), rr.top + S(114)}, fi && fi->drive ? fi->kind : r.kind == 2 ? FmtSize(r.us) : L"폴더", P.dim, g_fontSmall,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            continue;
        }
        RECT box = rr;
        if (L.rowRad > 0) box.bottom -= g_ly == TH_PASTEL || g_ly == TH_CHAGOK ? S(4) : S(2);
        if (sel && g_ly == TH_CHAGOK) { OffsetRect(&box, S(4), 0); OffsetRect(&rr, S(4), 0); }  // 차곡: 고른 줄이 살짝 빠져나옴
        COLORREF bg = NONE;
        if (zebra && (i % 2) == 0) bg = P.zebra;
        if (g_hoverRow == i) bg = P.hover;
        if (sel) bg = P.sel;
        FillR(g, box, L.rowRad, bg);
        if (sel && P.selBorder != NONE && (g_ly == TH_MIDNIGHT || g_ly == TH_BYEOL)) StrokeR(g, box, L.rowRad, P.selBorder, 1);
        if (g_ly == TH_CHAGOK) FillR(g, {box.left + S(8), (box.top + box.bottom) / 2 - S(11), box.left + S(12), (box.top + box.bottom) / 2 + S(11)}, SF(2), r.kind == 1 ? P.folderFill : P.primary);
        if (sel && P.selBar != NONE) { RECT bar{box.left, box.top, box.left + S(3), box.bottom}; FillR(g, bar, 0, P.selBar); }
        if (lines) HLine(g, rr.left, rr.right, rr.bottom - 1, P.lines);
        if (g_focus == i && GetFocus() == g_hwnd && !sel) StrokeR(g, box, L.rowRad, P.dim, 1, true);
        int isz = S(18);
        int ix = c.name1 - S(2);
        int iy = (box.top + box.bottom - isz) / 2;
        const FsItem* fi = g_fs ? &g_fsItems[(size_t)r.idx] : nullptr;
        if (g_ly == TH_CHAGOK) ix += S(8), c.name2 += 0;
        if (sel && g_ly == TH_CHAGOK) ix += S(4);
        int iconW = isz;
        if (g_ly == TH_BOM && r.kind == 2 && !(fi && fi->drive)) {  // 봄빛: 형식별 색 배지
            std::wstring ext = r.name.substr(r.name.find_last_of(L'.') == std::wstring::npos ? r.name.size() : r.name.find_last_of(L'.') + 1);
            CharUpperBuffW(ext.empty() ? nullptr : &ext[0], (DWORD)ext.size());
            if (ext.size() > 4) ext = ext.substr(0, 4);
            if (ext.empty()) ext = L"FILE";
            COLORREF bc = RGB(0x64, 0x74, 0x8b);
            if (ext == L"XLSX" || ext == L"XLS" || ext == L"CSV") bc = RGB(0x15, 0x80, 0x3d);
            else if (ext == L"PDF") bc = RGB(0xc2, 0x26, 0x26);
            else if (ext == L"HWP" || ext == L"HWPX") bc = RGB(0x1d, 0x4e, 0xd8);
            else if (ext == L"DOC" || ext == L"DOCX") bc = RGB(0x43, 0x38, 0xca);
            else if (ext == L"PPT" || ext == L"PPTX") bc = RGB(0xc2, 0x41, 0x0c);
            else if (ext == L"JPG" || ext == L"JPEG" || ext == L"PNG" || ext == L"GIF" || ext == L"BMP" || ext == L"WEBP") bc = RGB(0xbe, 0x18, 0x5d);
            else if (fi && fi->arc) bc = RGB(0xb4, 0x53, 0x09);
            iconW = S(42);
            RECT bd{ix, (box.top + box.bottom) / 2 - S(9), ix + iconW, (box.top + box.bottom) / 2 + S(9)};
            FillR(g, bd, SF(5), bc);
            Txt(dc, bd, ext, RGB(255, 255, 255), g_fontTiny, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            if (g_ly == TH_BOM) ix += S(12);
            DrawRowIcon(g, r, fi, (REAL)ix, (REAL)iy, (REAL)isz, 1.35f);
            if (g_ly == TH_BOM) { ix -= S(12); iconW = S(42); }
        }
        RECT nr{ix + iconW + S(10), box.top, c.name2, box.bottom};
        Txt(dc, nr, r.name, P.text, g_font);
        if (fi) {  // 내 PC: 크기 / 종류
            if (!fi->dir || fi->drive) Txt(dc, {c.size2 - S(84), box.top, c.size2, box.bottom}, fi->size || !fi->drive ? FmtSize(r.us) : L"", P.dim, g_font, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            Txt(dc, {c.csize2 - S(96), box.top, c.csize2, box.bottom}, fi->kind, P.dim, g_font, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else if (r.kind == 2) {
            Txt(dc, {c.size2 - S(84), box.top, c.size2, box.bottom}, FmtSize(r.us), P.dim, g_font, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            if (r.cs != UINT64_MAX) Txt(dc, {c.csize2 - S(84), box.top, c.csize2, box.bottom}, FmtSize(r.cs), P.dim, g_font, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        if (r.hasDate && c.date) Txt(dc, {c.date1, box.top, c.date2, box.bottom}, FmtDate(r.d, r.t), P.dim, g_font);
    }
    g.ResetClip();
    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);
    if (L.hasSb) {
        bool hov = g_hoverId == -2 || g_dragThumb;
        RECT t = L.sbThumb;
        if (!hov) { t.left += S(2); t.right -= S(2); }
        FillR(g, t, SF(4), P.scroll);
    }
}

static void DrawHeader(Graphics& g, HDC dc) {
    if (!HasList() || L.header.bottom <= L.header.top || L.tiles) return;
    Cols c = GetCols();
    RECT h = L.header;
    HFONT f = g_fontSmall;
    Txt(dc, {c.name1 - S(2), h.top, c.name2, h.bottom}, L"이름", P.dim, f);
    Txt(dc, {c.size2 - S(84), h.top, c.size2, h.bottom}, L"크기", P.dim, f, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    const wchar_t* col2 = g_fs ? L"종류" : L"압축 후";
    Txt(dc, {c.csize2 - S(84), h.top, c.csize2, h.bottom}, col2, P.dim, f, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    if (c.date) Txt(dc, {c.date1, h.top, c.date2, h.bottom}, L"수정한 날짜", P.dim, f);
    {   // 정렬 표시
        int ax = 0, asz = S(12);
        switch (g_sortCol) {
        case 0: ax = c.name1 - S(2) + TextW(L"이름", f) + S(4); break;
        case 1: ax = c.size2 - TextW(L"크기", f) - S(4) - asz; break;
        case 2: ax = c.csize2 - TextW(col2, f) - S(4) - asz; break;
        case 3: ax = c.date ? c.date1 + TextW(L"수정한 날짜", f) + S(4) : 0; break;
        }
        if (ax) DrawIco(g, g_sortDesc ? IC_CHEVDOWN : IC_CHEVUP, (REAL)ax, (REAL)(h.top + h.bottom - asz) / 2, (REAL)asz, P.accentFg, NONE, 2.2f);
    }
    if (g_ly == TH_BASIC || g_ly == TH_NORDIC || g_ly == TH_NAVY || g_ly == TH_CHAGOK || g_ly == TH_BOM || g_ly == TH_MUL || g_ly == TH_BYEOL)
        HLine(g, h.left + L.inset, h.right - L.inset, h.bottom - 1, P.lines);
}

static void DrawEmpty(Graphics& g, HDC dc) {
    int cx = (L.list.left + L.list.right) / 2, cy = L.emptyY ? L.emptyY : (L.list.top + L.list.bottom) / 2;
    bool anyRecent = false;
    for (auto& b : L.btns) if (b.style == BS_RECENT) { if (!anyRecent) Txt(dc, {b.rc.left, b.rc.top - S(28), b.rc.right, b.rc.top - S(4)}, L"최근 연 파일", P.dim, g_fontSmall); anyRecent = true; }
    int isz = S(48);
    DrawIco(g, IC_ARCHIVE, (REAL)cx - isz / 2, (REAL)cy - S(70), (REAL)isz, P.dim, NONE, 1.4f);
    Txt(dc, {L.list.left + S(16), cy - S(14), L.list.right - S(16), cy + S(10)}, L"압축 파일을 여기로 끌어다 놓으세요", P.text, g_fontBold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    Txt(dc, {L.list.left + S(16), cy + S(12), L.list.right - S(16), cy + S(34)}, L"다른 파일이나 폴더를 놓으면 새로 압축합니다", P.dim, g_fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 진행률(백그라운드 작업 중 목록 위에 뜨는 작은 막대). 누르면 진행 창을 다시 연다
static int JobPercent() {
    if (!g_job) return 0;
    uint64_t done = g_job->job.done, total = g_job->job.total;
    return total ? (int)min<uint64_t>(100, done * 100 / total) : 0;
}
static void DrawProgPill(Graphics& g, HDC dc, const Btn& b) {
    if (!g_job) return;
    RECT r = b.rc;
    bool hov = g_hoverId == b.id;
    FillR(g, r, SF(10), hov ? Mix(P.panel, P.text, 0.06f) : P.panel);
    StrokeR(g, r, SF(10), P.border);
    int pct = JobPercent();
    std::wstring t = std::wstring(JobName()) + L" 중 " + std::to_wstring(pct) + L"%";
    Txt(dc, {r.left + S(14), r.top + S(4), r.right - S(14), r.top + S(24)}, t, P.text, g_fontBold);
    Txt(dc, {r.left + S(14), r.top + S(4), r.right - S(14), r.top + S(24)}, L"진행 창 보기", P.accentFg, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    RECT bar{r.left + S(14), r.bottom - S(13), r.right - S(14), r.bottom - S(8)};
    FillR(g, bar, SF(2.5f), P.track);
    RECT f = bar; f.right = bar.left + (bar.right - bar.left) * pct / 100;
    if (f.right > f.left + S(4)) FillG(g, f, SF(2.5f), P.accent, P.accent2);
}

static void DrawChip(Graphics& g, HDC dc, int& x, int y, int h, const std::wstring& t, COLORREF bg, COLORREF fg) {
    int w = TextW(t, g_fontSmall) + S(22);
    FillR(g, {x, y, x + w, y + h}, (float)h / 2, bg);
    Txt(dc, {x, y, x + w, y + h}, t, fg, g_fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    x += w + S(8);
}

static void DrawInfoPanel(Graphics& g, HDC dc) {
    RECT r = L.info;
    InfoView v = GetInfo();
    int cx = (r.left + r.right) / 2;
    int isz = S(64);
    if (v.icon == IC_FOLDER) DrawIco(g, IC_FOLDER, (REAL)cx - isz / 2, (REAL)r.top + S(30), (REAL)isz, P.folderStroke, P.folderFill, 1.1f);
    else DrawIco(g, v.icon, (REAL)cx - isz / 2, (REAL)r.top + S(30), (REAL)isz, P.accentFg, Mix(P.panel, P.accentFg, 0.08f), 1.3f);
    Txt(dc, {r.left + S(14), r.top + S(104), r.right - S(14), r.top + S(132)}, v.name, P.text, g_fontTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    int y = r.top + S(146);
    for (auto& kv : v.rows) {
        RECT rr{r.left + S(18), y, r.right - S(18), y + S(30)};
        Txt(dc, rr, kv.first, P.dim, g_font);
        Txt(dc, rr, kv.second, P.text, g_font, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        Pen pen(GC(P.lines), 1);
        REAL d[] = {3, 3}; pen.SetDashPattern(d, 2);
        g.DrawLine(&pen, (REAL)rr.left, (REAL)rr.bottom - 0.5f, (REAL)rr.right, (REAL)rr.bottom - 0.5f);
        y += S(30);
    }
}

// 고유 배치 5종 배경·판·글자 (버튼과 목록은 공통 코드가 그림)
static std::wstring SubLine() {
    if (g_fs) return g_fsPath.empty() ? L"자주 쓰는 폴더와 드라이브" : g_fsPath;
    if (g_hasArc) return ArcSummary();
    return L"압축 파일을 열거나 끌어다 놓으세요";
}
static std::wstring StatusLeft() {
    if (!HasList()) return L"";
    std::wstring l = SelSummary();
    return l.empty() ? std::to_wstring(g_rows.size()) + L"개 항목" : l + L" · " + std::to_wstring(g_rows.size()) + L"개 항목";
}
static void PaintNew(Graphics& g, HDC dc, int W, int H) {
    RECT all{0, 0, W, H};
    std::wstring ratio = g_hasArc ? L"압축률 " + FmtRatio(g_totU, g_totC) : L"";
    switch (g_ly) {
    case TH_CHAGOK: {
        FillR(g, all, 0, P.bg);
        FillR(g, L.side, 0, P.side);
        for (auto& gp : L.groups) Txt(dc, gp.rc, gp.label, P.sideDim, g_fontTiny);
        DrawTree(g, dc);
        if (!HasList()) Txt(dc, {L.tree.left + S(10), L.tree.top, L.tree.right, L.tree.top + S(30)}, L"열린 파일 없음", P.sideDim, g_font);
        Txt(dc, L.title, DirTitle(), P.text, g_fontBig);
        Txt(dc, L.sub, SubLine(), P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        // 차곡차곡 쌓인 카드: 아래에 두 겹이 비쳐 보임
        RECT c = L.card;
        FillR(g, {c.left + S(8), c.top + S(8), c.right - S(8), c.bottom + S(8)}, SF(14), Mix(P.bg, P.border, 0.7f));
        FillR(g, {c.left + S(4), c.top + S(4), c.right - S(4), c.bottom + S(4)}, SF(14), Mix(P.bg, P.border, 1.0f));
        FillR(g, c, SF(14), P.panel);
        StrokeR(g, c, SF(14), P.border);
        // 아래: 상태 + 쌓인 색 막대 (색 부분 = 압축 후 크기, 빈 부분 = 줄어든 만큼)
        RECT st = L.status;
        std::wstring l = StatusLeft();
        int lw = TextW(l, g_fontSmall), rw = TextW(ratio, g_fontSmall);
        Txt(dc, {st.left, st.top, st.left + lw + S(4), st.bottom}, l, P.dim, g_fontSmall);
        Txt(dc, {st.right - rw - S(4), st.top, st.right, st.bottom}, ratio, P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        if (g_hasArc && g_totU) {
            RECT bar{st.left + lw + S(16), (st.top + st.bottom) / 2 - S(5), st.right - rw - S(16), (st.top + st.bottom) / 2 + S(5)};
            if (bar.right - bar.left > S(60)) {
                FillR(g, bar, SF(5), P.track);
                double f = min(1.0, (double)g_totC / g_totU);
                int fw = (int)((bar.right - bar.left) * f), x = bar.left;
                COLORREF cs[3] = {P.primary, P.accent, P.folderFill};
                GraphicsPath clip;
                RoundPath(clip, RF(bar), SF(5));
                Region rg(&clip);
                g.SetClip(&rg);
                for (int i = 0; i < 3; i++) { int w = i < 2 ? fw * (i == 0 ? 6 : 3) / 10 : fw - (x - bar.left); FillR(g, {x, bar.top, x + w, bar.bottom}, 0, cs[i]); x += w; }
                g.ResetClip();
            }
        }
        break;
    }
    case TH_SHIM: {
        FillR(g, all, 0, P.bg);
        FillR(g, L.side, SF(26), P.side);
        RECT sh = L.panel; OffsetRect(&sh, 0, S(6));
        FillRA(g, sh, SF(28), P.text, g_dark ? 40 : 10);
        FillR(g, L.panel, SF(28), P.panel);
        Txt(dc, L.title, DirTitle(), P.text, g_fontBig);
        Txt(dc, L.sub, SubLine(), P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        RECT dsh = L.dock; OffsetRect(&dsh, 0, S(5));
        FillRA(g, dsh, (float)(dsh.bottom - dsh.top) / 2, RGB(0, 0, 0), g_dark ? 70 : 34);
        FillR(g, L.dock, (float)(L.dock.bottom - L.dock.top) / 2, P.primary);
        Txt(dc, L.status, SelSummary(), P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case TH_BOM: {
        FillR(g, all, 0, P.bg);
        FillG3(g, L.top, 0, P.accent, P.accent2, P.band, 12);
        Txt(dc, L.title, DirTitle(), P.onBand, g_fontBig);
        RECT c = L.card;
        for (int i = 3; i >= 1; i--) { RECT s2 = c; InflateRect(&s2, S(i * 3), S(i * 3)); OffsetRect(&s2, 0, S(6)); FillRA(g, s2, SF(20 + i * 3), P.accent, (BYTE)(g_dark ? 14 : 8)); }
        FillR(g, c, SF(20), P.panel);
        Txt(dc, L.sub, g_hasArc ? ArcSummary() + L" · " + ratio : SubLine(), P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break;
    }
    case TH_MUL: {
        FillG(g, all, 0, P.bg, P.bg2, 60);
        // 아래쪽 물빛 번짐
        { GraphicsPath ep; ep.AddEllipse((REAL)-S(100), (REAL)(H - S(180)), (REAL)(W + S(200)), (REAL)S(320));
          PathGradientBrush pb(&ep); Color cc = GC(P.accent2, g_dark ? 40 : 46); pb.SetCenterColor(cc); Color ec = GC(P.accent2, 0); int n = 1; pb.SetSurroundColors(&ec, &n); g.FillPath(&pb, &ep); }
        BYTE ga = g_dark ? 160 : 184;
        FillRA(g, L.top, SF(18), P.panel, ga); StrokeR(g, L.top, SF(18), P.border);
        FillRA(g, L.side, SF(18), P.panel, (BYTE)(ga - 50)); StrokeR(g, L.side, SF(18), P.border);
        FillRA(g, L.panel, SF(18), P.panel, ga); StrokeR(g, L.panel, SF(18), P.border);
        for (auto& gp : L.groups) {
            if (gp.label.empty()) VLine(g, gp.rc.left, gp.rc.top, gp.rc.bottom, P.lines);
            else Txt(dc, gp.rc, gp.label, P.sideDim, g_fontTiny);
        }
        DrawTree(g, dc);
        if (!HasList()) Txt(dc, {L.tree.left + S(10), L.tree.top, L.tree.right, L.tree.top + S(30)}, L"열린 파일 없음", P.sideDim, g_font);
        Txt(dc, L.title, DirTitle(), P.text, g_fontBig);
        Txt(dc, L.sub, SubLine(), P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        Txt(dc, L.status, StatusLeft(), P.dim, g_fontSmall);
        Txt(dc, L.status, ratio, P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case TH_BYEOL: {
        FillG(g, all, 0, P.bg2, P.bg, 70);
        // 별: 창 크기에 맞춰 늘 같은 자리
        uint32_t seed = 20261102u;
        auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
        for (int i = 0; i < 110; i++) {
            int x = (int)(rnd() % (uint32_t)max(1, W)), y = (int)(rnd() % (uint32_t)max(1, H)), sz = 1 + (int)(rnd() % 3);
            BYTE a = (BYTE)(g_dark ? 110 + rnd() % 130 : 50 + rnd() % 70);
            FillRA(g, {x, y, x + S(sz), y + S(sz)}, SF(sz), g_dark ? RGB(255, 255, 255) : RGB(0x78, 0x5a, 0xb4), a);
        }
        // 별자리 선
        const Btn* prev = nullptr;
        for (auto& b : L.btns) {
            if (b.style != BS_ORB) continue;
            if (prev) {
                int cx = (b.rc.left + b.rc.right) / 2;
                Pen pen(GC(P.accent, 120), SF(1.2f));
                g.DrawLine(&pen, (REAL)cx, (REAL)prev->rc.bottom + S(4), (REAL)cx, (REAL)b.rc.top - S(6));
            }
            prev = &b;
        }
        FillRA(g, L.panel, SF(22), P.panel, g_dark ? 242 : 238);
        StrokeR(g, L.panel, SF(22), Mix(P.border, P.accent, 0.25f));
        Txt(dc, L.title, DirTitle(), P.text, g_fontBig);
        Txt(dc, L.sub, SubLine(), P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        Txt(dc, L.status, StatusLeft(), P.dim, g_fontSmall);
        Txt(dc, L.status, ratio, P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    }
}
// ---- 테마 미리보기 그림 (앨범·설정에서 사용). r 안에 테마 배치를 축소해서 그린다
static void DrawThemeThumb(Graphics& g, RECT r, int theme, bool dark) {
    const Palette& Q = *PALS[theme][dark ? 1 : 0];
    int ly = TH_LAYOUT[theme];
    auto C = [](COLORREF c, COLORREF f) { return c == NONE ? f : c; };
    int W = r.right - r.left, H = r.bottom - r.top, x0 = r.left, y0 = r.top;
    auto R = [&](float a, float b, float c, float d) { return RECT{x0 + (int)(W * a), y0 + (int)(H * b), x0 + (int)(W * c), y0 + (int)(H * d)}; };
    COLORREF panel = C(Q.panel, Q.bg), sel = C(Q.sel, Q.hover), acc = C(Q.primary, Q.accent), side = C(Q.side, Q.bg);
    float rad = (float)max(2, W / 28);
    // 배경
    {
        GraphicsPath p; RoundPath(p, RF(r), rad + 2);
        Region clip(&p);
        g.SetClip(&clip);
        FillG(g, r, 0, Q.bg, Q.bg2, 90);
    }
    // 목록 줄 (파일 3줄, 가운데 줄 선택)
    auto rows = [&](RECT a) {
        int rh = max(4, (int)(a.bottom - a.top) / 6), y = (int)a.top + rh / 2, pad = max(3, W / 30);
        for (int i = 0; i < 4 && y + rh <= a.bottom; i++, y += rh + rh / 3) {
            if (i == 1) FillR(g, {a.left + pad / 2, y - 1, a.right - pad / 2, y + rh + 1}, 2, sel);
            FillR(g, {a.left + pad, y + rh / 4, a.left + pad + rh / 2 + 1, y + rh - rh / 4}, 1, i == 0 ? C(Q.folderFill, Q.accent) : C(Q.fileStroke, Q.dim));
            FillR(g, {a.left + pad + rh, y + rh / 3, a.left + pad + rh + (a.right - a.left) * (i % 2 ? 40 : 55) / 100, y + rh - rh / 3}, 1, i == 1 ? Q.text : Mix(panel, Q.dim, 0.7f));
        }
    };
    auto pill = [&](RECT a, COLORREF c, COLORREF c2 = NONE) { FillG(g, a, (float)(a.bottom - a.top) / 2, c, c2); };
    auto card = [&](RECT a, COLORREF c, float rr) { FillRA(g, {a.left + 1, a.top + 2, a.right + 1, a.bottom + 2}, rr, RGB(0, 0, 0), dark ? 60 : 22); FillR(g, a, rr, c); };
    switch (ly) {
    case TH_BASIC: {
        FillR(g, R(0, 0, 1, .2f), 0, C(Q.band, acc));
        pill(R(.06f, .06f, .3f, .14f), Q.onBand == NONE ? RGB(255, 255, 255) : Q.onBand);
        FillR(g, R(0, .2f, .26f, 1), 0, side);
        VLine(g, x0 + (int)(W * .26f), y0 + (int)(H * .2f), r.bottom, Q.border);
        RECT a = R(.29f, .26f, .97f, .96f); FillR(g, a, 0, panel); rows(a);
        break;
    }
    case TH_MIDNIGHT: {
        for (int i = 0; i < 4; i++) FillR(g, R(.05f, .1f + i * .13f, .22f, .17f + i * .13f), 2, i == 0 ? sel : Mix(Q.bg, Q.dim, .35f));
        RECT a = R(.28f, .08f, .96f, .92f); card(a, panel, rad); rows(R(.28f, .22f, .96f, .92f));
        pill(R(.74f, .12f, .92f, .2f), Q.accent, Q.accent2);
        break;
    }
    case TH_PASTEL: case TH_CHAGOK: {
        if (ly == TH_PASTEL) {
            FillR(g, R(.03f, .05f, .16f, .95f), rad, side);
            for (int i = 0; i < 4; i++) FillR(g, R(.06f, .12f + i * .16f, .13f, .22f + i * .16f), 2, i == 0 ? acc : Mix(side, Q.sideDim == NONE ? Q.dim : Q.sideDim, .5f));
        } else {
            for (int i = 0; i < 4; i++) FillR(g, R(.04f + i * .012f, .1f + i * .16f, .24f + i * .012f, .22f + i * .16f), 2, i == 0 ? Q.primary : Mix(Q.bg, Q.dim, .3f));
        }
        RECT a = R(ly == TH_PASTEL ? .2f : .32f, .08f, .96f, .92f);
        if (ly == TH_CHAGOK) { card({a.left + 4, a.top + 4, a.right + 3, a.bottom + 3}, Mix(panel, Q.accent, .25f), rad); }
        card(a, panel, rad); rows({a.left, a.top + H / 8, a.right, a.bottom});
        if (ly == TH_CHAGOK) {
            RECT b = R(.36f, .12f, .92f, .17f);
            int w = b.right - b.left;
            FillR(g, {b.left, b.top, b.left + w / 2, b.bottom}, 1, Q.primary);
            FillR(g, {b.left + w / 2, b.top, b.left + w * 3 / 4, b.bottom}, 1, Q.accent);
            FillR(g, {b.left + w * 3 / 4, b.top, b.right, b.bottom}, 1, C(Q.folderFill, Q.accent2));
        } else pill(R(.7f, .12f, .9f, .19f), Q.accent, Q.accent2);
        break;
    }
    case TH_NORDIC: {
        FillR(g, R(0, 0, .28f, 1), 0, side);
        for (int i = 0; i < 4; i++) FillR(g, R(.04f, .14f + i * .14f, .24f, .22f + i * .14f), 2, i == 1 ? C(Q.sideSel, sel) : Mix(side, C(Q.sideDim, Q.dim), .3f));
        RECT a = R(.28f, 0, 1, 1); FillR(g, a, 0, panel);
        HLine(g, a.left, a.right, y0 + (int)(H * .2f), Q.border);
        pill(R(.74f, .06f, .94f, .14f), acc);
        rows(R(.3f, .24f, .98f, .98f));
        break;
    }
    case TH_NAVY: {
        FillR(g, R(0, 0, 1, .24f), 0, C(Q.band, acc));
        FillR(g, R(.05f, .08f, .3f, .15f), 1, Q.onBand == NONE ? RGB(255, 255, 255) : Q.onBand);
        pill(R(.72f, .07f, .94f, .16f), Q.accent, Q.accent2);
        FillR(g, R(0, .24f, .25f, 1), 0, side);
        RECT a = R(.28f, .3f, .96f, .95f); card(a, panel, 2); rows(a);
        break;
    }
    case TH_MINT: {
        FillR(g, R(0, 0, 1, .2f), 0, panel);
        HLine(g, r.left, r.right, y0 + (int)(H * .2f), Q.border);
        for (int i = 0; i < 4; i++) FillR(g, R(.05f + i * .13f, .06f, .15f + i * .13f, .14f), 2, i == 0 ? acc : Q.btn);
        RECT a = R(.04f, .26f, .96f, .95f); card(a, panel, rad); rows(a);
        break;
    }
    case TH_SHIM: {
        FillR(g, R(.03f, .05f, .14f, .78f), rad, side);
        RECT a = R(.18f, .06f, .96f, .74f); card(a, panel, rad); rows(a);
        RECT d = R(.25f, .8f, .75f, .95f); card(d, Q.primary, (float)(d.bottom - d.top) / 2);
        for (int i = 0; i < 5; i++) { int cx = d.left + (d.right - d.left) * (2 * i + 1) / 10, s = (d.bottom - d.top) / 3; FillR(g, {cx - s, d.top + s, cx + s, d.bottom - s}, (float)s, i == 0 ? Q.accent : Q.onPrimary); }
        break;
    }
    case TH_BOM: {
        FillG3(g, R(0, 0, 1, .42f), 0, Q.accent, Q.accent2, C(Q.band, Q.accent), 20);
        FillR(g, R(.06f, .08f, .4f, .16f), 1, C(Q.onBand, RGB(255, 255, 255)));
        pill(R(.64f, .07f, .94f, .17f), RGB(255, 255, 255));
        RECT a = R(.05f, .3f, .95f, .96f); card(a, panel, rad); rows({a.left, a.top + H / 10, a.right, a.bottom});
        break;
    }
    case TH_MUL: {
        FillRA(g, R(.03f, .05f, .97f, .2f), rad, panel, 150);
        pill(R(.06f, .08f, .26f, .17f), Q.accent, Q.accent2);
        FillRA(g, R(.03f, .25f, .27f, .95f), rad, panel, 140);
        RECT a = R(.3f, .25f, .97f, .95f); FillRA(g, a, rad, panel, 200); rows(a);
        break;
    }
    case TH_BYEOL: {
        unsigned sd = 2463534242u + theme * 7 + dark;
        auto rnd = [&]() { sd ^= sd << 13; sd ^= sd >> 17; sd ^= sd << 5; return (int)(sd & 0x7fffffff); };
        for (int i = 0; i < 18; i++) { int sx = r.left + rnd() % W, sy = r.top + rnd() % H; FillRA(g, {sx, sy, sx + 2, sy + 2}, 1, Q.text, 120); }
        int ox = x0 + (int)(W * .1f), s = max(5, H / 9);
        for (int i = 0; i < 4; i++) {
            int oy = y0 + (int)(H * (.14f + i * .2f));
            if (i) { Pen pn(GC(Q.accent, 110), 1); g.DrawLine(&pn, ox, oy - (int)(H * .2f) + s / 2, ox, oy - s / 2); }
            FillR(g, {ox - s / 2, oy - s / 2, ox + s / 2, oy + s / 2}, (float)s / 2, i == 0 ? Q.accent : Mix(Q.bg, Q.dim, .5f));
        }
        RECT a = R(.22f, .08f, .96f, .92f); card(a, panel, rad); StrokeR(g, a, rad, Mix(panel, Q.accent, .35f)); rows(a);
        break;
    }
    }
    g.ResetClip();
}
static void Paint(HDC dc, int W, int H) {
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    RECT all{0, 0, W, H};
    switch (g_ly) {
    case TH_BASIC: {
        FillR(g, all, 0, P.bg);
        FillR(g, L.top, 0, P.band);
        FillR(g, L.ribbon, 0, P.panel);
        HLine(g, 0, W, L.ribbon.bottom - 1, P.border);
        for (auto& gp : L.groups) {
            Txt(dc, {gp.rc.left, gp.rc.bottom - S(22), gp.rc.right, gp.rc.bottom}, gp.label, P.dim, g_fontTiny, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            VLine(g, gp.rc.right, gp.rc.top + S(2), gp.rc.bottom - S(2), P.border);
        }
        FillR(g, L.status, 0, P.band);
        std::wstring l, rt;
        if (g_fs) {
            l = std::to_wstring(g_rows.size()) + L"개 항목";
            std::wstring s = SelSummary();
            if (!s.empty()) l = s + L" · " + l;
            rt = g_fsPath.empty() ? L"내 PC" : g_fsPath;
        } else if (g_hasArc) {
            l = std::to_wstring(g_fileCount) + L"개 항목";
            std::wstring s = SelSummary();
            if (!s.empty()) l = s + L" · " + l;
            rt = FormatLabel() + L" · " + FmtSize(g_totU) + L" → " + FmtSize(g_totC) + (g_arc.readOnly ? L" · 읽기 전용" : L"");
        } else l = L"열린 압축 파일 없음";
        Txt(dc, {S(14), L.status.top, W / 2, L.status.bottom}, l, P.onBand, g_fontSmall);
        Txt(dc, {W / 2, L.status.top, W - S(14), L.status.bottom}, rt, P.onBand, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case TH_MIDNIGHT: {
        FillG(g, all, 0, P.bg, P.bg2, 120);
        FillR(g, L.card, SF(10), P.panel);
        StrokeR(g, L.card, SF(10), P.border);
        RECT c = L.card;
        int x = c.left + S(16), w = c.right - S(14);
        if (g_fs) {
            int files = 0, dirs = 0;
            for (auto& it : g_fsItems) { if (it.dir) dirs++; else files++; }
            Txt(dc, {x, c.top + S(14), w, c.top + S(36)}, FsLabel(g_fsPath), P.text, g_fontBold);
            Txt(dc, {x, c.top + S(38), w, c.top + S(58)}, L"폴더 " + std::to_wstring(dirs) + L"개 · 파일 " + std::to_wstring(files) + L"개", P.dim, g_fontSmall);
            Txt(dc, {x, c.top + S(62), w, c.bottom - S(10)}, L"압축 파일은 두 번 눌러 열고, 항목을 골라 '새로 압축'을 누르면 압축합니다.", P.dim, g_fontSmall, DT_LEFT | DT_WORDBREAK);
        } else if (g_hasArc) {
            Txt(dc, {x, c.top + S(14), w, c.top + S(36)}, FileNameOf(g_arc.path), P.text, g_fontBold);
            std::wstring l1 = L"항목 " + std::to_wstring(g_fileCount) + L"개 · " + FormatLabel();
            if (g_arc.isSplit) l1 += L" · 분할 " + std::to_wstring(g_arc.volumes.size()) + L"개";
            Txt(dc, {x, c.top + S(38), w, c.top + S(58)}, l1, P.dim, g_fontSmall);
            Txt(dc, {x, c.top + S(60), w, c.top + S(80)}, FmtSize(g_totU) + L" → " + FmtSize(g_totC), P.dim, g_fontSmall);
            RECT bar{x, c.top + S(90), w, c.top + S(95)};
            FillR(g, bar, SF(2.5f), P.track);
            RECT f = bar;
            if (g_totU) f.right = bar.left + (int)((bar.right - bar.left) * min(1.0, (double)g_totC / g_totU));
            if (f.right > f.left + S(4)) FillG(g, f, SF(2.5f), P.accent, P.accent2);
            Txt(dc, {x, c.top + S(104), w, c.top + S(124)}, L"압축률 " + FmtRatio(g_totU, g_totC), P.dim, g_fontSmall);
        } else {
            Txt(dc, {x, c.top + S(14), w, c.top + S(36)}, L"열린 파일 없음", P.text, g_fontBold);
            Txt(dc, {x, c.top + S(40), w, c.bottom - S(10)}, L"압축 파일을 열거나 오른쪽 영역으로 끌어다 놓으세요.", P.dim, g_fontSmall, DT_LEFT | DT_WORDBREAK);
        }
        FillR(g, L.panel, SF(10), P.panel);
        StrokeR(g, L.panel, SF(10), P.border);
        HLine(g, L.panel.left + 1, L.panel.right - 1, L.top.bottom, P.border);
        StrokeR(g, L.zone, SF(8), Mix(P.panel, P.dim, 0.45f), 1, true);
        if (!HasList()) Txt(dc, L.title, DirTitle(), P.text, g_fontTitle);
        std::wstring t = !g_hasArc ? (g_fs ? L"압축 파일을 두 번 누르면 열립니다 · 끌어다 놓으면 새로 압축합니다" : L"파일이나 폴더를 끌어다 놓으면 새로 압축합니다")
                       : g_arc.readOnly ? FormatLabel() + L" 형식은 읽기 전용입니다 · 끌어다 놓으면 새로 압축합니다"
                                        : L"여기로 파일을 끌어다 놓으면 압축 파일에 추가됩니다";
        int isz = S(18), tw = TextW(t, g_font);
        int sx = (L.zone.left + L.zone.right - (isz + S(8) + tw)) / 2;
        DrawIco(g, IC_UPLOAD, (REAL)sx, (REAL)(L.zone.top + L.zone.bottom - isz) / 2, (REAL)isz, P.dim);
        Txt(dc, {sx + isz + S(8), L.zone.top, L.zone.right - S(8), L.zone.bottom}, t, P.dim, g_font);
        break;
    }
    case TH_PASTEL: {
        FillG(g, all, 0, P.bg, P.bg2, 35);
        FillR(g, L.side, SF(22), P.side);
        FillR(g, L.panel, SF(22), P.panel);
        Txt(dc, L.title, DirTitle(), P.text, g_fontBig);
        if (!HasList()) Txt(dc, L.sub, L"압축 파일을 열거나 끌어다 놓으세요", P.dim, g_fontSmall);
        else if (g_fs) {
            int files = 0, dirs = 0, arcs = 0;
            for (auto& it : g_fsItems) { if (it.dir) dirs++; else files++; if (it.arc) arcs++; }
            int x = L.chips.left, y = L.chips.top, h = L.chips.bottom - L.chips.top;
            DrawChip(g, dc, x, y, h, L"폴더 " + std::to_wstring(dirs) + L"개", g_dark ? RGB(0x17, 0x36, 0x40) : RGB(0xe6, 0xfa, 0xfd), g_dark ? RGB(0x7d, 0xe8, 0xf8) : RGB(0x0e, 0x63, 0x7a));
            DrawChip(g, dc, x, y, h, L"파일 " + std::to_wstring(files) + L"개", g_dark ? RGB(0x46, 0x22, 0x3c) : RGB(0xfd, 0xec, 0xf6), g_dark ? RGB(0xf9, 0xb4, 0xda) : RGB(0xa3, 0x16, 0x5a));
            if (arcs) DrawChip(g, dc, x, y, h, L"압축 파일 " + std::to_wstring(arcs) + L"개", g_dark ? RGB(0x18, 0x3a, 0x2a) : RGB(0xea, 0xfb, 0xef), g_dark ? RGB(0x8a, 0xef, 0xb0) : RGB(0x16, 0x6b, 0x38));
        } else {
            int x = L.chips.left, y = L.chips.top, h = L.chips.bottom - L.chips.top;
            DrawChip(g, dc, x, y, h, L"항목 " + std::to_wstring(g_fileCount) + L"개", g_dark ? RGB(0x46, 0x22, 0x3c) : RGB(0xfd, 0xec, 0xf6), g_dark ? RGB(0xf9, 0xb4, 0xda) : RGB(0xa3, 0x16, 0x5a));
            DrawChip(g, dc, x, y, h, FmtSize(g_totU) + L" → " + FmtSize(g_totC), g_dark ? RGB(0x17, 0x36, 0x40) : RGB(0xe6, 0xfa, 0xfd), g_dark ? RGB(0x7d, 0xe8, 0xf8) : RGB(0x0e, 0x63, 0x7a));
            DrawChip(g, dc, x, y, h, L"압축률 " + FmtRatio(g_totU, g_totC), g_dark ? RGB(0x18, 0x3a, 0x2a) : RGB(0xea, 0xfb, 0xef), g_dark ? RGB(0x8a, 0xef, 0xb0) : RGB(0x16, 0x6b, 0x38));
            DrawChip(g, dc, x, y, h, FormatLabel(), P.sel, P.accentFg);
        }
        break;
    }
    case TH_NORDIC: case TH_NAVY: {
        bool navy = g_ly == TH_NAVY;
        FillR(g, all, 0, P.bg);
        FillR(g, L.side, 0, P.side);
        if (!navy) VLine(g, L.side.right - 1, 0, H, P.border);
        for (auto& gp : L.groups) Txt(dc, gp.rc, gp.label, P.sideDim, g_fontTiny);
        Txt(dc, L.title, DirTitle(), P.text, g_fontBig);
        Txt(dc, L.sub, g_fs ? (g_fsPath.empty() ? L"자주 쓰는 폴더와 드라이브" : g_fsPath) : g_hasArc ? FileNameOf(g_arc.path) + L" · " + ArcSummary() : L"압축 파일을 열거나 끌어다 놓으세요", P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        if (navy) FillR(g, L.top, 0, P.primary);
        if (!navy) {
            std::wstring l = HasList() ? SelSummary() : L"", rt = g_hasArc ? FormatLabel() + L" · 압축률 " + FmtRatio(g_totU, g_totC) : L"";
            if (HasList() && l.empty()) l = std::to_wstring(g_rows.size()) + L"개 항목";
            Txt(dc, L.status, l, P.dim, g_fontSmall);
            Txt(dc, L.status, rt, P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        DrawTree(g, dc);
        if (!HasList()) Txt(dc, {L.tree.left + S(14), L.tree.top, L.tree.right, L.tree.top + S(30)}, L"열린 파일 없음", P.sideDim, g_font);
        break;
    }
    case TH_CHAGOK: case TH_SHIM: case TH_BOM: case TH_MUL: case TH_BYEOL: PaintNew(g, dc, W, H); break;
    default: {  // 민트
        FillR(g, all, 0, P.panel);
        FillR(g, L.mid, 0, P.listBg);
        HLine(g, 0, W, L.top.bottom - 1, P.border);
        VLine(g, L.side.right - 1, L.side.top, H, P.border);
        VLine(g, L.info.left, L.info.top, H, P.border);
        for (auto& gp : L.groups) Txt(dc, gp.rc, gp.label, P.sideDim, g_fontTiny);
        DrawTree(g, dc);
        if (!HasList()) Txt(dc, {L.tree.left + S(14), L.tree.top, L.tree.right, L.tree.top + S(30)}, L"열린 파일 없음", P.sideDim, g_font);
        DrawInfoPanel(g, dc);
        break;
    }
    }
    if (L.search.right > L.search.left) {
        bool foc = GetFocus() == g_searchEdit;
        bool pill = g_ly == TH_PASTEL || g_ly == TH_SHIM || g_ly == TH_BOM || g_ly == TH_BYEOL;
        float rad = pill ? (float)(L.search.bottom - L.search.top) / 2 : g_ly == TH_MUL ? SF(12) : SF(8);
        FillR(g, L.search, rad, P.btn);
        StrokeR(g, L.search, rad, foc ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), foc ? SF(1.5f) : 1);
        int isz = S(16);
        DrawIco(g, IC_SEARCH, (REAL)L.search.left + S(10), (REAL)(L.search.top + L.search.bottom - isz) / 2, (REAL)isz, P.dim);
    }
    for (auto& b : L.btns) if (b.style != BS_PROGPILL) DrawBtn(g, dc, b);
    if (HasList()) {
        DrawHeader(g, dc); DrawRows(g, dc);
        if (g_rows.empty()) Txt(dc, {L.list.left, L.list.top + S(40), L.list.right, L.list.top + S(70)},
                                g_search.empty() ? L"비어 있는 폴더입니다" : L"찾는 이름이 없습니다", P.dim, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else DrawEmpty(g, dc);
    for (auto& b : L.btns) if (b.style == BS_PROGPILL) DrawBtn(g, dc, b);
}

// ============================================================ 팝업 메뉴 (테마 적용)
struct MItem { int id; std::wstring text; int kind; bool checked; bool enabled; };  // kind 0 항목 1 구분선 2 제목
struct PopupState { std::vector<MItem>* items; std::vector<RECT> rcs; int hover = -1, result = 0; bool done = false; };
static PopupState* g_pop;

static LRESULT CALLBACK PopupProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    PopupState* s = g_pop;
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        HDC md = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(md, bm);
        {
            Graphics g(md);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            COLORREF bg = P.panel;
            FillR(g, cr, 0, bg);
            StrokeR(g, cr, 0, P.border);
            for (size_t i = 0; i < s->items->size(); i++) {
                auto& it = (*s->items)[i];
                RECT r = s->rcs[i];
                if (it.kind == 1) { HLine(g, r.left + S(8), r.right - S(8), (r.top + r.bottom) / 2, P.border); continue; }
                if (it.kind == 2) { Txt(md, {r.left + S(12), r.top, r.right, r.bottom}, it.text, P.dim, g_fontSmall); continue; }
                if ((int)i == s->hover && it.enabled) { RECT hr = r; hr.left += S(4); hr.right -= S(4); FillR(g, hr, SF(4), Mix(P.panel, P.text, g_dark ? 0.12f : 0.07f)); }
                if (it.checked) DrawIco(g, IC_CHECK, (REAL)r.left + S(10), (REAL)(r.top + r.bottom - S(16)) / 2, (REAL)S(16), P.accent, NONE, 2.2f);
                size_t tab = it.text.find(L'\t');  // "이름\t단축키"
                Txt(md, {r.left + S(34), r.top, r.right - S(12), r.bottom}, it.text.substr(0, tab), it.enabled ? P.text : P.dim, g_font);
                if (tab != std::wstring::npos)
                    Txt(md, {r.left + S(34), r.top, r.right - S(14), r.bottom}, it.text.substr(tab + 1), P.dim, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        BitBlt(dc, 0, 0, cr.right, cr.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hv = -1;
        for (size_t i = 0; i < s->rcs.size(); i++) if (PtInRect(&s->rcs[i], pt) && (*s->items)[i].kind == 0) hv = (int)i;
        if (hv != s->hover) { s->hover = hv; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        RECT cr; GetClientRect(h, &cr);
        if (!PtInRect(&cr, pt)) s->done = true;
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        for (size_t i = 0; i < s->rcs.size(); i++) {
            auto& it = (*s->items)[i];
            if (PtInRect(&s->rcs[i], pt) && it.kind == 0 && it.enabled) { s->result = it.id; s->done = true; }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        int n = (int)s->items->size();
        if (w == VK_ESCAPE) s->done = true;
        else if (w == VK_UP || w == VK_DOWN) {
            int d = w == VK_UP ? -1 : 1, i = s->hover;
            for (int k = 0; k < n; k++) {
                i = (i + d + n) % n;
                if ((*s->items)[(size_t)i].kind == 0 && (*s->items)[(size_t)i].enabled) break;
            }
            s->hover = i;
            InvalidateRect(h, nullptr, FALSE);
        } else if (w == VK_RETURN && s->hover >= 0) { s->result = (*s->items)[(size_t)s->hover].id; s->done = true; }
        return 0;
    }
    case WM_CAPTURECHANGED:
        if ((HWND)lp != h) s->done = true;
        return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcW(h, m, w, lp);
}

static int RunPopup(HWND owner, RECT anchorScreen, std::vector<MItem>& items, bool alignRight = false) {
    PopupState st;
    st.items = &items;
    int itemH = S(32), w = S(180), y = S(4);
    for (auto& it : items) {
        int h = it.kind == 1 ? S(9) : it.kind == 2 ? S(26) : itemH;
        if (it.kind != 1) {
            size_t tab = it.text.find(L'\t');
            int tw = TextW(it.text.substr(0, tab), g_font) + (tab == std::wstring::npos ? 0 : TextW(it.text.substr(tab + 1), g_fontSmall) + S(28));
            w = max(w, tw + S(56));
        }
        st.rcs.push_back({0, y, 0, y + h});
        y += h;
    }
    for (auto& r : st.rcs) r.right = w;
    int h = y + S(4);
    int x = alignRight ? anchorScreen.right - w : anchorScreen.left, top = anchorScreen.bottom + S(4);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&anchorScreen, MONITOR_DEFAULTTONEAREST), &mi);
    if (top + h > mi.rcWork.bottom) top = anchorScreen.top - S(4) - h;
    if (top < mi.rcWork.top) top = max((int)mi.rcWork.top, (int)mi.rcWork.bottom - h);  // 화면 밖으로 나가지 않게
    x = max((int)mi.rcWork.left, min(x, (int)mi.rcWork.right - w));
    g_pop = &st;
    HWND pw = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"ChagokPopup", L"", WS_POPUP,
                              x, top, w, h, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    DWORD corner = 3;
    DwmSetWindowAttribute(pw, 33, &corner, sizeof(corner));
    ShowWindow(pw, SW_SHOWNA);
    SetCapture(pw);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN || msg.message == WM_CHAR) msg.hwnd = pw;
        if (msg.message == WM_SYSKEYDOWN) { st.done = true; continue; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) PostQuitMessage((int)msg.wParam);
    if (GetCapture() == pw) ReleaseCapture();
    DestroyWindow(pw);
    g_pop = nullptr;
    return st.result;
}

// ============================================================ 대화상자 (테마 적용)
struct DlgSpec {
    std::wstring title, msg;
    std::vector<std::wstring> buttons;
    int def = 0, cancel = -1;
    bool input = false, password = false, confirm = false;   // confirm: 암호 확인 입력란 추가
    bool text = false;  // 숫자가 아닌 일반 글자 입력
    int selEnd = -1;    // 처음 선택할 글자 범위 (확장자 앞까지 등)
    std::wstring inputText, inputSuffix, inputText2, label1, label2, note;
};
struct DlgState {
    DlgSpec* spec;
    std::vector<RECT> brc;
    RECT msgRc{}, editBox{}, editBox2{}, noteRc{};
    int hover = -1, focus = 0, result = -1, press = -1;
    bool done = false, kbd = false;
    HWND edit = nullptr, edit2 = nullptr;
    HBRUSH editBrush = nullptr;
};
static DlgState* g_dlg;

static LRESULT CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    DlgState* s = g_dlg;
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        HDC md = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(md, bm);
        {
            Graphics g(md);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHalf);
            FillR(g, cr, 0, P.panel);
            StrokeR(g, cr, 0, P.border);
            Txt(md, {S(22), S(16), cr.right - S(22), S(44)}, s->spec->title, P.text, g_fontTitle);
            Txt(md, s->msgRc, s->spec->msg, P.text, g_font, DT_LEFT | DT_WORDBREAK);
            auto box = [&](HWND e, RECT r, const std::wstring& label) {
                if (!e) return;
                if (!label.empty()) Txt(md, {r.left, r.top - S(22), r.right, r.top - S(2)}, label, P.dim, g_fontSmall);
                FillR(g, r, SF(4), P.bg);
                StrokeR(g, r, SF(4), GetFocus() == e ? P.accent : P.btnBorder, GetFocus() == e ? SF(1.5f) : 1);
            };
            box(s->edit, s->editBox, s->spec->label1);
            box(s->edit2, s->editBox2, s->spec->label2);
            if (s->edit && !s->spec->inputSuffix.empty())
                Txt(md, {s->editBox.right + S(8), s->editBox.top, cr.right - S(22), s->editBox.bottom}, s->spec->inputSuffix, P.dim, g_font);
            if (!s->spec->note.empty()) Txt(md, s->noteRc, s->spec->note, P.dim, g_fontSmall, DT_LEFT | DT_WORDBREAK);
            RECT foot{1, cr.bottom - S(68), cr.right - 1, cr.bottom - 1};
            FillR(g, foot, 0, Mix(P.panel, P.text, g_dark ? 0.04f : 0.025f));
            for (size_t i = 0; i < s->brc.size(); i++) {
                RECT r = s->brc[i];
                bool primary = (int)i == s->spec->def;
                bool hov = s->hover == (int)i;
                COLORREF bg = primary ? (hov ? Mix(P.accent, g_dark ? RGB(255, 255, 255) : RGB(0, 0, 0), 0.1f) : P.accent) : (hov ? P.hover : P.btn);
                FillR(g, r, SF(4), bg);
                if (!primary) StrokeR(g, r, SF(4), P.btnBorder);
                if ((int)i == s->focus && !s->edit && s->kbd) { RECT f = r; InflateRect(&f, S(3), S(3)); StrokeR(g, f, SF(6), P.text, SF(1.5f)); }
                Txt(md, r, s->spec->buttons[i], primary ? P.onAccent : P.text, primary ? g_fontBold : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        BitBlt(dc, 0, 0, cr.right, cr.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pt);
        if (pt.y < S(48)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hv = -1;
        for (size_t i = 0; i < s->brc.size(); i++) if (PtInRect(&s->brc[i], pt)) hv = (int)i;
        if (hv != s->hover) { s->hover = hv; InvalidateRect(h, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: s->hover = -1; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        s->press = -1;
        for (size_t i = 0; i < s->brc.size(); i++) if (PtInRect(&s->brc[i], pt)) s->press = (int)i;
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        for (size_t i = 0; i < s->brc.size(); i++)
            if (PtInRect(&s->brc[i], pt) && (s->press == (int)i || s->press < 0)) { s->result = (int)i; s->done = true; }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, P.text);
        SetBkColor(dc, P.bg);
        if (!s->editBrush) s->editBrush = CreateSolidBrush(P.bg);
        return (LRESULT)s->editBrush;
    }
    case WM_COMMAND:
        if (HIWORD(w) == EN_SETFOCUS || HIWORD(w) == EN_KILLFOCUS) InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CLOSE: s->result = s->spec->cancel; s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, lp);
}

static HWND ProgWindow();
static std::wstring AppVersion();
static bool g_mainHidden = false;   // 우클릭 메뉴로 실행: 메인 창 없이 진행 창만
static int RunDialog(HWND owner, DlgSpec& spec) {
    DlgState st;
    st.spec = &spec;
    st.focus = spec.def;
    int bh = S(32), gap = S(8), pad = S(22);
    std::vector<int> bw;
    int btnTotal = 0;
    for (auto& b : spec.buttons) { int x = max(S(84), TextW(b, g_fontBold) + S(28)); bw.push_back(x); btnTotal += x + gap; }
    int W = max(S(400), min(S(640), btnTotal + pad * 2));
    RECT mr{pad, S(52), W - pad, S(52)};
    SelectObject(g_mdc, g_font);
    DrawTextW(g_mdc, spec.msg.c_str(), (int)spec.msg.size(), &mr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    mr.right = W - pad;
    int y = mr.bottom + S(16);
    if (spec.input) {
        if (!spec.label1.empty()) y += S(22);
        st.editBox = {pad, y, W - pad - S(40), y + S(34)}; y += S(34) + S(18);
        if (spec.confirm) { y += S(22); st.editBox2 = {pad, y, W - pad - S(40), y + S(34)}; y += S(34) + S(18); }
    }
    if (!spec.note.empty()) {
        RECT nr{pad, y, W - pad, y};
        SelectObject(g_mdc, g_fontSmall);
        DrawTextW(g_mdc, spec.note.c_str(), (int)spec.note.size(), &nr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        nr.right = W - pad;
        st.noteRc = nr;
        y = nr.bottom + S(16);
    }
    int H = y + S(68);
    st.msgRc = mr;
    int x = W - pad;
    st.brc.resize(spec.buttons.size());
    for (int i = (int)spec.buttons.size() - 1; i >= 0; i--) {
        st.brc[(size_t)i] = {x - bw[(size_t)i], H - S(50), x, H - S(50) + bh};
        x -= bw[(size_t)i] + gap;
    }
    // 작업 중 질문(덮어쓰기·암호)은 진행 창 위에 떠야 한다: 진행 창이 보이면 그 창을 주인으로
    // (진행 창은 0.25초 뒤에 뜨므로 첫 파일에서 바로 묻는 경우엔 먼저 띄운다. 그러지 않으면 늦게 뜬 진행 창이 질문을 덮는다)
    if (HWND pw = ProgWindow())
        if (owner == g_hwnd && (!g_progHidden || g_mainHidden)) {
            if (IsIconic(pw)) ShowWindow(pw, SW_RESTORE);
            else if (!IsWindowVisible(pw)) ShowWindow(pw, SW_SHOW);
            owner = pw;
        }
    RECT orc;
    if (IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner, &orc);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &orc, 0);  // 주인 창이 숨어 있으면 화면 가운데
    int px = (orc.left + orc.right - W) / 2, py = (orc.top + orc.bottom - H) / 2;
    g_dlg = &st;
    HWND dw = CreateWindowExW(WS_EX_DLGMODALFRAME & 0, L"ChagokDialog", spec.title.c_str(), WS_POPUP,
                              px, py, W, H, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    DWORD corner = 2;
    DwmSetWindowAttribute(dw, 33, &corner, sizeof(corner));
    if (spec.input) {
        RECT e = st.editBox;
        int eh = S(20);
        st.edit = CreateWindowExW(0, L"EDIT", spec.inputText.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | (spec.password ? ES_PASSWORD : spec.text ? 0 : ES_NUMBER),
                                  e.left + S(10), (e.top + e.bottom - eh) / 2, e.right - e.left - S(20), eh, dw, nullptr, nullptr, nullptr);
        SendMessageW(st.edit, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(st.edit, EM_SETSEL, 0, spec.selEnd >= 0 ? spec.selEnd : -1);
        if (spec.confirm) {
            RECT e2 = st.editBox2;
            st.edit2 = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                       e2.left + S(10), (e2.top + e2.bottom - eh) / 2, e2.right - e2.left - S(20), eh, dw, nullptr, nullptr, nullptr);
            SendMessageW(st.edit2, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
    }
    EnableWindow(owner, FALSE);
    ShowWindow(dw, SW_SHOW);
    SetWindowPos(dw, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    BringWindowToTop(dw);
    if (!SetForegroundWindow(dw)) {  // 다른 프로그램을 쓰는 중이면 작업 표시줄에서 깜박여 알린다
        FLASHWINFO fi{sizeof(fi), owner, FLASHW_ALL | FLASHW_TIMERNOFG, 0, 0};
        FlashWindowEx(&fi);
    }
    if (st.edit) SetFocus(st.edit); else SetFocus(dw);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            int n = (int)spec.buttons.size();
            if (msg.wParam == VK_RETURN) { st.result = st.edit ? spec.def : st.focus; st.done = true; continue; }
            if (st.edit2 && msg.wParam == VK_TAB) {
                SetFocus(GetFocus() == st.edit ? st.edit2 : st.edit);
                InvalidateRect(dw, nullptr, FALSE);
                continue;
            }
            if (msg.wParam == VK_ESCAPE) { st.result = spec.cancel; st.done = true; continue; }
            if (!st.edit && (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT || msg.wParam == VK_TAB)) {
                bool back = msg.wParam == VK_LEFT || (msg.wParam == VK_TAB && GetKeyState(VK_SHIFT) < 0);
                st.focus = (st.focus + (back ? n - 1 : 1)) % n;
                st.kbd = true;
                InvalidateRect(dw, nullptr, FALSE);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) PostQuitMessage((int)msg.wParam);
    if (st.edit) {
        wchar_t b[256] = {};
        GetWindowTextW(st.edit, b, 256);
        spec.inputText = b;
        SecureZeroMemory(b, sizeof(b));
        if (st.edit2) { GetWindowTextW(st.edit2, b, 256); spec.inputText2 = b; SecureZeroMemory(b, sizeof(b)); }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    DestroyWindow(dw);
    if (st.editBrush) DeleteObject(st.editBrush);
    g_dlg = nullptr;
    return st.result;
}
static void MsgBox(const std::wstring& title, const std::wstring& msg) {
    DlgSpec d;
    d.title = title; d.msg = msg; d.buttons = {L"확인"}; d.cancel = 0;
    RunDialog(g_hwnd, d);
}

// ============================================================ 파일 대화상자
static bool PickPaths(std::vector<std::wstring>& out, bool folders, bool multi, const wchar_t* title, bool archives = false) {
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    DWORD o = 0;
    d->GetOptions(&o);
    o |= FOS_FORCEFILESYSTEM | (folders ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST) | (multi ? FOS_ALLOWMULTISELECT : 0);
    d->SetOptions(o);
    d->SetTitle(title);
    if (archives) {
        COMDLG_FILTERSPEC f[] = {{L"압축 파일", L"*.zip;*.z01;*.001;*.7z;*.rar;*.tar;*.gz;*.tgz;*.bz2;*.tbz;*.tbz2;*.xz;*.txz;*.zst;*.cab;*.iso;*.img;*.wim;*.lzh;*.lha;*.arj;*.jar;*.apk;*.cpio;*.rpm;*.deb;*.dmg;*.vhd;*.vhdx;*.vmdk;*.lzma;*.z"},
                                 {L"모든 파일", L"*.*"}};
        d->SetFileTypes(2, f);
    }
    bool ok = false;
    if (SUCCEEDED(d->Show(g_hwnd))) {
        IShellItemArray* arr = nullptr;
        if (SUCCEEDED(d->GetResults(&arr))) {
            DWORD n = 0;
            arr->GetCount(&n);
            for (DWORD i = 0; i < n; i++) {
                IShellItem* it = nullptr;
                if (SUCCEEDED(arr->GetItemAt(i, &it))) {
                    PWSTR p = nullptr;
                    if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out.push_back(p); CoTaskMemFree(p); }
                    it->Release();
                }
            }
            arr->Release();
            ok = !out.empty();
        }
    }
    d->Release();
    return ok;
}
static bool PickFolder(std::wstring& out, const std::wstring& initial, const wchar_t* title) {
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    DWORD o = 0;
    d->GetOptions(&o);
    d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(title);
    d->SetOkButtonLabel(L"여기에 풀기");
    IShellItem* si = nullptr;
    if (!initial.empty() && SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&si)))) { d->SetFolder(si); si->Release(); }
    bool ok = false;
    if (SUCCEEDED(d->Show(g_hwnd))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); ok = true; }
            it->Release();
        }
    }
    d->Release();
    return ok;
}
// 저장 창: 파일 형식(ZIP / 7z / TAR.GZ / TAR)을 고르게 한다. 마지막 선택을 기억
static const wchar_t* SAVE_EXT[] = {L".zip", L".7z", L".tar.gz", L".tar"};
static bool AskNewPassword(std::wstring& pw, bool sevenZip);
static bool PickSave(std::wstring& out, const std::wstring& dir, const std::wstring& name, std::wstring* password = nullptr) {
    IFileSaveDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    bool sz = SzAvailable();
    COMDLG_FILTERSPEC f[] = {{L"ZIP (*.zip) - 빠름, 어디서나 열림", L"*.zip"},
                             {L"7z (*.7z) - 더 작게, 느림", L"*.7z"},
                             {L"TAR.GZ (*.tar.gz) - 리눅스용", L"*.tar.gz;*.tgz"},
                             {L"TAR (*.tar) - 압축 없이 묶기", L"*.tar"}};
    UINT n = sz ? 4 : 1;
    UINT last = std::min<UINT>(RegGet(L"Format", 0), n - 1);
    d->SetFileTypes(n, f);
    d->SetFileTypeIndex(last + 1);
    d->SetTitle(L"압축 파일 저장");
    d->SetFileName((name + SAVE_EXT[last]).c_str());
    // 저장 창 아래쪽에 '암호 설정' 체크 상자
    IFileDialogCustomize* cust = nullptr;
    if (password && SUCCEEDED(d->QueryInterface(IID_PPV_ARGS(&cust))) && cust)
        cust->AddCheckButton(1001, L"암호 설정 (AES-256, ZIP·7z)", FALSE);
    IShellItem* si = nullptr;
    if (!dir.empty() && SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(&si)))) { d->SetFolder(si); si->Release(); }
    bool ok = false;
    UINT type = last + 1;
    BOOL wantPw = FALSE;
    if (SUCCEEDED(d->Show(g_hwnd))) {
        d->GetFileTypeIndex(&type);
        if (cust) cust->GetCheckButtonState(1001, &wantPw);
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); ok = true; }
            it->Release();
        }
    }
    if (cust) cust->Release();
    d->Release();
    if (!ok) return false;
    if (type < 1 || type > n) type = 1;
    // 사용자가 직접 확장자를 바꿨으면 그 형식, 아니면 고른 형식의 확장자로 맞춘다
    std::wstring low = fmt::Lower(out);
    auto ends = [&](const wchar_t* e) { size_t k = wcslen(e); return low.size() >= k && low.compare(low.size() - k, k, e) == 0; };
    // 저장 창이 고른 형식의 확장자를 덧붙인 경우(예: 이름.7z 입력 + ZIP 형식 → 이름.7z.zip) 덧붙은 쪽을 뗀다
    for (auto e : SAVE_EXT) {
        if (!ends(e)) continue;
        std::wstring rest = low.substr(0, low.size() - wcslen(e));
        for (auto e2 : SAVE_EXT) {
            size_t k = wcslen(e2);
            if (e2 != e && rest.size() > k && rest.compare(rest.size() - k, k, e2) == 0 && (sz || !wcscmp(e2, L".zip"))) {
                out = out.substr(0, rest.size()); low = rest; break;
            }
        }
        break;
    }
    bool known = ends(L".zip") || (sz && (ends(L".7z") || ends(L".tar.gz") || ends(L".tgz") || ends(L".tar")));
    if (!known) {
        for (auto e : SAVE_EXT) if (ends(e)) { out = out.substr(0, out.size() - wcslen(e)); break; }
        out += SAVE_EXT[type - 1];
    }
    low = fmt::Lower(out);
    RegSet(L"Format", ends(L".7z") ? 1 : (ends(L".tar.gz") || ends(L".tgz")) ? 2 : ends(L".tar") ? 3 : 0);
    if (password && wantPw) {
        if (!AnyCanEncrypt(out)) { MsgBox(L"암호 설정", L"TAR / TAR.GZ 형식은 암호를 걸 수 없습니다.\nZIP 이나 7z 형식을 고르세요."); return false; }
        if (!AskNewPassword(*password, ends(L".7z"))) return false;
    }
    return true;
}

// ============================================================ 진행 창 (실시간 측정값)
// 작업 스레드는 Job 의 원자 카운터만 올리고, 이 창이 0.1초마다 읽어 그린다 → 압축 속도에 영향 없음
static ITaskbarList3* g_tbl = nullptr;
struct ProgState {
    HWND h = nullptr;
    DWORD start = 0;
    std::vector<std::pair<DWORD, uint64_t>> samples;
    double speed = 0;      // 바이트/초 (최근 3초 평균)
    RECT bBg{}, bCancel{};
    int hover = 0, press = 0;
    bool cancelling = false;
};
static ProgState g_prog;
static HWND ProgWindow() { return g_prog.h; }

static HWND TaskbarTarget() { return g_mainHidden ? g_prog.h : g_hwnd; }
static void TaskbarProgress(uint64_t done, uint64_t total, bool clear = false) {
    if (!g_tbl) {
        if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_tbl))) || !g_tbl) return;
        if (FAILED(g_tbl->HrInit())) { g_tbl->Release(); g_tbl = nullptr; return; }
    }
    HWND t = TaskbarTarget();
    if (!t) return;
    if (clear) { g_tbl->SetProgressState(t, TBPF_NOPROGRESS); return; }
    if (!total) { g_tbl->SetProgressState(t, TBPF_INDETERMINATE); return; }
    g_tbl->SetProgressState(t, TBPF_NORMAL);
    g_tbl->SetProgressValue(t, done, total);
}

static std::wstring FmtSpeed(double bps) {
    wchar_t s[32];
    if (bps >= 1024.0 * 1024 * 1024) swprintf(s, 32, L"%.1f GB/s", bps / (1024.0 * 1024 * 1024));
    else if (bps >= 10.0 * 1024 * 1024) swprintf(s, 32, L"%.0f MB/s", bps / (1024.0 * 1024));
    else if (bps >= 1024.0 * 1024) swprintf(s, 32, L"%.1f MB/s", bps / (1024.0 * 1024));
    else swprintf(s, 32, L"%.0f KB/s", bps / 1024.0);
    return s;
}
static std::wstring FmtLeft(double sec) {
    long long s = (long long)(sec + 0.5);
    if (s < 1) s = 1;
    if (s < 60) return L"약 " + std::to_wstring(s) + L"초";
    if (s < 3600) return L"약 " + std::to_wstring(s / 60) + L"분 " + std::to_wstring(s % 60) + L"초";
    return L"약 " + std::to_wstring(s / 3600) + L"시간 " + std::to_wstring((s % 3600) / 60) + L"분";
}
static std::wstring FmtMB(uint64_t b) {  // "640MB"
    std::wstring s = FmtSize(b);
    size_t k = s.find(L' ');
    if (k != std::wstring::npos) s.erase(k, 1);
    return s;
}

static void ProgSample() {
    if (!g_job) return;
    DWORD now = GetTickCount();
    uint64_t done = g_job->job.done;
    auto& v = g_prog.samples;
    if (!v.empty() && done < v.back().second) v.clear();  // 단계가 바뀌어 처음부터 다시 셈
    v.push_back({now, done});
    while (v.size() > 2 && now - v.front().first > 3000) v.erase(v.begin());
    DWORD dt = v.back().first - v.front().first;
    if (dt >= 400) g_prog.speed = (double)(v.back().second - v.front().second) * 1000.0 / dt;
}

static void ProgLayout(int W, int H) {
    int bh = S(34), y = H - S(20) - bh;
    int w1 = TextW(L"백그라운드로 작업", g_font) + S(32), w2 = max(S(84), TextW(L"취소", g_font) + S(32));
    g_prog.bCancel = {W - S(22) - w2, y, W - S(22), y + bh};
    g_prog.bBg = {g_prog.bCancel.left - S(8) - w1, y, g_prog.bCancel.left - S(8), y + bh};
}

static void ProgPaint(HWND h, HDC dc) {
    RECT cr; GetClientRect(h, &cr);
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    FillR(g, cr, 0, P.panel);
    StrokeR(g, cr, 0, P.border);
    ProgLayout(cr.right, cr.bottom);
    if (!g_job) return;
    JobState* j = g_job;
    int icon = j->kind == JK_EXTRACT ? IC_EXTRACT : j->kind == JK_ADD ? IC_PLUS : j->kind == JK_TEST || j->kind == JK_HASH ? IC_SHIELD : IC_ARCHIVE;
    RECT ib{S(24), S(24), S(24) + S(46), S(24) + S(46)};
    FillG(g, ib, SF(12), P.accent, P.accent2, 45);
    int isz = S(26);
    DrawIco(g, icon, (REAL)ib.left + (S(46) - isz) / 2, (REAL)ib.top + (S(46) - isz) / 2, (REAL)isz, P.onAccent, NONE, 1.8f);
    std::wstring title = g_prog.cancelling ? L"취소하는 중…"
                       : j->collecting ? L"파일 목록을 모으는 중…"
                       : j->kind == JK_EXTRACT ? (j->openAfter.empty() ? L"압축 푸는 중…" : L"파일 여는 중…")
                       : j->kind == JK_ADD ? L"파일 추가 중…"
                       : j->kind == JK_TEST ? L"압축 파일 검사 중…"
                       : j->kind == JK_HASH ? L"SHA-256 계산 중…"
                       : j->kind == JK_MODIFY ? j->modifyWhat + L" 중…"
                       : j->sfx ? L"자동 실행 파일 만드는 중…" : (j->split ? L"분할 압축 중…" : L"파일 압축 중…");
    int tx = ib.right + S(16), tr = cr.right - S(24);
    Txt(dc, {tx, S(24), tr, S(48)}, title, P.text, g_fontTitle);
    if (!g_queue.empty())  // 대기열
        Txt(dc, {tx, S(24), tr, S(48)}, L"대기 " + std::to_wstring(g_queue.size()) + L"개", P.accentFg, g_fontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    uint64_t done = j->job.done, total = j->job.total;
    std::wstring cur;
    EnterCriticalSection(&j->cs); cur = j->cur; LeaveCriticalSection(&j->cs);
    int pct = total ? (int)min<uint64_t>(100, done * 100 / total) : 0;
    RECT bar{S(24), S(90), cr.right - S(24), S(100)};
    FillR(g, bar, SF(5), P.track);
    RECT f = bar; f.right = bar.left + (int)((int64_t)(bar.right - bar.left) * pct / 100);
    if (f.right > f.left + S(6)) FillG(g, f, SF(5), P.accent, P.accent2 == NONE ? Mix(P.accent, RGB(255, 255, 255), 0.35f) : P.accent2);
    std::wstring p1 = std::to_wstring(pct) + L"% 완료";
    Txt(dc, {S(24), S(110), tr, S(132)}, p1, P.text, g_fontBold);
    uint32_t ft = j->filesTotal, fd = min<uint32_t>(j->filesDone, ft);
    if (!j->cum.empty() && j->cum.back() && total && (total > j->cum.back() ? total - j->cum.back() : j->cum.back() - total) <= j->cum.back() / 20) {
        // 처리한 바이트가 몇 번째 파일까지 왔는지 (7z 처럼 미리 읽는 방식에서도 정확)
        size_t k = (size_t)(std::upper_bound(j->cum.begin(), j->cum.end(), done) - j->cum.begin());
        fd = (uint32_t)min<size_t>(ft, k + 1);
        if (k < j->names.size() && !j->names[k].empty()) cur = j->names[k];
    }
    if (cur.empty()) cur = j->kind == JK_COMPRESS ? FileNameOf(j->target) : FileNameOf(g_arc.path);
    Txt(dc, {tx, S(50), tr, S(70)}, cur, P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    if (ft && !j->collecting) {
        int w = TextW(p1, g_fontBold) + S(6);
        Txt(dc, {S(24) + w, S(110), tr, S(132)}, L"(전체 " + std::to_wstring(ft) + L"개 중 " + std::to_wstring(max<uint32_t>(1, fd)) + L"개째)", P.dim, g_font);
    }
    bool early = GetTickCount() - g_prog.start < 600 || g_prog.speed <= 0;
    std::wstring sp = L"속도: " + (early ? std::wstring(L"측정 중…") : FmtSpeed(g_prog.speed));
    std::wstring left = L"남은 시간: " + (early || !total ? std::wstring(L"계산 중…") : FmtLeft((double)(total - min(done, total)) / g_prog.speed));
    if (total) left += L" (" + FmtMB(total) + L" 중 " + FmtMB(min(done, total)) + L" 처리됨)";
    Txt(dc, {S(24), S(136), tr, S(156)}, sp, P.dim, g_font);
    Txt(dc, {S(24), S(158), tr, S(178)}, left, P.dim, g_font);
    auto btn = [&](RECT r, const wchar_t* t, int id) {
        bool hv = g_prog.hover == id;
        FillR(g, r, SF(6), hv ? P.hover : P.btn);
        StrokeR(g, r, SF(6), P.btnBorder == NONE ? P.border : P.btnBorder);
        Txt(dc, r, t, P.text, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };
    btn(g_prog.bBg, L"백그라운드로 작업", 1);
    btn(g_prog.bCancel, L"취소", 2);
}

static void ProgBackground() {
    if (!g_prog.h) return;
    if (g_mainHidden) ShowWindow(g_prog.h, SW_MINIMIZE);
    else {
        ShowWindow(g_prog.h, SW_HIDE);
        g_progHidden = true;
        DoLayout(); InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}
static void ProgShow() {
    if (!g_prog.h) return;
    g_progHidden = false;
    ShowWindow(g_prog.h, g_mainHidden ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(g_prog.h);
    DoLayout(); InvalidateRect(g_hwnd, nullptr, FALSE);
}
static void ProgCancel() {
    if (!g_job) return;
    g_job->job.cancel = true;
    g_prog.cancelling = true;
    InvalidateRect(g_prog.h, nullptr, FALSE);
}

static LRESULT CALLBACK ProgProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        HDC md = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(md, bm);
        ProgPaint(h, md);
        BitBlt(dc, 0, 0, cr.right, cr.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER: {
        if (!g_job) return 0;
        ProgSample();
        if (!IsWindowVisible(h) && !g_progHidden && GetTickCount() - g_prog.start >= 250) {
            ShowWindow(h, SW_SHOW);
            if (g_mainHidden) SetForegroundWindow(h);
        }
        TaskbarProgress(g_job->job.done, g_job->job.total);
        if (IsWindowVisible(h) && !IsIconic(h)) InvalidateRect(h, nullptr, FALSE);
        if (g_progHidden) InvalidateRect(g_hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pt);
        if (PtInRect(&g_prog.bBg, pt) || PtInRect(&g_prog.bCancel, pt)) return HTCLIENT;
        return HTCAPTION;  // 어디를 잡아도 창 이동
    }
    case WM_SETCURSOR: SetCursor(LoadCursorW(nullptr, IDC_ARROW)); return TRUE;
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hv = PtInRect(&g_prog.bBg, pt) ? 1 : PtInRect(&g_prog.bCancel, pt) ? 2 : 0;
        if (hv != g_prog.hover) { g_prog.hover = hv; InvalidateRect(h, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: g_prog.hover = 0; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: g_prog.press = g_prog.hover; return 0;
    case WM_LBUTTONUP: {
        int p = g_prog.press;
        g_prog.press = 0;
        if (p && p == g_prog.hover) { if (p == 1) ProgBackground(); else ProgCancel(); }
        return 0;
    }
    case WM_KEYDOWN: if (w == VK_ESCAPE) ProgCancel(); return 0;
    case WM_CLOSE: ProgCancel(); return 0;
    }
    return DefWindowProcW(h, m, w, lp);
}

static void ProgOpen() {
    g_prog = ProgState();
    g_prog.start = GetTickCount();
    int W = S(500), H = S(250);
    RECT orc;
    if (g_mainHidden || !IsWindowVisible(g_hwnd) || IsIconic(g_hwnd)) {
        RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0); orc = wa;
    } else GetWindowRect(g_hwnd, &orc);
    std::wstring t = std::wstring(JobName()) + L" - " APP_NAME;
    g_prog.h = CreateWindowExW(g_mainHidden ? WS_EX_APPWINDOW : 0, L"ChagokProgress", t.c_str(), WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
                               (orc.left + orc.right - W) / 2, (orc.top + orc.bottom - H) / 2, W, H,
                               g_mainHidden ? nullptr : g_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    DWORD corner = 2;
    DwmSetWindowAttribute(g_prog.h, 33, &corner, sizeof(corner));
    ApplyOpacity(g_prog.h);
    ApplyIcon(g_prog.h);
    SetTimer(g_prog.h, 1, 100, nullptr);
}
static void ProgClose() {
    TaskbarProgress(0, 0, true);
    if (g_prog.h) { KillTimer(g_prog.h, 1); DestroyWindow(g_prog.h); }
    g_prog.h = nullptr;
    g_progHidden = false;
}

// ============================================================ 설정 창 (압축 설정 / 일반 설정)
static const int PRESET_LEVEL[3] = {3, 5, 9};
static const wchar_t* PRESET_NAME[3] = {L"빠름", L"보통", L"최대"};
static const wchar_t* PRESET_SUB[3] = {L"ZIP · Fast", L"7z · HC4", L"7z · LZMA2"};
static const wchar_t* PRESET_DESC[3] = {L"가장 빠르게 묶습니다. 어디서나 열립니다.", L"크기와 속도의 균형. 7z 로 만듭니다.", L"가장 작게 만듭니다. 오래 걸립니다."};
static const int PRESET_ICON[3] = {IC_BOLT, IC_SCALE, IC_STACK};
static const uint64_t SPLIT_PRESETS[] = {10, 20, 100, 700, 1024, 4095};
static const wchar_t* SPLIT_CHIPS[] = {L"10 MB", L"20 MB (메일)", L"100 MB", L"700 MB", L"1 GB", L"4 GB"};

static std::wstring LowerS(std::wstring s) { return fmt::Lower(s); }
static bool EndsI(const std::wstring& s, const wchar_t* e) {
    std::wstring l = LowerS(s);
    size_t k = wcslen(e);
    return l.size() >= k && l.compare(l.size() - k, k, e) == 0;
}
static bool IsTarPath(const std::wstring& p) { return EndsI(p, L".tar") || EndsI(p, L".tar.gz") || EndsI(p, L".tgz"); }
static std::wstring PresetExt(int preset) { return preset == 0 || !SzAvailable() ? L".zip" : L".7z"; }

struct CompressOpts { std::wstring target, password; bool split = false, sfx = false; int preset = 0; };

struct SetState {
    bool compress = false;
    int page = 1;
    CompressOpts o;
    bool encrypt = false;
    HWND h = nullptr, e1 = nullptr, e2 = nullptr;
    HBRUSH eb = nullptr;
    std::vector<std::pair<int, RECT>> hot;
    RECT e1Box{}, e2Box{}, slider{};
    int hover = 0, press = 0, result = 0;
    bool done = false, drag = false;
    int scroll = 0, scrollMax = 0, sdragOff = 0;  // 테마 앨범 스크롤
    bool sdrag = false;
    RECT album{}, sbar{}, sthumb{};
};
static SetState* g_set;
static bool ShellRegistered();
static bool ShellRegister();
static void ShellUnregister();
static std::wstring ExePath();
// ---- Windows 11 첫 화면 메뉴 (사내 인증서로 서명한 스파스 패키지 Sinsul.Chagok)
static bool IsWin11() {
    wchar_t b[32] = {}; DWORD sz = sizeof(b);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber", RRF_RT_REG_SZ, nullptr, b, &sz) != ERROR_SUCCESS) return false;
    return _wtoi(b) >= 22000;
}
static bool PackageRegistered() {  // 패키지 패밀리 이름으로 현재 사용자 등록 여부 확인
    typedef LONG(WINAPI * Fn)(PCWSTR, UINT32*, PWSTR*, UINT32*, WCHAR*);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetPackagesByPackageFamily");
    if (!fn) return false;
    UINT32 count = 0, len = 0;
    LONG r = fn(L"Sinsul.Chagok_q1acyza95e29j", &count, nullptr, &len, nullptr);
    return (r == ERROR_SUCCESS || r == ERROR_INSUFFICIENT_BUFFER) && count > 0;
}
// PowerShell 명령을 창 없이 실행하고 종료 코드를 돌려준다
static int RunPS(const std::wstring& script) {
    wchar_t win[MAX_PATH];
    GetWindowsDirectoryW(win, MAX_PATH);
    std::wstring exe = std::wstring(win) + L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring cmd = L"\"" + exe + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"$ErrorActionPreference = 'Stop'; " + script + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, 60000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return (int)code;
}
// 인증서가 이미 신뢰되어 있을 때만 새 메뉴 등록 (인증서 등록은 관리자 권한이라 설치 프로그램이 한다)
static bool RegisterModernMenu() {
    if (!IsWin11()) return false;
    std::wstring dir = ParentOf(ExePath());
    if (!FileExists(dir + L"\\ChagokShell.msix")) return false;
    int r = RunPS(L"if (-not (Get-ChildItem Cert:\\LocalMachine\\TrustedPeople | Where-Object Subject -eq 'CN=Sinsul Chagok')) { exit 2 }; "
                  L"Get-AppxPackage -Name Sinsul.Chagok | Remove-AppxPackage; "
                  L"Add-AppxPackage -Path '" + dir + L"\\ChagokShell.msix' -ExternalLocation '" + dir + L"'");
    if (r != 0 || !PackageRegistered()) return false;
    ShellUnregister();  // 예전 방식 메뉴와 중복되지 않도록
    return true;
}
static void CmdSetDefault();
static bool AskSplitCustom();

static void SetHot(int id, RECT r) { g_set->hot.push_back({id, r}); }

// 내용 영역 그리기와 클릭 영역 계산을 함께 한다 (g == nullptr 이면 계산만)
static void SetDraw(Graphics* g, HDC dc, int W, int H) {
    SetState* s = g_set;
    s->hot.clear();
    auto T = [&](RECT r, const std::wstring& t, COLORREF c, HFONT f, UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE) { if (g) Txt(dc, r, t, c, f, fmt); };
    auto toggle = [&](int id, int x, int y, bool on, bool en) {
        RECT r{x, y, x + S(42), y + S(24)};
        if (g) {
            FillR(*g, r, SF(12), !en ? P.track : on ? P.accent : P.track);
            if (!on) StrokeR(*g, r, SF(12), Mix(P.track, P.dim, 0.35f));
            int k = S(18), kx = on ? r.right - S(3) - k : r.left + S(3);
            FillR(*g, {kx, r.top + S(3), kx + k, r.top + S(3) + k}, (float)k / 2, on ? P.onAccent : (g_dark ? P.dim : RGB(255, 255, 255)));
            if (!on && !g_dark) StrokeR(*g, {kx, r.top + S(3), kx + k, r.top + S(3) + k}, (float)k / 2, Mix(P.track, P.dim, 0.35f));
        }
        if (en) SetHot(id, r);
    };
    auto chip = [&](int id, int& x, int& y, const std::wstring& t, bool cur, bool en) {
        int w = TextW(t, g_font) + S(26);
        int left = S(176) + S(26), right = W - S(26);  // 내용 영역 (아래 cx / cr 과 같음)
        if (x + w > right && x > left) { x = left; y += S(40); }  // 줄 바꿈
        RECT r{x, y, x + w, y + S(32)};
        if (g) {
            bool hv = s->hover == id && en;
            FillR(*g, r, SF(8), cur ? P.sel : hv ? P.hover : P.btn);
            StrokeR(*g, r, SF(8), cur ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), cur ? SF(1.4f) : 1);
            Txt(dc, r, t, !en ? P.dim : cur ? P.accentFg : P.text, cur ? g_fontBold : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        if (en) SetHot(id, r);
        x += w + S(8);
    };
    auto button = [&](int id, RECT r, const std::wstring& t, bool primary) {
        if (g) {
            bool hv = s->hover == id;
            if (primary) FillG(*g, r, SF(8), hv ? Mix(P.accent, g_dark ? RGB(255, 255, 255) : RGB(0, 0, 0), 0.1f) : P.accent, P.accent2);
            else { FillR(*g, r, SF(8), hv ? P.hover : P.btn); StrokeR(*g, r, SF(8), P.btnBorder == NONE ? P.border : P.btnBorder); }
            Txt(dc, r, t, primary ? P.onAccent : P.text, primary ? g_fontBold : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SetHot(id, r);
    };
    // 머리글
    T({S(24), 0, W - S(60), S(56)}, s->compress ? L"압축 설정" : L"설정", P.text, g_fontTitle);
    RECT xr{W - S(48), S(12), W - S(16), S(44)};
    if (g) { if (s->hover == 92) FillR(*g, xr, SF(6), P.hover); DrawIco(*g, IC_X, (REAL)xr.left + S(8), (REAL)xr.top + S(8), (REAL)S(16), P.dim); }
    SetHot(92, xr);
    // 왼쪽 메뉴
    int nw = S(176);
    if (g) { FillR(*g, {0, S(56), nw, H - S(68)}, 0, Mix(P.panel, P.text, g_dark ? 0.04f : 0.025f)); }
    // 메뉴 순서: 일반 · 테마 · 프리셋 · 보안 및 기타 (압축 설정에는 테마 없음). 클릭 id = 1 + 쪽 번호
    struct { const wchar_t* t; int ic, page; } nav[] = {{L"일반", IC_SLIDERS, 0}, {L"테마", IC_THEME, 3}, {L"프리셋", IC_STACK, 1}, {L"보안 및 기타", IC_SHIELD, 2}};
    int ni = 0;
    for (auto& n : nav) {
        if (n.page == 3 && s->compress) continue;
        RECT r{S(12), S(68) + ni * S(46), nw - S(12), S(68) + ni * S(46) + S(40)};
        ni++;
        bool cur = s->page == n.page;
        int i = &n - nav;
        if (g) {
            if (cur) FillR(*g, r, SF(8), P.sel);
            else if (s->hover == 1 + n.page) FillR(*g, r, SF(8), P.hover);
            if (cur) FillR(*g, {r.left, r.top + S(10), r.left + S(3), r.bottom - S(10)}, SF(1.5f), P.accentFg);
            DrawIco(*g, nav[i].ic, (REAL)r.left + S(14), (REAL)(r.top + r.bottom - S(18)) / 2, (REAL)S(18), cur ? P.accentFg : P.dim);
            Txt(dc, {r.left + S(42), r.top, r.right, r.bottom}, nav[i].t, cur ? P.text : P.dim, cur ? g_fontBold : g_font);
        }
        SetHot(1 + n.page, r);
    }
    int cx = nw + S(26), cr = W - S(26), y = S(70);
    auto head = [&](const wchar_t* t) { T({cx, y, cr, y + S(22)}, t, P.text, g_fontBold); y += S(30); };
    bool showPw = false;
    if (s->page == 0 && s->compress) {
        head(L"저장 위치");
        RECT box{cx, y, cr - S(100), y + S(36)};
        if (g) { FillR(*g, box, SF(6), P.bg); StrokeR(*g, box, SF(6), P.btnBorder == NONE ? P.border : P.btnBorder); }
        T({box.left + S(10), box.top, box.right - S(8), box.bottom}, s->o.target, P.text, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        button(40, {cr - S(92), y, cr, y + S(36)}, L"변경…", false);
        y += S(42);
        std::wstring fmtName = s->o.sfx ? L"자동 실행 파일 (ZIP)" : IsTarPath(s->o.target) ? (EndsI(s->o.target, L".tar") ? L"TAR" : L"TAR.GZ") : EndsI(s->o.target, L".7z") ? L"7z" : L"ZIP";
        T({cx, y, cr, y + S(20)}, L"형식: " + fmtName + L" (확장자를 바꾸면 형식도 바뀝니다)", P.dim, g_fontSmall);
        y += S(40);
        T({cx, y, cr - S(60), y + S(24)}, L"자동 실행 파일(.exe)로 만들기", P.text, g_fontBold);
        toggle(22, cr - S(42), y, s->o.sfx, true);
        y += S(26);
        T({cx, y, cr, y + S(20)}, L"차곡이 없는 PC 에서도 실행만 하면 압축이 풀립니다. (ZIP · 분할 불가 · 약 260KB 늘어남)", P.dim, g_fontSmall);
        y += S(34);
        T({cx, y, cr - S(60), y + S(24)}, L"분할 압축", s->o.sfx ? P.dim : P.text, g_fontBold);
        toggle(21, cr - S(42), y, s->o.split && !s->o.sfx, !s->o.sfx);
        y += S(34);
        bool sp = s->o.split && !s->o.sfx;
        T({cx, y, cr, y + S(20)}, L"조각 하나의 크기", sp ? P.dim : Mix(P.panel, P.dim, 0.6f), g_fontSmall);
        y += S(24);
        int x = cx;
        bool preset = false;
        for (int i = 0; i < 6; i++) { bool c = g_splitMB == SPLIT_PRESETS[i]; preset |= c; chip(30 + i, x, y, SPLIT_CHIPS[i], c && sp, sp); }
        chip(36, x, y, preset ? std::wstring(L"직접 입력…") : SplitLabel(g_splitMB), !preset && sp, sp);
    } else if (s->page == 3) {
        head(L"색상");
        int x = cx;
        const wchar_t* modes[] = {L"시스템 설정 따르기", L"라이트", L"다크"};
        for (int i = 0; i < 3; i++) chip(60 + i, x, y, modes[i], g_mode == i, true);
        y += S(48);
        head(L"테마");
        // 앨범: 4칸 격자, 이 영역만 스크롤
        RECT al{cx, y, cr, H - S(68) - S(10)};
        s->album = al;
        int cols = 4, gap = S(12), cw = (al.right - al.left - S(14) - (cols - 1) * gap) / cols;
        int th = cw * 62 / 100, chh = th + S(28), rowsN = (TH_COUNT + cols - 1) / cols;
        int total = rowsN * (chh + gap) - gap + S(4);
        s->scrollMax = max(0, total - (int)(al.bottom - al.top));
        s->scroll = max(0, min(s->scroll, s->scrollMax));
        int sv = 0;
        if (g) { sv = SaveDC(dc); IntersectClipRect(dc, al.left, al.top, al.right, al.bottom); g->SetClip(RF(al)); }
        for (int i = 0; i < TH_COUNT; i++) {
            int c = i % cols, rw = i / cols;
            RECT cell{al.left + c * (cw + gap), al.top + S(2) + rw * (chh + gap) - s->scroll, 0, 0};
            cell.right = cell.left + cw; cell.bottom = cell.top + chh;
            if (cell.bottom < al.top || cell.top > al.bottom) continue;
            bool cur = g_theme == i, hv = s->hover == 200 + i;
            if (g) {
                FillR(*g, cell, SF(10), cur ? P.sel : hv ? P.hover : P.btn);
                StrokeR(*g, cell, SF(10), cur ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), cur ? SF(2) : 1);
                RECT tr{cell.left + S(6), cell.top + S(6), cell.right - S(6), cell.top + th};
                DrawThemeThumb(*g, tr, i, g_dark);
                g->SetClip(RF(al));
                Txt(dc, {cell.left + S(4), tr.bottom + S(2), cell.right - S(4), cell.bottom - S(2)}, TH_NAMES[i], cur ? P.accentFg : P.text, cur ? g_fontBold : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (cur) { int k = S(18); RECT ck{tr.right - k - S(4), tr.top + S(4), tr.right - S(4), tr.top + S(4) + k}; FillR(*g, ck, (float)k / 2, P.accentFg); DrawIco(*g, IC_CHECK, (REAL)ck.left + S(3), (REAL)ck.top + S(3), (REAL)(k - S(6)), P.panel, NONE, 2.6f); }
            }
            RECT hr{cell.left, max(cell.top, al.top), cell.right, min(cell.bottom, al.bottom)};
            if (hr.bottom > hr.top) SetHot(200 + i, hr);
        }
        if (g) { g->ResetClip(); RestoreDC(dc, sv); }
        // 스크롤 막대
        s->sbar = {al.right - S(8), al.top, al.right - S(2), al.bottom};
        if (s->scrollMax > 0) {
            int vh = al.bottom - al.top, tl = max(S(30), vh * vh / total);
            int ty = al.top + (int)((int64_t)(vh - tl) * s->scroll / s->scrollMax);
            s->sthumb = {s->sbar.left, ty, s->sbar.right, ty + tl};
            if (g) { FillR(*g, s->sbar, SF(3), P.track); FillR(*g, s->sthumb, SF(3), s->hover == 81 || s->sdrag ? P.dim : P.scroll); }
            SetHot(81, {s->sbar.left - S(4), al.top, s->sbar.right + S(2), al.bottom});
        } else s->sthumb = {};
    } else if (s->page == 0) {
        head(L"프로그램 아이콘");
        for (int i = 0; i < 3; i++) {
            RECT r{cx + i * S(118), y, cx + i * S(118) + S(110), y + S(74)};
            bool cur = g_iconSel == i;
            if (g) {
                bool hv = s->hover == 100 + i;
                FillR(*g, r, SF(10), cur ? P.sel : hv ? P.hover : P.btn);
                StrokeR(*g, r, SF(10), cur ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), cur ? SF(1.6f) : 1);
                HICON ic = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IconResId(i)), IMAGE_ICON, S(36), S(36), 0);
                if (ic) { DrawIconEx(dc, (r.left + r.right - S(36)) / 2, r.top + S(8), ic, S(36), S(36), 0, nullptr, DI_NORMAL); DestroyIcon(ic); }
                Txt(dc, {r.left, r.top + S(48), r.right, r.bottom - S(4)}, ICON_NAMES[i], cur ? P.accentFg : P.text, cur ? g_fontBold : g_fontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SetHot(100 + i, r);
        }
        y += S(90);
        head(L"분할 크기 기본값");
        int x = cx;
        bool preset = false;
        for (int i = 0; i < 6; i++) { bool c = g_splitMB == SPLIT_PRESETS[i]; preset |= c; chip(30 + i, x, y, SPLIT_CHIPS[i], c, true); }
        chip(36, x, y, preset ? std::wstring(L"직접 입력…") : SplitLabel(g_splitMB), !preset, true);
    } else if (s->page == 1) {
        head(L"프리셋");
        int gap = S(12), cw = (cr - cx - 2 * gap) / 3, chh = S(118);
        bool tar = s->compress && IsTarPath(s->o.target);
        for (int i = 0; i < 3; i++) {
            RECT r{cx + i * (cw + gap), y, cx + i * (cw + gap) + cw, y + chh};
            bool en = i == 0 || SzAvailable();
            bool cur = s->o.preset == i;
            if (g) {
                bool hv = s->hover == 10 + i && en;
                FillR(*g, r, SF(10), cur ? P.sel : hv ? P.hover : P.btn);
                StrokeR(*g, r, SF(10), cur ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), cur ? SF(1.8f) : 1);
                DrawIco(*g, PRESET_ICON[i], (REAL)r.left + S(14), (REAL)r.top + S(14), (REAL)S(22), en ? P.accentFg : P.dim, NONE, 1.7f);
                if (cur) DrawIco(*g, IC_CHECK, (REAL)r.right - S(30), (REAL)r.top + S(14), (REAL)S(18), P.accentFg, NONE, 2.4f);
                Txt(dc, {r.left + S(14), r.top + S(44), r.right - S(8), r.top + S(66)}, PRESET_NAME[i], en ? P.text : P.dim, g_fontTitle);
                Txt(dc, {r.left + S(14), r.top + S(66), r.right - S(8), r.top + S(84)}, PRESET_SUB[i], en ? P.accentFg : P.dim, g_fontSmall);
                Txt(dc, {r.left + S(14), r.top + S(86), r.right - S(8), r.bottom - S(4)}, en ? PRESET_DESC[i] : L"7z.dll 이 없어 사용할 수 없습니다.", P.dim, g_fontTiny, DT_LEFT | DT_WORDBREAK);
            }
            if (en) SetHot(10 + i, r);
        }
        y += chh + S(10);
        if (tar) { T({cx, y, cr, y + S(20)}, L"TAR / TAR.GZ 형식은 프리셋 수준과 암호가 적용되지 않습니다.", P.dim, g_fontSmall); }
        else if (s->compress && s->o.sfx) { T({cx, y, cr, y + S(20)}, L"자동 실행 파일은 항상 ZIP 으로 만듭니다. (프리셋은 압축 수준만 적용)", P.dim, g_fontSmall); }
        y += S(26);
        head(L"상세 옵션");
        if (s->compress) {
            bool can = !tar;
            if (g) DrawIco(*g, IC_LOCK, (REAL)cx, (REAL)y + S(3), (REAL)S(18), can ? P.text : P.dim);
            T({cx + S(26), y, cr - S(60), y + S(24)}, L"암호화 (AES-256)", can ? P.text : P.dim, g_font);
            toggle(20, cr - S(42), y, s->encrypt && can, can);
            y += S(34);
            int hw = (cr - cx - S(16)) / 2;
            showPw = s->encrypt && can;
            COLORREF lc = showPw ? P.dim : Mix(P.panel, P.dim, 0.5f);
            T({cx, y, cx + hw, y + S(18)}, L"암호", lc, g_fontSmall);
            T({cx + hw + S(16), y, cr, y + S(18)}, L"암호 확인", lc, g_fontSmall);
            y += S(20);
            s->e1Box = {cx, y, cx + hw, y + S(36)};
            s->e2Box = {cx + hw + S(16), y, cr, y + S(36)};
            if (g) {
                for (int k = 0; k < 2; k++) {
                    RECT b = k ? s->e2Box : s->e1Box;
                    HWND e = k ? s->e2 : s->e1;
                    FillR(*g, b, SF(6), showPw ? P.bg : Mix(P.panel, P.bg, 0.5f));
                    StrokeR(*g, b, SF(6), showPw && GetFocus() == e ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), showPw && GetFocus() == e ? SF(1.5f) : 1);
                }
            }
            y += S(46);
        } else {
            T({cx, y, cr, y + S(20)}, L"암호는 파일을 압축할 때 설정 창에서 켭니다.", P.dim, g_fontSmall);
            y += S(30);
        }
        T({cx, y, cx + S(80), y + S(24)}, L"창 투명도", P.text, g_font);
        s->slider = {cx + S(86), y, cr - S(56), y + S(24)};
        if (g) {
            RECT tr = s->slider; tr.top += S(10); tr.bottom = tr.top + S(4);
            FillR(*g, tr, SF(2), P.track);
            int kx = s->slider.left + (s->slider.right - s->slider.left) * (g_opacity - 40) / 60;
            RECT fr = tr; fr.right = kx;
            FillG(*g, fr, SF(2), P.accent, P.accent2);
            RECT kn{kx - S(9), s->slider.top + S(3), kx + S(9), s->slider.top + S(21)};
            FillR(*g, kn, SF(9), RGB(255, 255, 255));
            StrokeR(*g, kn, SF(9), P.accent, SF(2));
            Txt(dc, {cr - S(50), y, cr, y + S(24)}, std::to_wstring(g_opacity) + L"%", P.dim, g_font, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        SetHot(80, s->slider);
    } else if (s->page == 2) {
        head(L"탐색기");
        T({cx, y, cr - S(60), y + S(24)}, L"우클릭 메뉴에 '" APP_NAME L"' 추가", P.text, g_font);
        bool pkg = PackageRegistered();
        toggle(70, cr - S(42), y, pkg || ShellRegistered(), true);
        y += S(30);
        T({cx, y, cr, y + S(36)}, pkg ? L"Windows 11 우클릭 첫 화면에 나타납니다. (사내 인증서로 서명된 메뉴)"
                                     : IsWin11() ? L"지금은 우클릭 → '추가 옵션 표시' 안에 나타납니다. 첫 화면에 넣으려면 설치 프로그램을 다시 실행하세요."
                                                 : L"우클릭하면 '" APP_NAME L"' 메뉴가 나타납니다.", P.dim, g_fontSmall, DT_LEFT | DT_WORDBREAK);
        y += S(42);
        T({cx, y, cr - S(120), y + S(34)}, L"기본 압축 프로그램으로 설정", P.text, g_font);
        button(71, {cr - S(112), y, cr, y + S(34)}, L"설정 열기…", false);
        y += S(52);
        if (!s->compress) {
            head(L"자동 업데이트");
            T({cx, y, cr - S(60), y + S(24)}, L"시작할 때 새 버전 확인 (지금 " + AppVersion() + L")", P.text, g_font);
            toggle(72, cr - S(42), y, RegGet(L"UpdateAuto", 1) != 0, true);
            y += S(34);
            std::wstring url = RegReadStr(REG_KEY, L"UpdateUrl");
            T({cx, y - S(2), cr - S(214), y + S(16)}, L"GitHub 에서 확인 · 안 되면 사내 위치", P.dim, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            T({cx, y + S(16), cr - S(214), y + S(34)}, url.empty() ? L"사내 위치 없음" : L"사내: " + url, url.empty() ? P.dim : P.text, g_fontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
            button(73, {cr - S(206), y, cr - S(110), y + S(34)}, L"사내 위치…", false);
            button(74, {cr - S(102), y, cr, y + S(34)}, L"지금 확인", false);
            y += S(48);
        }
        head(L"보안");
        T({cx, y, cr, y + S(110)},
          L"· 암호는 AES-256 으로 걸립니다. 암호를 잊으면 누구도 복구할 수 없습니다.\n"
          L"· ZIP 은 파일 이름 목록이 보이고 내용만 잠깁니다. 7z 는 이름까지 숨깁니다.\n"
          L"· TAR / TAR.GZ 형식은 암호를 지원하지 않습니다.\n"
          L"· 입력한 암호는 저장하지 않습니다.", P.dim, g_fontSmall, DT_LEFT | DT_WORDBREAK);
    }
    // 아래 버튼
    if (g) { FillR(*g, {1, H - S(68), W - 1, H - 1}, 0, Mix(P.panel, P.text, g_dark ? 0.04f : 0.025f)); HLine(*g, 1, W - 1, H - S(68), P.border); }
    int by = H - S(52), bh = S(36);
    if (s->compress) {
        button(91, {W - S(24) - S(96), by, W - S(24), by + bh}, L"취소", false);
        button(90, {W - S(24) - S(96) - S(8) - S(110), by, W - S(24) - S(96) - S(8), by + bh}, L"압축 시작", true);
    } else button(90, {W - S(24) - S(96), by, W - S(24), by + bh}, L"닫기", true);
    // 암호 입력란 위치/표시
    if (!g && s->e1) {
        bool vis = showPw && s->page == 1;
        int eh = S(20);
        auto place = [&](HWND e, RECT b) {
            SetWindowPos(e, nullptr, b.left + S(10), (b.top + b.bottom - eh) / 2, b.right - b.left - S(20), eh, SWP_NOZORDER | SWP_NOACTIVATE);
            ShowWindow(e, vis ? SW_SHOWNA : SW_HIDE);
        };
        place(s->e1, s->e1Box);
        place(s->e2, s->e2Box);
    }
}

static void SetRefresh() {
    RECT cr; GetClientRect(g_set->h, &cr);
    SetDraw(nullptr, nullptr, cr.right, cr.bottom);
    InvalidateRect(g_set->h, nullptr, FALSE);
}
static void SetScrollTo(int thumbTop) {  // 막대 위치 → 스크롤 값
    SetState* s = g_set;
    int vh = s->album.bottom - s->album.top, tl = s->sthumb.bottom - s->sthumb.top;
    if (vh - tl <= 0 || s->scrollMax <= 0) return;
    int v = (int)((int64_t)(thumbTop - s->album.top) * s->scrollMax / (vh - tl));
    v = max(0, min(s->scrollMax, v));
    if (v != s->scroll) { s->scroll = v; SetRefresh(); }
}
static void SetSlider(int x) {
    RECT r = g_set->slider;
    int v = 40 + (int)((int64_t)(x - r.left) * 60 / max(1L, r.right - r.left));
    v = max(40, min(100, v));
    if (v == g_opacity) return;
    g_opacity = v;
    ApplyOpacity(g_hwnd);
    ApplyOpacity(g_set->h);
    InvalidateRect(g_set->h, nullptr, FALSE);
}
static void SetApplyTheme() {
    ApplyTheme(true);
    if (g_set->eb) { DeleteObject(g_set->eb); g_set->eb = nullptr; }
    ApplyTitleBar(g_set->h);
    if (g_hasArc) { int c = g_cur; std::vector<char> ex = g_expand; ShowDir(c); g_expand = ex; }
    DoLayout(); InvalidateRect(g_hwnd, nullptr, FALSE);
    SetRefresh();
}

static bool AssocRegistered();
static void AssocRegister();
// 시작 메뉴·바탕 화면 바로 가기의 아이콘도 바꾼다
static void UpdateShortcutIcon(REFKNOWNFOLDERID fid) {
    PWSTR dir = nullptr;
    if (FAILED(SHGetKnownFolderPath(fid, 0, nullptr, &dir))) return;
    std::wstring lnk = std::wstring(dir) + L"\\" APP_NAME L".lnk";
    CoTaskMemFree(dir);
    if (!FileExists(lnk)) return;
    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) return;
    IPersistFile* pf = nullptr;
    if (SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) {
        if (SUCCEEDED(pf->Load(lnk.c_str(), STGM_READWRITE))) {
            wchar_t exe[MAX_PATH * 4];
            GetModuleFileNameW(nullptr, exe, MAX_PATH * 4);
            sl->SetIconLocation(exe, IconIndex(g_iconSel));
            pf->Save(lnk.c_str(), TRUE);
            SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, lnk.c_str(), nullptr);
        }
        pf->Release();
    }
    sl->Release();
}
static void SetAppIcon(int sel) {
    if (sel == g_iconSel) return;
    g_iconSel = sel;
    RegSet(L"Icon", (DWORD)sel);
    LoadAppIcons();
    SetClassLongPtrW(g_hwnd, GCLP_HICON, (LONG_PTR)g_iconBig);
    SetClassLongPtrW(g_hwnd, GCLP_HICONSM, (LONG_PTR)g_iconSmall);
    ApplyIcon(g_hwnd);
    if (g_set) ApplyIcon(g_set->h);
    if (g_prog.h) ApplyIcon(g_prog.h);
    if (AssocRegistered()) AssocRegister();  // 압축 파일 아이콘도 함께
    UpdateShortcutIcon(FOLDERID_Programs);
    UpdateShortcutIcon(FOLDERID_Desktop);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

static void SetClick(int id) {
    SetState* s = g_set;
    if (id >= 1 && id <= 4) { s->page = id - 1; SetRefresh(); if (s->page == 1 && s->encrypt && s->e1) SetFocus(s->e1); return; }
    if (id >= 10 && id <= 12) {
        s->o.preset = id - 10;
        if (s->compress && !IsTarPath(s->o.target) && !s->o.sfx) {
            std::wstring t = s->o.target;
            if (EndsI(t, L".zip")) t.resize(t.size() - 4); else if (EndsI(t, L".7z")) t.resize(t.size() - 3);
            s->o.target = t + PresetExt(s->o.preset);
        }
        if (!s->compress) { g_preset = s->o.preset; RegSet(L"Preset", (DWORD)g_preset); }
        SetRefresh();
        return;
    }
    if (id == 20) { s->encrypt = !s->encrypt; SetRefresh(); if (s->encrypt) SetFocus(s->e1); return; }
    if (id == 21) { s->o.split = !s->o.split; SetRefresh(); return; }
    if (id == 22) {  // 자동 실행 파일: 확장자를 .exe 로 (끄면 프리셋 형식으로)
        s->o.sfx = !s->o.sfx;
        std::wstring t = s->o.target;
        for (const wchar_t* e : {L".tar.gz", L".tgz", L".tar", L".zip", L".7z", L".exe"}) if (EndsI(t, e)) { t.resize(t.size() - wcslen(e)); break; }
        s->o.target = t + (s->o.sfx ? L".exe" : PresetExt(s->o.preset));
        if (s->o.sfx) s->o.split = false;
        SetRefresh();
        return;
    }
    if (id >= 30 && id <= 35) { g_splitMB = SPLIT_PRESETS[id - 30]; RegSet(L"SplitMB", (DWORD)g_splitMB); SetRefresh(); return; }
    if (id == 36) { EnableWindow(s->h, FALSE); AskSplitCustom(); EnableWindow(s->h, TRUE); if (!g_mainHidden) EnableWindow(g_hwnd, FALSE); SetForegroundWindow(s->h); SetRefresh(); return; }
    if (id == 40) {
        std::wstring dir = ParentOf(s->o.target), name = FileNameOf(s->o.target), out;
        for (const wchar_t* e : {L".tar.gz", L".tgz", L".tar", L".zip", L".7z"}) if (EndsI(name, e)) { name.resize(name.size() - wcslen(e)); break; }
        RegSet(L"Format", EndsI(s->o.target, L".7z") ? 1 : IsTarPath(s->o.target) ? (EndsI(s->o.target, L".tar") ? 3 : 2) : 0);
        EnableWindow(s->h, FALSE);
        bool ok = PickSave(out, dir, name);
        EnableWindow(s->h, TRUE); if (!g_mainHidden) EnableWindow(g_hwnd, FALSE);
        SetForegroundWindow(s->h);
        if (ok) {
            s->o.target = out;
            if (EndsI(out, L".zip") && s->o.preset != 0) s->o.preset = 0;
            else if (EndsI(out, L".7z") && s->o.preset == 0) s->o.preset = 1;
        }
        SetRefresh();
        return;
    }
    if (id >= 200 && id < 200 + TH_COUNT) { g_theme = id - 200; SetApplyTheme(); return; }
    if (id >= 60 && id <= 62) { g_mode = id - 60; SetApplyTheme(); return; }
    if (id >= 100 && id <= 102) { SetAppIcon(id - 100); SetRefresh(); return; }
    if (id == 72) { RegSet(L"UpdateAuto", RegGet(L"UpdateAuto", 1) ? 0 : 1); SetRefresh(); return; }
    if (id == 73) {
        DlgSpec d;
        d.title = L"사내 업데이트 위치";
        d.msg = L"새 버전은 GitHub 에서 먼저 확인하고, GitHub 에 연결되지 않을 때 이 위치를 봅니다.\n새 버전을 올려 두는 공유 폴더나 웹 주소를 넣으세요. (비워 두면 GitHub 만 확인)\n예) \\\\파일서버\\공유\\차곡   또는   https://intra.sinsul.com/chagok/";
        d.buttons = {L"확인", L"취소"};
        d.def = 0; d.cancel = 1;
        d.input = true; d.text = true;
        d.inputText = RegReadStr(REG_KEY, L"UpdateUrl");
        EnableWindow(s->h, FALSE);
        int r = RunDialog(s->h, d);
        EnableWindow(s->h, TRUE); if (!g_mainHidden) EnableWindow(g_hwnd, FALSE);
        SetForegroundWindow(s->h);
        if (r == 0) {
            std::wstring v = d.inputText;
            while (!v.empty() && (v.back() == L' ' || v.back() == L'"')) v.pop_back();
            while (!v.empty() && (v[0] == L' ' || v[0] == L'"')) v.erase(0, 1);
            RegStr(REG_KEY, L"UpdateUrl", v);
            RegDeleteKeyValueW(HKEY_CURRENT_USER, REG_KEY, L"UpdateSkip");
        }
        SetRefresh();
        return;
    }
    if (id == 74) { s->result = 0; s->done = true; PostMessageW(g_hwnd, WM_APP_UPDATE + 1, 0, 0); return; }
    if (id == 70) {
        SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        if (PackageRegistered() || ShellRegistered()) {
            if (PackageRegistered()) RunPS(L"Get-AppxPackage -Name Sinsul.Chagok | Remove-AppxPackage");
            ShellUnregister();
        } else if (!RegisterModernMenu() && !ShellRegister()) {
            EnableWindow(s->h, FALSE);
            MsgBox(L"우클릭 메뉴", L"ChagokShell.dll 파일이 Chagok.exe 와 같은 폴더에 있어야 합니다.\n설치 프로그램으로 설치하면 자동으로 들어갑니다.");
            EnableWindow(s->h, TRUE); if (!g_mainHidden) EnableWindow(g_hwnd, FALSE);
        }
        SetRefresh();
        return;
    }
    if (id == 71) { EnableWindow(s->h, FALSE); CmdSetDefault(); EnableWindow(s->h, TRUE); if (!g_mainHidden) EnableWindow(g_hwnd, FALSE); SetForegroundWindow(s->h); return; }
    if (id == 91 || id == 92) { s->result = 0; s->done = true; return; }
    if (id == 90) {
        if (!s->compress) { s->result = 1; s->done = true; return; }
        // 압축 시작: 입력 확인
        EnableWindow(s->h, FALSE);
        bool ok = true;
        if (s->encrypt && !IsTarPath(s->o.target)) {
            wchar_t a[256] = {}, b[256] = {};
            GetWindowTextW(s->e1, a, 256);
            GetWindowTextW(s->e2, b, 256);
            std::wstring p1 = a, p2 = b;
            SecureZeroMemory(a, sizeof(a)); SecureZeroMemory(b, sizeof(b));
            if (p1.empty()) { MsgBox(L"암호 설정", L"암호를 입력하세요."); ok = false; }
            else if (p1 != p2) { MsgBox(L"암호 설정", L"두 암호가 서로 다릅니다. 다시 입력하세요."); ok = false; }
            else if (p1.size() < 4) {
                DlgSpec w;
                w.title = L"짧은 암호";
                w.msg = L"암호가 너무 짧아 쉽게 풀릴 수 있습니다. 그대로 사용할까요?";
                w.buttons = {L"다시 입력", L"그대로 사용"};
                w.def = 0; w.cancel = 0;
                if (RunDialog(g_hwnd, w) == 0) ok = false;
            }
            if (ok) s->o.password = p1;
            if (!p1.empty()) SecureZeroMemory(&p1[0], p1.size() * sizeof(wchar_t));
            if (!p2.empty()) SecureZeroMemory(&p2[0], p2.size() * sizeof(wchar_t));
            if (!ok) { s->page = 1; SetRefresh(); }
        }
        if (ok && (FileExists(s->o.target) || DirExists(s->o.target))) {
            DlgSpec d;
            d.title = L"같은 이름의 파일이 있습니다";
            d.msg = s->o.target + L"\n\n덮어쓸까요?";
            d.buttons = {L"덮어쓰기", L"취소"};
            d.def = 1; d.cancel = 1;
            if (RunDialog(g_hwnd, d) != 0) { ok = false; s->page = 0; SetRefresh(); }
        }
        EnableWindow(s->h, TRUE); if (!g_mainHidden) EnableWindow(g_hwnd, FALSE);
        SetForegroundWindow(s->h);
        if (ok) { s->result = 1; s->done = true; }
        else if (s->encrypt && s->page == 1) SetFocus(s->e1);
        return;
    }
}

static LRESULT CALLBACK SetProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    SetState* s = g_set;
    if (!s) return DefWindowProcW(h, m, w, lp);
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        HDC md = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(md, bm);
        {
            Graphics g(md);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHalf);
            FillR(g, cr, 0, P.panel);
            StrokeR(g, cr, 0, P.border);
            HLine(g, 1, cr.right - 1, S(56), P.border);
            SetDraw(&g, md, cr.right, cr.bottom);
        }
        BitBlt(dc, 0, 0, cr.right, cr.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pt);
        if (pt.y < S(56)) { for (auto& hs : s->hot) if (PtInRect(&hs.second, pt)) return HTCLIENT; return HTCAPTION; }
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (s->drag) { SetSlider(pt.x); return 0; }
        if (s->sdrag) { SetScrollTo(pt.y - s->sdragOff); return 0; }
        int hv = 0;
        for (auto& hs : s->hot) if (PtInRect(&hs.second, pt)) hv = hs.first;
        if (hv != s->hover) { s->hover = hv; InvalidateRect(h, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: if (!s->drag && !s->sdrag) { s->hover = 0; InvalidateRect(h, nullptr, FALSE); } return 0;
    case WM_MOUSEWHEEL:
        if (s->page == 3 && s->scrollMax > 0) {
            s->scroll = max(0, min(s->scrollMax, s->scroll - GET_WHEEL_DELTA_WPARAM(w) * S(60) / WHEEL_DELTA));
            SetRefresh();
            POINT pt; GetCursorPos(&pt); ScreenToClient(h, &pt);
            int hv = 0; for (auto& hs : s->hot) if (PtInRect(&hs.second, pt)) hv = hs.first;
            if (hv != s->hover) { s->hover = hv; InvalidateRect(h, nullptr, FALSE); }
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        s->press = 0;
        for (auto& hs : s->hot) if (PtInRect(&hs.second, pt)) s->press = hs.first;
        if (s->press == 80) { s->drag = true; SetCapture(h); SetSlider(pt.x); }
        else if (s->press == 81) {  // 막대 잡기 (빈 곳을 누르면 그 자리로 이동)
            if (!PtInRect(&s->sthumb, pt)) SetScrollTo(pt.y - (s->sthumb.bottom - s->sthumb.top) / 2);
            s->sdragOff = pt.y - s->sthumb.top; s->sdrag = true; SetCapture(h);
        }
        else if (!s->press) SetFocus(h);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (s->drag) { s->drag = false; ReleaseCapture(); RegSet(L"Opacity", (DWORD)g_opacity); return 0; }
        if (s->sdrag) { s->sdrag = false; ReleaseCapture(); s->press = 0; InvalidateRect(h, nullptr, FALSE); return 0; }
        int id = 0;
        for (auto& hs : s->hot) if (PtInRect(&hs.second, pt)) id = hs.first;
        if (id && id == s->press) SetClick(id);
        s->press = 0;
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, P.text);
        SetBkColor(dc, P.bg);
        if (!s->eb) s->eb = CreateSolidBrush(P.bg);
        return (LRESULT)s->eb;
    }
    case WM_COMMAND:
        if (HIWORD(w) == EN_SETFOCUS || HIWORD(w) == EN_KILLFOCUS) InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CLOSE: s->result = 0; s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, lp);
}

// compress=true: 압축 설정 (opts 를 채워 돌려줌). false: 일반 설정
static void AlbumClose();
static bool RunSettings(bool compress, CompressOpts* opts, int page) {
    AlbumClose();
    SetState st;
    st.compress = compress;
    st.page = page;
    if (opts) st.o = *opts;
    st.o.preset = opts ? opts->preset : g_preset;
    g_set = &st;
    int W = S(700), H = S(590);
    HWND owner = g_hwnd;
    RECT orc;
    if (g_mainHidden || !IsWindowVisible(g_hwnd)) { RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0); orc = wa; }
    else GetWindowRect(owner, &orc);
    st.h = CreateWindowExW(g_mainHidden ? WS_EX_APPWINDOW : 0, L"ChagokSettings", compress ? L"압축 설정 - " APP_NAME : L"설정 - " APP_NAME, WS_POPUP | WS_SYSMENU,
                           (orc.left + orc.right - W) / 2, (orc.top + orc.bottom - H) / 2, W, H,
                           g_mainHidden ? nullptr : owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    DWORD corner = 2;
    DwmSetWindowAttribute(st.h, 33, &corner, sizeof(corner));
    ApplyOpacity(st.h);
    ApplyIcon(st.h);
    if (compress) {
        for (int k = 0; k < 2; k++) {
            HWND e = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | ES_PASSWORD | WS_TABSTOP, 0, 0, 10, 10, st.h, nullptr, nullptr, nullptr);
            SendMessageW(e, WM_SETFONT, (WPARAM)g_font, TRUE);
            (k ? st.e2 : st.e1) = e;
        }
    }
    SetRefresh();
    if (!g_mainHidden) EnableWindow(owner, FALSE);
    ShowWindow(st.h, SW_SHOW);
    SetForegroundWindow(st.h);
    SetFocus(st.h);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_CHAR && (msg.wParam == L'\r' || msg.wParam == 27)) {  // IME 가 키를 가로챈 경우 대비
            if (msg.wParam == 27) { st.result = 0; st.done = true; } else SetClick(90);
            continue;
        }
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) { st.result = 0; st.done = true; continue; }
            if (msg.wParam == VK_RETURN) { SetClick(90); continue; }
            if (msg.wParam == VK_TAB && st.e1 && IsWindowVisible(st.e1)) { SetFocus(GetFocus() == st.e1 ? st.e2 : st.e1); InvalidateRect(st.h, nullptr, FALSE); continue; }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) PostQuitMessage((int)msg.wParam);
    RegSet(L"Opacity", (DWORD)g_opacity);
    if (st.result && compress) {
        if (!st.encrypt || IsTarPath(st.o.target)) st.o.password.clear();
        g_preset = st.o.preset;
        RegSet(L"Preset", (DWORD)g_preset);
        if (!st.o.sfx) RegSet(L"Format", EndsI(st.o.target, L".7z") ? 1 : IsTarPath(st.o.target) ? (EndsI(st.o.target, L".tar") ? 3 : 2) : 0);
        if (st.o.sfx) st.o.split = false;
        *opts = st.o;
    }
    if (!g_mainHidden) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    DestroyWindow(st.h);
    if (st.eb) DeleteObject(st.eb);
    SecureZeroMemory(&st.o.password[0], st.o.password.size() * sizeof(wchar_t));
    g_set = nullptr;
    return st.result != 0;
}

// ============================================================ 동작
static void Relayout() { DoLayout(); InvalidateRect(g_hwnd, nullptr, FALSE); }
static void UpdateTitle() {
    std::wstring t = g_hasArc ? FileNameOf(g_arc.path) + L" - " APP_NAME : g_fs ? FsLabel(g_fsPath) + L" - " APP_NAME : APP_NAME;
    SetWindowTextW(g_hwnd, t.c_str());
}

// 압축할 때 걸 암호 (두 번 입력해 확인)
static bool AskNewPassword(std::wstring& pw, bool sevenZip) {
    for (;;) {
        DlgSpec d;
        d.title = L"암호 설정";
        d.msg = sevenZip ? L"AES-256 으로 암호화합니다. 파일 내용과 함께 파일 이름 목록도 암호 없이는 볼 수 없습니다."
                         : L"AES-256 으로 암호화합니다. (ZIP 은 파일 이름 목록은 보이고, 내용은 암호 없이 열 수 없습니다. 이름까지 숨기려면 7z 를 고르세요.)";
        d.buttons = {L"확인", L"취소"};
        d.def = 0; d.cancel = 1;
        d.input = true; d.password = true; d.confirm = true;
        d.label1 = L"암호"; d.label2 = L"암호 확인";
        d.note = std::wstring(L"· 암호를 잊으면 누구도 복구할 수 없습니다.\n· 8자 이상, 영문·숫자·기호를 섞으면 안전합니다.") +
                 (sevenZip ? L"" : L"\n· AES 암호 ZIP 은 Windows 탐색기 기본 기능으로는 열 수 없습니다. 차곡·7-Zip·반디집 등에서 열립니다.");
        if (RunDialog(g_hwnd, d) != 0) return false;
        if (d.inputText.empty()) { MsgBox(L"암호 설정", L"암호를 입력하세요."); continue; }
        if (d.inputText != d.inputText2) { MsgBox(L"암호 설정", L"두 암호가 서로 다릅니다. 다시 입력하세요."); continue; }
        if (d.inputText.size() < 4) {
            DlgSpec w;
            w.title = L"짧은 암호";
            w.msg = L"암호가 너무 짧아 쉽게 풀릴 수 있습니다. 그대로 사용할까요?";
            w.buttons = {L"다시 입력", L"그대로 사용"};
            w.def = 0; w.cancel = 0;
            if (RunDialog(g_hwnd, w) == 0) continue;
        }
        pw = d.inputText;
        SecureZeroMemory(&d.inputText[0], d.inputText.size() * sizeof(wchar_t));
        SecureZeroMemory(&d.inputText2[0], d.inputText2.size() * sizeof(wchar_t));
        return true;
    }
}
static bool AskPassword(std::wstring& pw, bool again) {
    DlgSpec d;
    d.title = L"암호 입력";
    d.msg = std::wstring(again ? L"암호가 틀렸습니다. 다시 입력하세요.\n\n" : L"") + L"암호로 보호된 압축 파일입니다.";
    d.buttons = {L"확인", L"취소"};
    d.def = 0; d.cancel = 1;
    d.input = true; d.password = true;
    if (RunDialog(g_hwnd, d) != 0 || d.inputText.empty()) return false;
    pw = d.inputText;
    return true;
}
static std::wstring* g_reopenPw = nullptr;
static bool OpenArchive(const std::wstring& pathIn, const std::wstring& dirPath = L"") {
    std::wstring err, p = pathIn;  // g_arc.path 를 넘겨받아도 안전하도록 복사
    std::function<bool(std::wstring&)> ask = [](std::wstring& pw) {
        if (g_reopenPw && !g_reopenPw->empty()) { pw = *g_reopenPw; g_reopenPw->clear(); return true; }  // 방금 건 암호
        return AskPassword(pw, false);
    };
    if (!g_arc.Open(p, err, ask)) {
        g_hasArc = false;
        g_dirs.clear(); g_rows.clear(); g_sel.clear();
        if (g_fs) FsShow(FileNameOf(p));
        UpdateTitle();
        Relayout();
        MsgBox(L"열 수 없음", FileNameOf(p) + L"\n" + err);
        return false;
    }
    g_hasArc = true;
    g_fs = false;
    if (g_fsWatch != INVALID_HANDLE_VALUE) { FindCloseChangeNotification(g_fsWatch); g_fsWatch = INVALID_HANDLE_VALUE; }
    AddRecent(g_arc.path);
    g_search.clear();
    if (g_searchEdit) { g_searchSilent = true; SetWindowTextW(g_searchEdit, L""); g_searchSilent = false; }
    BuildTree();
    int node = 0;
    if (!dirPath.empty())
        for (size_t i = 0; i < g_dirs.size(); i++) if (g_dirs[i].path == dirPath) node = (int)i;
    ShowDir(node);
    UpdateTitle();
    Relayout();
    return true;
}

// 작업 대기열: 지금 작업이 있으면 뒤에 줄 세운다
static bool ModifyingJob(const JobState* j) { return j && (j->kind == JK_ADD || j->kind == JK_MODIFY); }
static bool QueueAllowed() {  // 압축 파일을 바꾸는 작업이 진행/대기 중이면 같은 압축 파일을 다루는 작업은 받지 않는다
    if (ModifyingJob(g_job)) return false;
    for (auto* q : g_queue) if (ModifyingJob(q)) return false;
    return true;
}
static void StartJobNow(JobState* j);
static void StartJob(JobState* j) {
    if (g_job) {
        g_queue.push_back(j);
        if (g_prog.h) InvalidateRect(g_prog.h, nullptr, FALSE);
        return;
    }
    StartJobNow(j);
}
static void StartJobNow(JobState* j) {
    j->level = PRESET_LEVEL[max(0, min(2, g_preset))];
    g_job = j;
    g_progHidden = false;
    ProgOpen();
    DWORD tid;
    g_jobThread = CreateThread(nullptr, 0, JobThread, nullptr, 0, &tid);
    Relayout();
}

static bool AskSplitCustom() {
    DlgSpec d;
    d.title = L"분할 크기 직접 입력";
    d.msg = L"조각 하나의 크기를 MB 단위로 입력하세요. (1 ~ 1048576)";
    d.buttons = {L"확인", L"취소"};
    d.def = 0; d.cancel = 1;
    d.input = true;
    d.inputText = std::to_wstring(g_splitMB);
    d.inputSuffix = L"MB";
    if (RunDialog(g_hwnd, d) != 0) return false;
    uint64_t v = _wcstoui64(d.inputText.c_str(), nullptr, 10);
    if (v < 1 || v > 1048576) { MsgBox(L"분할 크기", L"1 ~ 1048576 사이의 값을 입력하세요."); return false; }
    g_splitMB = v;
    RegSet(L"SplitMB", (DWORD)v);
    return true;
}

static const wchar_t* SPLIT_NAMES[] = {L"10 MB", L"20 MB (메일 첨부)", L"100 MB", L"700 MB (CD)", L"1 GB", L"4 GB (FAT32 USB)"};

static void AppendSizeItems(std::vector<MItem>& v) {
    bool preset = false;
    for (int i = 0; i < 6; i++) {
        v.push_back({500 + i, SPLIT_NAMES[i], 0, g_splitMB == SPLIT_PRESETS[i], true});
        preset |= g_splitMB == SPLIT_PRESETS[i];
    }
    v.push_back({510, preset ? std::wstring(L"직접 입력…") : L"직접 입력… (" + SplitLabel(g_splitMB) + L")", 0, !preset, true});
}
static bool HandleSizeChoice(int id) {
    if (id >= 500 && id < 506) { g_splitMB = SPLIT_PRESETS[id - 500]; RegSet(L"SplitMB", (DWORD)g_splitMB); return true; }
    if (id == 510) return AskSplitCustom();
    return false;
}

static RECT BtnScreenRect(int id) {
    for (auto& b : L.btns)
        if (b.id == id) {
            RECT r = b.rc;
            MapWindowPoints(g_hwnd, nullptr, (POINT*)&r, 2);
            return r;
        }
    POINT p; GetCursorPos(&p);
    return {p.x, p.y, p.x, p.y};
}

static std::wstring UniqueFile(const std::wstring& base, const std::wstring& ext) {
    std::wstring p = base + ext;
    for (int i = 2; i < 1000 && (FileExists(p) || DirExists(p)); i++) p = base + L" (" + std::to_wstring(i) + L")" + ext;
    return p;
}
// 압축 설정 창(프리셋·암호·분할) → 작업 시작
static void StartCompress(std::vector<std::wstring> src, bool split) {
    if (src.empty()) return;
    std::wstring dir = ParentOf(src[0]);
    std::wstring name = src.size() == 1 ? FileNameOf(src[0]) : FileNameOf(dir);
    if (src.size() == 1 && !DirExists(src[0])) { size_t d = name.find_last_of(L'.'); if (d != std::wstring::npos && d > 0) name = name.substr(0, d); }
    if (name.empty() || name.back() == L':') name = L"압축";
    if (dir.size() == 2 && dir[1] == L':') dir += L"\\";
    CompressOpts o;
    o.preset = g_preset;
    if (o.preset > 0 && !SzAvailable()) o.preset = 0;
    std::wstring base = dir + (dir.empty() || dir.back() == L'\\' ? L"" : L"\\") + name;
    DWORD fmtLast = RegGet(L"Format", 0);  // 지난번에 TAR 계열을 골랐으면 그 형식 유지
    std::wstring ext = fmtLast == 2 && SzAvailable() ? L".tar.gz" : fmtLast == 3 && SzAvailable() ? L".tar" : PresetExt(o.preset);
    o.target = UniqueFile(base, ext);
    o.split = split;
    if (!RunSettings(true, &o, split ? 0 : 1)) return;
    JobState* j = new JobState();
    j->password = o.password;
    j->reopenPw = o.password;
    SecureZeroMemory(&o.password[0], o.password.size() * sizeof(wchar_t));
    j->kind = JK_COMPRESS;
    j->sources = src;
    j->target = o.target;
    j->split = o.split && !o.sfx ? g_splitMB * 1024 * 1024 : 0;
    j->sfx = o.sfx;
    StartJob(j);
}

static void CmdCompress(bool split) {
    std::vector<std::wstring> sel = FsSelPaths();
    if (!sel.empty()) {
        if (!split) { StartCompress(sel, false); return; }
        for (;;) {
            std::vector<MItem> v = {{3, L"선택한 " + std::to_wstring(sel.size()) + L"개 항목 분할 압축", 0, false, true},
                                    {0, L"", 1, false, false}, {0, L"분할 크기", 2, false, false}};
            AppendSizeItems(v);
            int id = RunPopup(g_hwnd, BtnScreenRect(ID_SPLIT), v);
            if (id >= 500) { HandleSizeChoice(id); Relayout(); continue; }
            if (id == 3) StartCompress(sel, true);
            return;
        }
    }
    int id = 0;
    for (;;) {
        std::vector<MItem> v = {{1, L"파일 선택…", 0, false, true}, {2, L"폴더 선택…", 0, false, true}};
        if (split) { v.push_back({0, L"", 1, false, false}); v.push_back({0, L"분할 크기", 2, false, false}); AppendSizeItems(v); }
        id = RunPopup(g_hwnd, BtnScreenRect(split ? ID_SPLIT : ID_COMPRESS), v);
        if (id >= 500) { HandleSizeChoice(id); Relayout(); continue; }
        break;
    }
    if (id != 1 && id != 2) return;
    std::vector<std::wstring> src;
    if (!PickPaths(src, id == 2, true, id == 2 ? L"압축할 폴더 선택" : L"압축할 파일 선택")) return;
    StartCompress(src, split);
}

static std::wstring UniqueDir(const std::wstring& base) {
    if (!FileExists(base) && !DirExists(base)) return base;
    for (int i = 2; i < 1000; i++) {
        std::wstring p = base + L" (" + std::to_wstring(i) + L")";
        if (!FileExists(p) && !DirExists(p)) return p;
    }
    return base;
}

enum { XT_PICK, XT_HERE, XT_FOLDER, XT_SMART };
static bool g_menuLaunch = false;   // 우클릭 메뉴(풀기)로 실행됨 → 작업 후 종료
static bool g_quietDone = false;    // 완료 대화상자 없이 종료

static std::wstring ArchiveStem() { return fmt::Stem(g_arc.path); }
static bool ExtractAllTo(int mode) {
    std::wstring dest, parent = ParentOf(g_arc.path);
    if (mode == XT_HERE) dest = parent;
    else if (mode == XT_FOLDER) dest = UniqueDir(parent + L"\\" + ArchiveStem());
    else if (mode == XT_SMART) {  // 알아서 풀기: 맨 위 항목(파일·폴더)이 하나면 현재 폴더에, 여러 개면 파일 이름 폴더에
        auto& root = g_dirs[0];
        dest = root.dirs.size() + root.files.size() <= 1 ? parent : UniqueDir(parent + L"\\" + ArchiveStem());
    }
    else {
        std::wstring dir;
        if (!PickFolder(dir, parent, L"압축을 풀 위치 선택")) return false;
        // 루트에 항목이 하나뿐이면 그대로, 여러 개면 "압축파일이름" 폴더를 만들어 그 안에
        auto& root = g_dirs[0];
        dest = dir;
        if (root.dirs.size() + root.files.size() > 1 && lstrcmpiW(FileNameOf(dir).c_str(), ArchiveStem().c_str()) != 0)
            dest = UniqueDir(dir + L"\\" + ArchiveStem());
    }
    JobState* j = new JobState();
    j->kind = JK_EXTRACT;
    for (size_t i = 0; i < g_arc.entries.size(); i++) j->idx.push_back((int)i);
    j->dest = dest;
    StartJob(j);
    return true;
}
static void FsExtract(const std::vector<std::wstring>& arcs, int mode);
static void CmdExtractAll() {
    if (g_fs) {
        std::vector<std::wstring> arcs = FsSelPaths(true, true);
        if (!arcs.empty()) {
            std::vector<MItem> v = {{4, L"알아서 풀기", 0, false, true}, {1, L"현재 폴더에 압축 풀기", 0, false, true},
                                    {2, L"파일 이름 폴더에 풀기", 0, false, true}, {3, L"풀 위치 선택…", 0, false, true}};
            int id = RunPopup(g_hwnd, BtnScreenRect(ID_EXTRACT_ALL), v);
            if (id) FsExtract(arcs, id == 4 ? XT_SMART : id == 1 ? XT_HERE : id == 2 ? XT_FOLDER : XT_PICK);
            return;
        }
    }
    if (!g_hasArc && Busy()) { MsgBox(L"압축 해제", L"진행 중인 작업이 끝난 뒤에 다른 압축 파일을 열 수 있습니다."); return; }
    if (g_hasArc && !QueueAllowed()) { MsgBox(L"압축 해제", L"압축 파일을 바꾸는 작업이 끝난 뒤에 다시 시도하세요."); return; }
    if (!g_hasArc) {
        std::vector<std::wstring> a;
        if (!PickPaths(a, false, false, L"압축 해제할 파일 선택", true) || !OpenArchive(a[0])) return;
    }
    ExtractAllTo(XT_PICK);
}

static void CmdExtractSel() {
    std::vector<int> idx;
    for (size_t i = 0; i < g_rows.size(); i++) {
        if (!g_sel[i]) continue;
        if (g_rows[i].kind == 1) CollectEntries(g_rows[i].idx, idx);
        else if (g_rows[i].kind == 2) idx.push_back(g_rows[i].idx);
    }
    if (idx.empty()) return;
    std::wstring dir;
    if (!PickFolder(dir, ParentOf(g_arc.path), L"선택 항목을 추출할 위치")) return;
    JobState* j = new JobState();
    j->kind = JK_EXTRACT;
    j->idx = idx;
    j->dest = dir;
    j->strip = CurStrip();
    StartJob(j);
}

static void StartAdd(const std::vector<std::wstring>& src) {
    JobState* j = new JobState();
    j->kind = JK_ADD;
    j->sources = src;
    j->prefix = g_cur == 0 ? L"" : g_dirs[(size_t)g_cur].path + L"/";
    j->target = g_dirs[(size_t)g_cur].path;
    j->arcPath = g_arc.path;
    StartJob(j);
}
static void CmdAdd() {
    if (g_arc.readOnly) { MsgBox(L"파일 추가 불가", g_arc.readOnlyWhy); return; }
    if (Busy()) { MsgBox(L"파일 추가", L"진행 중인 작업이 끝난 뒤에 할 수 있습니다."); return; }
    std::vector<std::wstring> src;
    if (!PickPaths(src, false, true, L"압축 파일에 추가할 파일 선택")) return;
    StartAdd(src);
}

// ============================================================ 탐색기 우클릭 메뉴 (클래식 메뉴, 서명 불필요)
static std::wstring ExePath() {
    wchar_t b[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, b, MAX_PATH * 4);
    return b;
}
static const wchar_t* SHELL_ROOTS[] = {L"Software\\Classes\\*\\shell\\ChagokCompress",
                                       L"Software\\Classes\\Directory\\shell\\ChagokCompress",
                                       L"Software\\Classes\\SystemFileAssociations\\.zip\\shell\\ChagokExtract",
                                       L"Software\\Classes\\SystemFileAssociations\\.z01\\shell\\ChagokExtract",
                                       L"Software\\Classes\\SystemFileAssociations\\.001\\shell\\ChagokExtract"};
static void RegStr(const std::wstring& key, const wchar_t* name, const std::wstring& val) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}
static std::wstring RegReadStr(const std::wstring& key, const wchar_t* name) {
    wchar_t b[2048] = {};
    DWORD sz = sizeof(b);
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, b, &sz) != ERROR_SUCCESS) return L"";
    return b;
}
// 우클릭 메뉴는 ChagokShell.dll(IContextMenu 확장)이 선택 내용을 보고 항목을 바꿔 보여 준다.
#define CHAGOK_CLSID L"{8D0C9DD2-90A8-492A-AEE8-45AF3C6D4EFD}"
static const wchar_t* HANDLER_KEY = L"Software\\Classes\\AllFilesystemObjects\\shellex\\ContextMenuHandlers\\Chagok";
static const wchar_t* CLSID_KEY = L"Software\\Classes\\CLSID\\" CHAGOK_CLSID;
static std::wstring ShellDllPath() { return ParentOf(ExePath()) + L"\\ChagokShell.dll"; }
static bool ShellRegistered() {
    return !RegReadStr(HANDLER_KEY, L"").empty();
}
static void ShellUnregister() {
    RegDeleteTreeW(HKEY_CURRENT_USER, HANDLER_KEY);
    RegDeleteTreeW(HKEY_CURRENT_USER, CLSID_KEY);
    // 이전 버전(정적 메뉴: ChagokCompress/ChagokExtract, 그 전 이름 Zipper*) 정리
    for (auto r : SHELL_ROOTS) {
        std::wstring o = r;
        RegDeleteTreeW(HKEY_CURRENT_USER, o.c_str());
        size_t k = o.find(L"Chagok");
        if (k != std::wstring::npos) { o.replace(k, 6, L"Zipper"); RegDeleteTreeW(HKEY_CURRENT_USER, o.c_str()); }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
static bool ShellRegister() {
    ShellUnregister();
    if (!FileExists(ShellDllPath())) return false;
    RegStr(CLSID_KEY, L"", L"차곡 탐색기 메뉴");
    RegStr(std::wstring(CLSID_KEY) + L"\\InprocServer32", L"", ShellDllPath());
    RegStr(std::wstring(CLSID_KEY) + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    RegStr(HANDLER_KEY, L"", CHAGOK_CLSID);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}
// exe 를 옮긴 경우 등록된 경로를 자동으로 갱신
// ============================================================ 기본 프로그램 후보 등록
// Windows 10/11은 프로그램이 스스로 기본 앱이 될 수 없다(사용자 선택 보호).
// 대신 '연결 프로그램' 후보와 '기본 앱' 설정 목록에 올리고, 사용자가 설정 창에서 고르게 한다.
static const wchar_t* PROGID = L"Chagok.zip";          // ZIP 계열
static const wchar_t* PROGID2 = L"Chagok.archive";     // 그 밖의 압축 형식
// 연결 후보로 등록할 확장자 (.z01 / .001 포함)
static std::vector<std::pair<std::wstring, const wchar_t*>> AssocExts() {
    std::vector<std::pair<std::wstring, const wchar_t*>> v;
    for (auto e : fmt::ZIP_EXTS) v.push_back({std::wstring(L".") + e, PROGID});
    v.push_back({L".z01", PROGID});
    v.push_back({L".001", PROGID2});
    for (auto e : fmt::SZ_EXTS) v.push_back({std::wstring(L".") + e, PROGID2});
    return v;
}
static const wchar_t* CAPS_KEY = L"Software\\Sinsul\\ChagokApp\\Capabilities";
static bool AssocRegistered() {
    return !RegReadStr(L"Software\\RegisteredApplications", L"Chagok").empty();
}
static void AssocUnregister() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Chagok.zip");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Chagok.archive");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\Chagok.exe");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Sinsul\\ChagokApp");
    for (auto& e : AssocExts()) {
        HKEY k;
        std::wstring key = std::wstring(L"Software\\Classes\\") + e.first + L"\\OpenWithProgids";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
            RegDeleteValueW(k, PROGID);
            RegDeleteValueW(k, PROGID2);
            RegCloseKey(k);
        }
    }
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteValueW(k, L"Chagok");
        RegCloseKey(k);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
static void AssocRegister() {
    std::wstring exe = L"\"" + ExePath() + L"\"";
    for (int k = 0; k < 2; k++) {
        std::wstring pk = std::wstring(L"Software\\Classes\\") + (k ? PROGID2 : PROGID);
        const wchar_t* desc = k ? L"압축 파일" : L"ZIP 압축 파일";
        RegStr(pk, L"", desc);
        RegStr(pk, L"FriendlyTypeName", desc);
        RegStr(pk + L"\\DefaultIcon", L"", exe + L"," + std::to_wstring(IconIndex(g_iconSel)));
        RegStr(pk + L"\\shell", L"", L"open");
        RegStr(pk + L"\\shell\\open", L"FriendlyAppName", APP_NAME);
        RegStr(pk + L"\\shell\\open\\command", L"", exe + L" \"%1\"");
    }
    std::wstring ak = L"Software\\Classes\\Applications\\Chagok.exe";
    RegStr(ak, L"FriendlyAppName", APP_NAME);
    RegStr(ak + L"\\DefaultIcon", L"", exe + L"," + std::to_wstring(IconIndex(g_iconSel)));
    RegStr(ak + L"\\shell\\open\\command", L"", exe + L" \"%1\"");
    for (auto& e : AssocExts()) {
        RegStr(std::wstring(L"Software\\Classes\\") + e.first + L"\\OpenWithProgids", e.second, L"");
        RegStr(ak + L"\\SupportedTypes", e.first.c_str(), L"");
        RegStr(std::wstring(CAPS_KEY) + L"\\FileAssociations", e.first.c_str(), e.second);
    }
    RegStr(CAPS_KEY, L"ApplicationName", APP_NAME);
    RegStr(CAPS_KEY, L"ApplicationDescription", L"압축 · 분할 압축 · 압축 해제");
    RegStr(CAPS_KEY, L"ApplicationIcon", exe + L"," + std::to_wstring(IconIndex(g_iconSel)));
    RegStr(L"Software\\RegisteredApplications", L"Chagok", CAPS_KEY);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
static void OpenDefaultAppsSettings() {
    // Windows 11: 차곡 항목 페이지로 바로, Windows 10: 기본 앱 페이지
    if ((INT_PTR)ShellExecuteW(g_hwnd, L"open", L"ms-settings:defaultapps?registeredAppUser=Chagok", nullptr, nullptr, SW_SHOWNORMAL) <= 32)
        ShellExecuteW(g_hwnd, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
}
static void CmdSetDefault() {
    AssocRegister();
    DlgSpec d;
    d.title = L"기본 압축 프로그램으로 설정";
    d.msg = L"Windows 설정의 '기본 앱' 화면을 엽니다.\n\n"
            L"· Windows 11: 차곡 화면 맨 위 [기본값 설정]을 누르면 zip·7z·rar 등이 한 번에 바뀝니다.\n"
            L"   (버튼이 없으면 .zip 등 확장자를 하나씩 눌러 '차곡' 선택)\n"
            L"· Windows 10: 아래쪽 '파일 형식별 기본 앱 선택' → .zip / .7z … → '차곡'\n\n"
            L"또는 ZIP 파일 우클릭 → 연결 프로그램 → 다른 앱 선택 → 차곡 → [항상]";
    d.buttons = {L"설정 열기", L"닫기"};
    d.def = 0; d.cancel = 1;
    if (RunDialog(g_hwnd, d) == 0) OpenDefaultAppsSettings();
}

// exe 를 옮긴 경우 등록된 경로를 자동으로 갱신
static void ShellRefreshPath() {
    if (AssocRegistered()) {
        std::wstring cmd = RegReadStr(std::wstring(L"Software\\Classes\\") + PROGID + L"\\shell\\open\\command", L"");
        if (cmd.find(ExePath()) == std::wstring::npos) AssocRegister();
    }
    // 이전 버전의 정적 메뉴가 남아 있으면 새 메뉴로 교체
    HKEY k;
    bool legacy = RegOpenKeyExW(HKEY_CURRENT_USER, SHELL_ROOTS[0], 0, KEY_READ, &k) == ERROR_SUCCESS;
    if (legacy) RegCloseKey(k);
    if (legacy) { ShellRegister(); return; }
    if (!ShellRegistered()) return;
    if (lstrcmpiW(RegReadStr(std::wstring(CLSID_KEY) + L"\\InprocServer32", L"").c_str(), ShellDllPath().c_str()) != 0) ShellRegister();
}

static void SetThemeMode(int theme, int mode) {
    bool changed = theme != g_theme;
    g_theme = theme; g_mode = mode;
    ApplyTheme(true);
    if (changed && g_hasArc) { int c = g_cur; std::vector<char> ex = g_expand; ShowDir(c); g_expand = ex; }
    Relayout();
}
// ---- 테마 앨범: 테마 버튼에 커서를 올리거나 누르면 뜬다. 누르면 바로 적용되고 앨범은 그대로, 커서가 벗어나면 닫힌다
static HWND g_album;
static int g_albumHover;
static std::vector<std::pair<int, RECT>> g_albumHot;
static ULONGLONG g_albumOut;
static const int AL_COLS = 5;
static SIZE AlbumSize() {
    int pad = S(14), cw = S(108), gap = S(8), th = S(66), rows = (TH_COUNT + AL_COLS - 1) / AL_COLS;
    int w = 2 * pad + AL_COLS * cw + (AL_COLS - 1) * gap;
    int h = pad + S(22) + S(32) + S(14) + S(22) + rows * (th + S(28)) + (rows - 1) * gap + pad;
    return {w, h};
}
static void AlbumDraw(Graphics* g, HDC dc, int W, int H) {
    g_albumHot.clear();
    int pad = S(14), cw = S(108), gap = S(8), th = S(66), y = pad;
    if (g) {
        FillR(*g, {0, 0, W, H}, 0, P.panel);
        StrokeR(*g, {0, 0, W, H}, 0, P.border);
        Txt(dc, {pad, y, W - pad, y + S(20)}, L"색상", P.dim, g_fontSmall);
    }
    y += S(22);
    const wchar_t* modes[] = {L"시스템 설정 따르기", L"라이트", L"다크"};
    int x = pad;
    for (int i = 0; i < 3; i++) {
        int w = TextW(modes[i], g_font) + S(26);
        RECT r{x, y, x + w, y + S(32)};
        bool cur = g_mode == i, hv = g_albumHover == 60 + i;
        if (g) {
            FillR(*g, r, SF(16), cur ? P.sel : hv ? P.hover : P.btn);
            StrokeR(*g, r, SF(16), cur ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), cur ? SF(1.4f) : 1);
            Txt(dc, r, modes[i], cur ? P.accentFg : P.text, cur ? g_fontBold : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        g_albumHot.push_back({60 + i, r});
        x += w + S(8);
    }
    y += S(32) + S(14);
    if (g) Txt(dc, {pad, y, W - pad, y + S(20)}, L"테마", P.dim, g_fontSmall);
    y += S(22);
    for (int i = 0; i < TH_COUNT; i++) {
        int c = i % AL_COLS, rw = i / AL_COLS;
        RECT cell{pad + c * (cw + gap), y + rw * (th + S(28) + gap), 0, 0};
        cell.right = cell.left + cw; cell.bottom = cell.top + th + S(28);
        bool cur = g_theme == i, hv = g_albumHover == 200 + i;
        if (g) {
            FillR(*g, cell, SF(10), cur ? P.sel : hv ? P.hover : NONE);
            if (cur || hv) StrokeR(*g, cell, SF(10), cur ? P.accentFg : (P.btnBorder == NONE ? P.border : P.btnBorder), cur ? SF(2) : 1);
            RECT tr{cell.left + S(5), cell.top + S(5), cell.right - S(5), cell.top + th};
            DrawThemeThumb(*g, tr, i, g_dark);
            if (!cur && !hv) StrokeR(*g, tr, SF(6), P.border);
            Txt(dc, {cell.left + S(2), tr.bottom + S(2), cell.right - S(2), cell.bottom - S(2)}, TH_NAMES[i], cur ? P.accentFg : P.text, cur ? g_fontBold : g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (cur) { int k = S(18); RECT ck{tr.right - k - S(4), tr.top + S(4), tr.right - S(4), tr.top + S(4) + k}; FillR(*g, ck, (float)k / 2, P.accentFg); DrawIco(*g, IC_CHECK, (REAL)ck.left + S(3), (REAL)ck.top + S(3), (REAL)(k - S(6)), P.panel, NONE, 2.6f); }
        }
        g_albumHot.push_back({200 + i, cell});
    }
}
static void AlbumClose() {
    if (!g_album) return;
    HWND a = g_album; g_album = nullptr;
    KillTimer(g_hwnd, 79);
    DestroyWindow(a);
}
static void AlbumOpen() {
    if (g_album) return;
    RECT btn = BtnScreenRect(ID_THEME), mw; GetWindowRect(g_hwnd, &mw);
    SIZE sz = AlbumSize();
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromRect(&btn, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;
    bool leftRail = btn.left - mw.left < S(140) && btn.top - mw.top > S(160);  // 왼쪽 세로 메뉴에 있는 버튼
    int x, y;
    if (!leftRail && btn.bottom + S(6) + sz.cy <= wa.bottom) {  // 아래
        y = btn.bottom + S(6);
        x = (btn.left + btn.right) / 2 < (mw.left + mw.right) / 2 ? btn.left : btn.right - sz.cx;
    } else if (btn.right + S(8) + sz.cx <= wa.right) {  // 오른쪽 (아래 끝 맞춤)
        x = btn.right + S(8);
        y = btn.bottom - sz.cy;
    } else {  // 위
        y = btn.top - S(6) - sz.cy;
        x = btn.right - sz.cx;
    }
    x = max((int)wa.left + S(4), min(x, (int)(wa.right - S(4) - sz.cx)));
    y = max((int)wa.top + S(4), min(y, (int)(wa.bottom - S(4) - sz.cy)));
    g_albumHover = 0; g_albumOut = 0;
    g_album = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"ChagokAlbum", L"", WS_POPUP, x, y, sz.cx, sz.cy, g_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    DWORD corner = 2;
    DwmSetWindowAttribute(g_album, 33, &corner, sizeof(corner));
    ShowWindow(g_album, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, 79, 100, nullptr);
}
// 100ms 마다: 커서가 앨범·테마 버튼 밖에 0.4초 넘게 있으면 닫는다
static void AlbumTick() {
    if (!g_album) { KillTimer(g_hwnd, 79); return; }
    HWND fg = GetForegroundWindow();
    if (fg != g_hwnd && fg != g_album) { AlbumClose(); return; }
    POINT pt; GetCursorPos(&pt);
    RECT ar, br = BtnScreenRect(ID_THEME);
    GetWindowRect(g_album, &ar);
    InflateRect(&ar, S(6), S(6));
    InflateRect(&br, S(4), S(4));
    if (PtInRect(&ar, pt) || PtInRect(&br, pt)) { g_albumOut = 0; return; }
    if (!g_albumOut) g_albumOut = GetTickCount64();
    else if (GetTickCount64() - g_albumOut >= 400) AlbumClose();
}
static LRESULT CALLBACK AlbumProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    switch (m) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        HDC md = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(md, bm);
        {
            Graphics g(md);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHalf);
            AlbumDraw(&g, md, cr.right, cr.bottom);
        }
        BitBlt(dc, 0, 0, cr.right, cr.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (g_albumHot.empty()) { RECT cr; GetClientRect(h, &cr); AlbumDraw(nullptr, nullptr, cr.right, cr.bottom); }
        int hv = 0;
        for (auto& hs : g_albumHot) if (PtInRect(&hs.second, pt)) hv = hs.first;
        if (hv != g_albumHover) { g_albumHover = hv; InvalidateRect(h, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: g_albumHover = 0; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int id = 0;
        for (auto& hs : g_albumHot) if (PtInRect(&hs.second, pt)) id = hs.first;
        if (id >= 200 && id < 200 + TH_COUNT) SetThemeMode(id - 200, g_mode);
        else if (id >= 60 && id <= 62) SetThemeMode(g_theme, id - 60);
        else return 0;
        if (g_album) InvalidateRect(g_album, nullptr, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, lp);
}
static void CmdTheme() { AlbumOpen(); }

static void ClearSearch();
static void EnsureVisible(int i);
// ---- 내 PC 둘러보기: 이동·새로 고침·풀기
static bool ArcInUse() {  // 진행·대기 중인 작업이 지금 열린 압축 파일을 쓰는지
    auto uses = [](const JobState* j) { return j && j->kind != JK_COMPRESS && !(j->kind == JK_HASH && !j->hashFiles.empty()); };
    if (uses(g_job)) return true;
    for (auto* q : g_queue) if (uses(q)) return true;
    return false;
}
static void FsWatchReset() {
    if (g_fsWatch != INVALID_HANDLE_VALUE) { FindCloseChangeNotification(g_fsWatch); g_fsWatch = INVALID_HANDLE_VALUE; }
    if (g_fs && !g_fsPath.empty())
        g_fsWatch = FindFirstChangeNotificationW(LongPath(g_fsPath).c_str(), FALSE,
                                                 FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
}
static void FsNavigate(const std::wstring& pathIn, const std::wstring& focus = L"") {
    std::wstring p = FsNorm(pathIn), old = g_fsPath;
    bool wasFs = g_fs;
    if (!p.empty() && !DirExists(p)) { MsgBox(L"폴더 열기", L"폴더를 찾을 수 없습니다.\n" + p); return; }
    g_fs = true;
    g_fsPath = p;
    if (!FsLoad()) {
        DWORD e = GetLastError();
        g_fsPath = old; g_fs = wasFs;
        if (g_fs) FsLoad();
        MsgBox(L"폴더 열기", (e == ERROR_ACCESS_DENIED ? L"이 폴더를 열 권한이 없습니다.\n" : L"폴더를 열 수 없습니다.\n") + p);
        if (!g_fs) return;
        p = old;
    }
    ClearSearch();
    FsShow(focus);
    g_treeScroll = 0;
    RegStr(REG_KEY, L"BrowsePath", g_fsPath);
    FsWatchReset();
    UpdateTitle();
    Relayout();
    if (g_focus >= 0) { EnsureVisible(g_focus); Relayout(); }
}
static void EnterBrowse(std::wstring path, const std::wstring& focus = L"") {
    if (g_hasArc) {
        if (ArcInUse()) { MsgBox(L"폴더 보기", L"이 압축 파일을 쓰는 작업이 끝난 뒤에 나갈 수 있습니다."); return; }
        g_arc.Close();
        g_hasArc = false;
        g_dirs.clear(); g_expand.clear(); g_rows.clear(); g_sel.clear();
        g_cur = 0;
    }
    BuildPlaces();
    path = FsNorm(path);
    if (!path.empty() && !DirExists(path)) path.clear();
    FsNavigate(path, focus);
}
static void ExitBrowse() {
    g_fs = false;
    ClearSearch();
    g_rows.clear(); g_sel.clear(); g_fsItems.clear();
    g_focus = g_anchor = -1; g_scrollY = 0;
    FsWatchReset();
    UpdateTitle();
    Relayout();
}
static void FsRefresh(const std::wstring& select = L"") {
    if (!g_fs) return;
    if (!g_fsPath.empty() && !DirExists(g_fsPath)) { FsNavigate(FsParent(g_fsPath)); return; }  // 지금 폴더가 지워짐
    std::vector<std::wstring> sel;
    for (size_t i = 0; i < g_rows.size(); i++) if (g_sel[i]) sel.push_back(g_fsItems[(size_t)g_rows[i].idx].name);
    std::wstring foc = g_focus >= 0 && g_focus < (int)g_rows.size() ? g_fsItems[(size_t)g_rows[(size_t)g_focus].idx].name : L"";
    int sy = g_scrollY;
    if (g_fsPath.empty()) BuildPlaces();
    if (!FsLoad()) return;
    FsShow();
    if (!select.empty()) { sel = {select}; foc = select; }
    for (size_t i = 0; i < g_rows.size(); i++) {
        const std::wstring& n = g_fsItems[(size_t)g_rows[i].idx].name;
        for (auto& x : sel) if (!lstrcmpiW(x.c_str(), n.c_str())) g_sel[i] = 1;
        if (!foc.empty() && !lstrcmpiW(foc.c_str(), n.c_str())) g_focus = g_anchor = (int)i;
    }
    g_scrollY = sy;
    Relayout();
    if (!select.empty() && g_focus >= 0) { EnsureVisible(g_focus); Relayout(); }
}
// 압축 파일 풀기는 '우클릭 메뉴로 풀기'와 같은 방법(차곡을 하나 더 띄움)으로: 여러 개를 한꺼번에, 이 창은 계속 둘러보기
static void FsExtract(const std::vector<std::wstring>& arcs, int mode) {
    const wchar_t* verb = mode == XT_SMART ? L"/x-smart \"" : mode == XT_HERE ? L"/x-here \"" : mode == XT_FOLDER ? L"/x-folder \"" : L"/x-to \"";
    std::wstring exe = ExePath();
    for (auto& a : arcs) ShellExecuteW(g_hwnd, L"open", exe.c_str(), (verb + a + L"\"").c_str(), ParentOf(a).c_str(), SW_SHOWNORMAL);
}
static void FsOpenItem(int row) {
    const FsItem* it = FsAt(row);
    if (!it) return;
    if (it->dir) { FsNavigate(it->path); return; }
    if (it->arc) { std::wstring p = it->path; OpenArchive(p); return; }
    if ((INT_PTR)ShellExecuteW(g_hwnd, L"open", it->path.c_str(), nullptr, g_fsPath.c_str(), SW_SHOWNORMAL) <= 32)
        ShellExecuteW(g_hwnd, L"open", L"rundll32.exe", (L"shell32.dll,OpenAs_RunDLL " + it->path).c_str(), nullptr, SW_SHOWNORMAL);
}
static void GoUp() {
    if (!g_search.empty()) { ClearSearch(); ShowCur(); Relayout(); return; }  // 검색 중이면 검색만 끝냄
    if (g_fs) {
        if (g_fsPath.empty()) { ExitBrowse(); return; }
        std::wstring from = g_fsPath;
        FsNavigate(FsParent(from), from);
        return;
    }
    if (g_hasArc && g_cur == 0) { EnterBrowse(ParentOf(g_arc.path), FileNameOf(g_arc.path)); return; }  // 압축 파일이 있는 폴더로
    if (!g_hasArc || g_cur == 0) return;
    int from = g_cur;
    ShowDir(g_dirs[(size_t)g_cur].parent, from);
    Relayout();
}
// 바로 열기: 임시 폴더에 풀고 연결 프로그램으로 연다
static void CmdOpenFile() {
    if (g_fs) { FsOpenItem(SingleSel()); return; }
    int one = SingleSel();
    if (one < 0 || g_rows[(size_t)one].kind != 2 || !QueueAllowed()) return;
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"Chagok\\" + std::to_wstring(GetTickCount());
    MakeDirs(dir);
    JobState* j = new JobState();
    j->kind = JK_EXTRACT;
    j->idx = {g_rows[(size_t)one].idx};
    j->dest = dir;
    j->strip = CurStrip();
    j->openAfter = dir + L"\\" + SanitizeRel(g_rows[(size_t)one].name);
    StartJob(j);
}
static void Activate(int row) {
    if (row < 0 || row >= (int)g_rows.size()) return;
    if (g_fs) { FsOpenItem(row); return; }
    auto& r = g_rows[(size_t)row];
    if (r.kind == 1) { ClearSearch(); ShowDir(r.idx); Relayout(); }
    else if (r.kind == 2) {
        std::fill(g_sel.begin(), g_sel.end(), 0);
        g_sel[(size_t)row] = 1;
        CmdOpenFile();
    }
}

static void OnCommand(int id);
// ============================================================ 1.11 추가 기능
static void SelectRow(int i, bool ctrl, bool shift);
static void EnsureVisible(int i);
static void OpenInExplorer(const std::wstring& p);
static void ClearSearch() {
    if (g_search.empty()) return;
    g_search.clear();
    g_searchSilent = true;
    if (g_searchEdit) SetWindowTextW(g_searchEdit, L"");
    g_searchSilent = false;
}
static void NavigateTo(int node) { ClearSearch(); ShowDir(node); Relayout(); }

// 압축 파일을 바꾸는 작업은 다른 작업이 끝난 뒤에만
static bool NotBusyFor(const wchar_t* what) {
    if (!Busy()) return true;
    MsgBox(what, L"진행 중인 작업이 끝난 뒤에 할 수 있습니다.");
    return false;
}
static bool QueueOkFor(const wchar_t* what) {
    if (QueueAllowed()) return true;
    MsgBox(what, L"압축 파일을 바꾸는 작업이 진행 중입니다. 끝난 뒤에 다시 시도하세요.");
    return false;
}
// 선택한 행 → 압축 파일 항목 번호 (폴더는 안쪽 전체)
static std::vector<int> SelectedEntries(bool filesOnly = false) {
    std::vector<int> idx;
    for (size_t i = 0; i < g_rows.size(); i++) {
        if (!g_sel[i]) continue;
        if (g_rows[i].kind == 1) { if (!filesOnly) CollectEntries(g_rows[i].idx, idx); else { std::vector<int> t; CollectEntries(g_rows[i].idx, t); for (int k : t) if (!g_arc.entries[(size_t)k].isDir) idx.push_back(k); } }
        else if (g_rows[i].kind == 2) idx.push_back(g_rows[i].idx);
    }
    return idx;
}
static std::wstring ParentPath(const std::wstring& n) { size_t s = n.find_last_of(L'/'); return s == std::wstring::npos ? L"" : n.substr(0, s); }

// ---- 이름 바꾸기 / 삭제 (다시 압축하지 않고 데이터 복사)
static void CmdRename() {
    if (!g_hasArc) return;
    if (g_arc.readOnly) { MsgBox(L"이름 바꾸기", g_arc.readOnlyWhy); return; }
    int one = SingleSel();
    if (one < 0) { MsgBox(L"이름 바꾸기", L"이름을 바꿀 항목을 하나만 고르세요."); return; }
    if (!NotBusyFor(L"이름 바꾸기")) return;
    const Row& r = g_rows[(size_t)one];
    std::wstring oldPath = r.kind == 2 ? g_arc.entries[(size_t)r.idx].name : g_dirs[(size_t)r.idx].path;
    std::wstring oldName = FileNameOf(oldPath.empty() ? oldPath : oldPath), parent = ParentPath(oldPath);
    size_t sl = oldPath.find_last_of(L'/');
    oldName = sl == std::wstring::npos ? oldPath : oldPath.substr(sl + 1);
    DlgSpec d;
    d.title = L"이름 바꾸기";
    d.msg = L"새 이름을 입력하세요.";
    d.buttons = {L"바꾸기", L"취소"};
    d.def = 0; d.cancel = 1;
    d.input = true; d.text = true;
    d.inputText = oldName;
    size_t dot = oldName.find_last_of(L'.');
    d.selEnd = r.kind == 2 && dot != std::wstring::npos && dot > 0 ? (int)dot : -1;
    if (RunDialog(g_hwnd, d) != 0) return;
    std::wstring nn = d.inputText;
    while (!nn.empty() && (nn.back() == L' ' || nn.back() == L'.')) nn.pop_back();
    while (!nn.empty() && nn[0] == L' ') nn.erase(0, 1);
    if (nn.empty() || nn == oldName) return;
    if (nn.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) { MsgBox(L"이름 바꾸기", L"이름에 \\ / : * ? \" < > | 는 쓸 수 없습니다."); return; }
    std::wstring newPath = parent.empty() ? nn : parent + L"/" + nn;
    for (size_t i = 0; i < g_arc.entries.size(); i++) {
        const std::wstring& n = g_arc.entries[i].name;
        bool mine = n == oldPath || n.compare(0, oldPath.size() + 1, oldPath + L"/") == 0;
        if (!mine && (!lstrcmpiW(n.c_str(), newPath.c_str()) || (n.size() > newPath.size() && n[newPath.size()] == L'/' && !_wcsnicmp(n.c_str(), newPath.c_str(), newPath.size())))) {
            MsgBox(L"이름 바꾸기", L"같은 이름의 항목이 이미 있습니다: " + nn);
            return;
        }
    }
    JobState* j = new JobState();
    j->kind = JK_MODIFY;
    j->modifyWhat = L"이름 바꾸기";
    for (size_t i = 0; i < g_arc.entries.size(); i++) {
        const std::wstring& n = g_arc.entries[i].name;
        if (n == oldPath) j->rename[(int)i] = newPath;
        else if (r.kind == 1 && n.compare(0, oldPath.size() + 1, oldPath + L"/") == 0) j->rename[(int)i] = newPath + n.substr(oldPath.size());
    }
    j->arcPath = g_arc.path;
    j->target = g_dirs[(size_t)g_cur].path;
    StartJob(j);
}
static void CmdDelete() {
    if (!g_hasArc) return;
    if (g_arc.readOnly) { MsgBox(L"삭제", g_arc.readOnlyWhy); return; }
    std::vector<int> idx = SelectedEntries();
    if (idx.empty()) return;
    if (!NotBusyFor(L"삭제")) return;
    int n = SelCount();
    DlgSpec d;
    d.title = L"압축 파일에서 삭제";
    d.msg = (n == 1 ? L"'" + g_rows[(size_t)SingleSel()].name + L"'" : std::to_wstring(n) + L"개 항목") +
            L"을(를) 압축 파일에서 지울까요?\n지운 항목은 되돌릴 수 없습니다.";
    d.buttons = {L"삭제", L"취소"};
    d.def = 1; d.cancel = 1;
    if (RunDialog(g_hwnd, d) != 0) return;
    JobState* j = new JobState();
    j->kind = JK_MODIFY;
    j->modifyWhat = L"삭제";
    j->del = idx;
    j->arcPath = g_arc.path;
    j->target = g_dirs[(size_t)g_cur].path;
    StartJob(j);
}

// ---- 압축 파일 검사
static void CmdTest() {
    if (!g_hasArc || !QueueOkFor(L"압축 파일 검사")) return;
    JobState* j = new JobState();
    j->kind = JK_TEST;
    for (size_t i = 0; i < g_arc.entries.size(); i++) if (!g_arc.entries[i].isDir) j->idx.push_back((int)i);
    j->arcPath = g_arc.path;
    StartJob(j);
}

// ---- SHA-256 (압축 파일 자체 또는 안의 파일)
static void CmdHash(bool archiveItself) {
    if (!g_hasArc || !QueueOkFor(L"SHA-256")) return;
    JobState* j = new JobState();
    j->kind = JK_HASH;
    j->arcPath = g_arc.path;
    if (archiveItself) j->hashFiles = g_arc.volumes;
    else {
        j->idx = SelectedEntries(true);
        if (j->idx.empty()) { delete j; return; }
        if (j->idx.size() > 50) j->idx.resize(50);
    }
    StartJob(j);
}
static std::wstring ClipText() {
    std::wstring t;
    if (!OpenClipboard(g_hwnd)) return t;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) { t = p; GlobalUnlock(h); }
    }
    CloseClipboard();
    return t;
}
static void SetClipText(const std::wstring& t) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
    if (h) { memcpy(GlobalLock(h), t.c_str(), (t.size() + 1) * sizeof(wchar_t)); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
    CloseClipboard();
}
static void ShowHashResult(JobState* j) {
    // 클립보드에 64자리 16진수가 있으면 비교해 준다
    std::wstring clip;
    for (wchar_t c : ClipText()) if (iswxdigit(c)) clip += (wchar_t)towlower(c); else if (!iswspace(c)) { clip.clear(); break; }
    bool haveClip = clip.size() == 64;
    std::wstring msg, copy;
    for (auto& h : j->hashes) {
        msg += h.first + L"\n" + h.second;
        if (haveClip) msg += h.second == clip ? L"\n✔ 클립보드의 값과 같습니다" : L"\n✘ 클립보드의 값과 다릅니다";
        msg += L"\n\n";
        copy += (j->hashes.size() > 1 ? h.first + L"  " : L"") + h.second + L"\r\n";
    }
    if (!haveClip) msg += L"비교할 값을 복사(Ctrl+C)해 두고 다시 계산하면 같은지 바로 알려 줍니다.";
    while (!copy.empty() && (copy.back() == L'\n' || copy.back() == L'\r')) copy.pop_back();
    DlgSpec d;
    d.title = L"SHA-256";
    d.msg = msg;
    d.buttons = {L"복사", L"닫기"};
    d.def = 1; d.cancel = 1;
    if (RunDialog(g_hwnd, d) == 0) SetClipText(copy);
}

// ---- 이름 글자 코드 (ZIP 이름이 깨져 보일 때)
static void CmdNameCodepage(RECT anchor) {
    if (!g_hasArc || !g_arc.isZip) { MsgBox(L"이름 글자 코드", L"ZIP 형식에서만 바꿀 수 있습니다."); return; }
    if (!NotBusyFor(L"이름 글자 코드")) return;
    struct { int cp; const wchar_t* t; } cps[] = {{0, L"자동"}, {949, L"한국어 (CP949)"}, {65001, L"유니코드 (UTF-8)"},
                                                 {932, L"일본어 (Shift-JIS)"}, {936, L"중국어 간체 (GBK)"}, {950, L"중국어 번체 (Big5)"}, {437, L"영어 (DOS)"}};
    std::vector<MItem> v = {{0, L"이름 글자 코드", 2, false, true}};
    for (auto& c : cps) v.push_back({800 + (c.cp == 0 ? 0 : (int)(&c - cps)), c.t, 0, g_zipNameCP == c.cp, true});
    int id = RunPopup(g_hwnd, anchor, v);
    if (id < 800) return;
    int k = id - 800;
    if (k < 0 || k >= (int)(sizeof(cps) / sizeof(cps[0]))) return;
    g_zipNameCP = cps[k].cp;
    std::wstring dir = g_dirs[(size_t)g_cur].path;
    OpenArchive(g_arc.path);
    (void)dir;
}

// ---- 미리보기 (사진 · 글)
struct PreviewState { std::wstring name, file; Bitmap* img = nullptr; HWND edit = nullptr, h = nullptr; HBRUSH brush = nullptr; bool done = false; int result = 0; RECT bOpen{}, bClose{}, bX{}; int hover = 0; std::wstring note; };
static PreviewState* g_pv;
static LRESULT CALLBACK PreviewProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    PreviewState* s = g_pv;
    if (!s) return DefWindowProcW(h, m, w, lp);
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        HDC md = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(md, bm);
        {
            Graphics g(md);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            FillR(g, cr, 0, P.panel);
            StrokeR(g, cr, 0, P.border);
            Txt(md, {S(20), 0, cr.right - S(60), S(52)}, s->name, P.text, g_fontTitle);
            s->bX = {cr.right - S(46), S(10), cr.right - S(14), S(42)};
            if (s->hover == 3) FillR(g, s->bX, SF(6), P.hover);
            DrawIco(g, IC_X, (REAL)s->bX.left + S(8), (REAL)s->bX.top + S(8), (REAL)S(16), P.dim);
            HLine(g, 1, cr.right - 1, S(52), P.border);
            RECT area{S(16), S(62), cr.right - S(16), cr.bottom - S(64)};
            if (s->img) {
                FillR(g, area, SF(8), Mix(P.panel, P.text, 0.05f));
                double iw = s->img->GetWidth(), ih = s->img->GetHeight();
                double k = min(1.0, min((area.right - area.left - S(16)) / iw, (area.bottom - area.top - S(16)) / ih));
                int dw = (int)(iw * k), dh = (int)(ih * k);
                g.DrawImage(s->img, (INT)((area.left + area.right - dw) / 2), (INT)((area.top + area.bottom - dh) / 2), dw, dh);
            } else if (!s->edit) {
                Txt(md, area, s->note, P.dim, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            Txt(md, {S(20), cr.bottom - S(56), cr.right / 2, cr.bottom - S(12)}, s->note.empty() || !s->img ? L"" : s->note, P.dim, g_fontSmall);
            int bh = S(34), y = cr.bottom - S(50);
            s->bClose = {cr.right - S(20) - S(88), y, cr.right - S(20), y + bh};
            s->bOpen = {s->bClose.left - S(8) - S(110), y, s->bClose.left - S(8), y + bh};
            for (int k = 1; k <= 2; k++) {
                RECT r = k == 1 ? s->bOpen : s->bClose;
                FillR(g, r, SF(8), s->hover == k ? P.hover : P.btn);
                StrokeR(g, r, SF(8), P.btnBorder == NONE ? P.border : P.btnBorder);
                Txt(md, r, k == 1 ? L"바로 열기" : L"닫기", P.text, g_font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        BitBlt(dc, 0, 0, cr.right, cr.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pt);
        if (pt.y < S(52) && !PtInRect(&s->bX, pt)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hv = PtInRect(&s->bOpen, pt) ? 1 : PtInRect(&s->bClose, pt) ? 2 : PtInRect(&s->bX, pt) ? 3 : 0;
        if (hv != s->hover) { s->hover = hv; InvalidateRect(h, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: s->hover = 0; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONUP:
        if (s->hover == 1) { s->result = 1; s->done = true; }
        else if (s->hover) s->done = true;
        return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, P.text);
        SetBkColor(dc, P.bg);
        if (!s->brush) s->brush = CreateSolidBrush(P.bg);
        return (LRESULT)s->brush;
    }
    case WM_CLOSE: s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, lp);
}
static std::wstring DecodeText(const std::vector<uint8_t>& b) {
    if (b.size() >= 2 && b[0] == 0xFF && b[1] == 0xFE) return std::wstring((const wchar_t*)(b.data() + 2), (b.size() - 2) / 2);
    if (b.size() >= 2 && b[0] == 0xFE && b[1] == 0xFF) {
        std::wstring w;
        for (size_t i = 2; i + 1 < b.size(); i += 2) w += (wchar_t)(b[i] << 8 | b[i + 1]);
        return w;
    }
    size_t st = b.size() >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF ? 3 : 0;
    const char* p = (const char*)b.data() + st;
    int n = (int)(b.size() - st);
    int k = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, n, nullptr, 0);
    UINT cp = CP_UTF8;
    if (k <= 0) {  // 끝에서 잘린 UTF-8 글자 하나는 봐준다
        int n2 = n;
        while (n2 > 0 && n - n2 < 4 && ((uint8_t)p[n2 - 1] & 0xC0) == 0x80) n2--;
        if (n2 > 0 && ((uint8_t)p[n2 - 1] & 0xC0) == 0xC0) n2--;
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, n2, nullptr, 0) > 0) n = n2; else cp = 949;
    }
    k = MultiByteToWideChar(cp, 0, p, n, nullptr, 0);
    std::wstring w((size_t)max(0, k), 0);
    if (k > 0) MultiByteToWideChar(cp, 0, p, n, &w[0], k);
    return w;
}
static void RunPreview(const std::wstring& name, const std::wstring& file, bool& wantOpen) {
    PreviewState st;
    st.name = name; st.file = file;
    g_pv = &st;
    std::wstring low = fmt::Lower(file);
    auto ends = [&](const wchar_t* e) { size_t k = wcslen(e); return low.size() >= k && !low.compare(low.size() - k, k, e); };
    bool isImg = ends(L".png") || ends(L".jpg") || ends(L".jpeg") || ends(L".gif") || ends(L".bmp") || ends(L".tif") || ends(L".tiff") || ends(L".ico");
    std::wstring text;
    if (isImg) {
        st.img = Bitmap::FromFile(LongPath(file).c_str());
        if (st.img && st.img->GetLastStatus() != Ok) { delete st.img; st.img = nullptr; }
        if (st.img) st.note = std::to_wstring(st.img->GetWidth()) + L" × " + std::to_wstring(st.img->GetHeight());
        else st.note = L"사진을 읽을 수 없습니다.";
    } else {
        HANDLE f = CreateFileW(LongPath(file).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        std::vector<uint8_t> b(512 * 1024);
        DWORD got = 0;
        if (f != INVALID_HANDLE_VALUE) { ReadFile(f, b.data(), (DWORD)b.size(), &got, nullptr); CloseHandle(f); }
        b.resize(got);
        size_t zeros = 0, look = min<size_t>(b.size(), 8192);
        bool utf16 = b.size() >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF));
        for (size_t i = 0; i < look; i++) if (!b[i]) zeros++;
        if (!utf16 && zeros > look / 100) st.note = L"이 형식은 미리 볼 수 없습니다. '바로 열기'로 여세요.";
        else {
            std::wstring raw = DecodeText(b);
            for (size_t i = 0; i < raw.size(); i++) {  // 줄바꿈을 \r\n 으로
                if (raw[i] == L'\n' && (i == 0 || raw[i - 1] != L'\r')) text += L'\r';
                text += raw[i];
            }
            if (got == 512 * 1024) text += L"\r\n\r\n… (앞부분 512KB 만 표시)";
        }
    }
    RECT orc; GetWindowRect(g_hwnd, &orc);
    int W = min(S(820), (int)(orc.right - orc.left) - S(40)), H = min(S(620), (int)(orc.bottom - orc.top) - S(40));
    W = max(W, S(480)); H = max(H, S(360));
    st.h = CreateWindowExW(0, L"ChagokPreview", (name + L" - 미리보기").c_str(), WS_POPUP | WS_SYSMENU,
                           (orc.left + orc.right - W) / 2, (orc.top + orc.bottom - H) / 2, W, H, g_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    DWORD corner = 2;
    DwmSetWindowAttribute(st.h, 33, &corner, sizeof(corner));
    ApplyOpacity(st.h);
    if (!st.img && st.note.empty()) {
        st.edit = CreateWindowExW(0, L"EDIT", text.c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                  S(16), S(62), W - S(32), H - S(62) - S(64), st.h, nullptr, nullptr, nullptr);
        SendMessageW(st.edit, WM_SETFONT, (WPARAM)g_font, TRUE);
        // 다크 테마: 글 상자의 스크롤 막대도 어둡게 (Windows 10 1809 이상, uxtheme 을 동적으로 불러 옴)
        if (g_dark)
            if (HMODULE ux = LoadLibraryW(L"uxtheme.dll"))
                if (auto swt = (HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR))GetProcAddress(ux, "SetWindowTheme")) swt(st.edit, L"DarkMode_Explorer", nullptr);
    }
    EnableWindow(g_hwnd, FALSE);
    ShowWindow(st.h, SW_SHOW);
    SetForegroundWindow(st.h);
    if (st.edit) SetFocus(st.edit); else SetFocus(st.h);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if ((msg.message == WM_KEYDOWN && (msg.wParam == VK_ESCAPE || (msg.wParam == VK_SPACE && msg.hwnd != st.edit))) ||
            (msg.message == WM_CHAR && msg.wParam == 27)) { st.done = true; continue; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { st.result = 1; st.done = true; continue; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) PostQuitMessage((int)msg.wParam);
    EnableWindow(g_hwnd, TRUE);
    SetActiveWindow(g_hwnd);
    DestroyWindow(st.h);
    if (st.brush) DeleteObject(st.brush);
    delete st.img;
    g_pv = nullptr;
    wantOpen = st.result == 1;
}
static void CmdOpenFile();
static void DeleteTree(const std::wstring& dir) {
    SHFILEOPSTRUCTW op{};
    std::wstring from = dir + L'\0';
    op.wFunc = FO_DELETE; op.pFrom = from.c_str(); op.fFlags = FOF_NO_UI;
    SHFileOperationW(&op);
}
// 압축 파일 항목 몇 개를 지금 바로(작업 창 없이) 임시 폴더에 푼다
static bool ExtractNow(const std::vector<int>& idx, const std::wstring& strip, const std::wstring& dir, std::wstring& err) {
    Job job;
    job.askPassword = [](std::wstring& pw) { return AskPassword(pw, false); };
    job.askOverwrite = [](const std::wstring&) { return 1; };
    int failed = 0;
    std::wstring fe;
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    bool ok = AnyExtract(g_arc, idx, strip, dir, job, err, failed, fe);
    SetCursor(old);
    if (ok && failed) { err = fe; ok = false; }
    return ok;
}
static void CmdPreview() {
    int one = SingleSel();
    if (g_fs) {
        const FsItem* it = FsAt(one);
        if (!it || it->dir) return;
        if (it->size > 64ull * 1024 * 1024) { MsgBox(L"미리보기", L"64MB 보다 큰 파일은 미리 볼 수 없습니다."); return; }
        std::wstring path = it->path;
        bool open = false;
        RunPreview(it->name, path, open);
        if (open) FsOpenItem(one);
        return;
    }
    if (!g_hasArc || one < 0 || g_rows[(size_t)one].kind != 2) return;
    if (Busy() && !(g_arc.isZip && QueueAllowed())) { MsgBox(L"미리보기", L"진행 중인 작업이 끝난 뒤에 볼 수 있습니다."); return; }
    const ZipEntry& e = g_arc.entries[(size_t)g_rows[(size_t)one].idx];
    if (e.usize > 64ull * 1024 * 1024) { MsgBox(L"미리보기", L"64MB 보다 큰 파일은 미리 볼 수 없습니다. '바로 열기'를 쓰세요."); return; }
    std::wstring dir = TempWorkDir(L"preview"), err;
    std::wstring strip = ParentPath(e.name);
    if (!strip.empty()) strip += L"/";
    if (!ExtractNow({g_rows[(size_t)one].idx}, strip, dir, err)) { MsgBox(L"미리보기", err); DeleteTree(dir); return; }
    std::wstring file = dir + L"\\" + SanitizeRel(e.name.substr(strip.size()));
    bool open = false;
    RunPreview(FileNameOf(file), file, open);
    DeleteTree(dir);
    if (open) CmdOpenFile();
}

// ---- 끌어서 꺼내기: 선택 항목을 임시 폴더에 풀고 탐색기로 끌어다 놓게 한다
static bool g_dragging = false;
static void DragPaths(const std::wstring& dir, const std::vector<std::wstring>& tops);
static void DoDragOut() {
    if (g_fs) { std::vector<std::wstring> v = FsSelPaths(); if (!v.empty()) DragPaths(g_fsPath, v); return; }
    if (!g_hasArc || !HasRealSel()) return;
    if (Busy() && !(g_arc.isZip && QueueAllowed())) return;
    std::wstring dir = TempWorkDir(L"drag"), err;
    std::vector<std::wstring> tops;
    for (size_t i = 0; i < g_rows.size(); i++) {
        if (!g_sel[i]) continue;
        const Row& r = g_rows[i];
        std::wstring full = r.kind == 2 ? g_arc.entries[(size_t)r.idx].name : g_dirs[(size_t)r.idx].path;
        std::wstring strip = ParentPath(full);
        if (!strip.empty()) strip += L"/";
        std::vector<int> idx;
        if (r.kind == 2) idx.push_back(r.idx); else CollectEntries(r.idx, idx);
        if (!ExtractNow(idx, strip, dir, err)) { MsgBox(L"끌어서 꺼내기", err); DeleteTree(dir); return; }
        std::wstring p = dir + L"\\" + SanitizeRel(full.substr(strip.size()));
        if (r.kind == 1) MakeDirs(p);  // 빈 폴더도
        tops.push_back(p);
    }
    DragPaths(dir, tops);
    // 임시 파일은 탐색기가 복사를 마칠 때까지 남겨 두고, 다음 실행 때 정리한다
}
// 디스크의 파일들을 끌기 시작 (복사만 허용)
static void DragPaths(const std::wstring& dir, const std::vector<std::wstring>& tops) {
    PIDLIST_ABSOLUTE parent = nullptr;
    if (FAILED(SHParseDisplayName(dir.c_str(), nullptr, &parent, 0, nullptr))) return;
    std::vector<PIDLIST_ABSOLUTE> full;
    std::vector<PCUITEMID_CHILD> kids;
    for (auto& t : tops) {
        PIDLIST_ABSOLUTE p = nullptr;
        if (SUCCEEDED(SHParseDisplayName(t.c_str(), nullptr, &p, 0, nullptr))) { full.push_back(p); kids.push_back(ILFindLastID(p)); }
    }
    IDataObject* dobj = nullptr;
    if (!kids.empty() && SUCCEEDED(SHCreateDataObject(parent, (UINT)kids.size(), kids.data(), nullptr, IID_PPV_ARGS(&dobj)))) {
        DWORD eff = 0;
        g_dragging = true;
        SHDoDragDrop(g_hwnd, dobj, nullptr, DROPEFFECT_COPY, &eff);
        g_dragging = false;
        dobj->Release();
    }
    for (auto p : full) CoTaskMemFree(p);
    CoTaskMemFree(parent);
}
// 하루 지난 임시 폴더 정리 (%TEMP%\Chagok)
static void CleanTemp() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring base = std::wstring(tmp) + L"Chagok";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((base + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    FILETIME now; GetSystemTimeAsFileTime(&now);
    uint64_t n = ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        uint64_t c = ((uint64_t)fd.ftCreationTime.dwHighDateTime << 32) | fd.ftCreationTime.dwLowDateTime;
        if (n > c && n - c > 24ull * 3600 * 10000000) DeleteTree(base + L"\\" + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// ---- 오른쪽 클릭 메뉴
static int SortMenu(RECT a) {
    std::vector<MItem> sv = {{0, L"정렬", 2, false, true}};
    const wchar_t* cols[] = {L"이름", L"크기", g_fs ? L"종류" : L"압축 후 크기", L"수정한 날짜"};
    for (int i = 0; i < 4; i++) sv.push_back({720 + i, cols[i], 0, g_sortCol == i, true});
    sv.push_back({0, L"", 1, false, true});
    sv.push_back({724, L"내림차순", 0, g_sortDesc, true});
    return RunPopup(g_hwnd, a, sv);
}
static void ApplySort(int id) {
    if (id == 724) g_sortDesc = !g_sortDesc; else g_sortCol = id - 720;
    RegSet(L"SortCol", (DWORD)g_sortCol); RegSet(L"SortDesc", g_sortDesc ? 1 : 0);
    SortRows();
    std::fill(g_sel.begin(), g_sel.end(), 0);
    g_focus = g_anchor = -1;
    Relayout();
}
static void CmdFsContextMenu(POINT screen) {
    int one = SingleSel(), n = SelCount();
    const FsItem* it = FsAt(one);
    bool root = g_fsPath.empty();
    std::vector<std::wstring> arcs = FsSelPaths(true, true), files = FsSelPaths(true);
    std::vector<MItem> v;
    if (it && it->dir) v.push_back({740, L"열기\tEnter", 0, false, true});
    else if (it && it->arc) v.push_back({740, L"차곡으로 열기\tEnter", 0, false, true});
    else if (it) { v.push_back({740, L"열기\tEnter", 0, false, true}); v.push_back({741, L"미리보기\tSpace", 0, false, true}); }
    if (!arcs.empty()) {
        if (!v.empty()) v.push_back({0, L"", 1, false, true});
        v.push_back({751, L"알아서 풀기", 0, false, true});
        v.push_back({742, L"현재 폴더에 압축 풀기", 0, false, true});
        v.push_back({743, L"파일 이름 폴더에 풀기", 0, false, true});
        v.push_back({744, L"풀 위치 선택…", 0, false, true});
    }
    if (n && !root) {
        if (!v.empty()) v.push_back({0, L"", 1, false, true});
        v.push_back({745, L"압축하기…", 0, false, true});
        v.push_back({746, L"분할 압축 (" + SplitLabel(g_splitMB) + L")…", 0, false, true});
        if (!files.empty()) v.push_back({747, L"SHA-256 확인", 0, false, true});
    }
    if (n) {
        v.push_back({0, L"", 1, false, true});
        v.push_back({748, L"Windows 탐색기에서 보기", 0, false, true});
        v.push_back({749, L"경로 복사", 0, false, true});
    }
    if (!v.empty()) v.push_back({0, L"", 1, false, true});
    v.push_back({750, L"새로 고침\tF5", 0, false, true});
    v.push_back({725, L"정렬…", 0, false, true});
    RECT a{screen.x, screen.y, screen.x, screen.y};
    int id = RunPopup(g_hwnd, a, v);
    if (id == 725) id = SortMenu(a);
    switch (id) {
    case 740: FsOpenItem(one); break;
    case 741: CmdPreview(); break;
    case 751: FsExtract(arcs, XT_SMART); break;
    case 742: FsExtract(arcs, XT_HERE); break;
    case 743: FsExtract(arcs, XT_FOLDER); break;
    case 744: FsExtract(arcs, XT_PICK); break;
    case 745: StartCompress(FsSelPaths(), false); break;
    case 746: StartCompress(FsSelPaths(), true); break;
    case 747: {
        JobState* j = new JobState();
        j->kind = JK_HASH;
        j->hashFiles = files;
        if (j->hashFiles.size() > 50) j->hashFiles.resize(50);
        StartJob(j);
        break;
    }
    case 748: { std::vector<std::wstring> s = FsSelPaths(); if (s.empty() && it) s.push_back(it->path); if (!s.empty()) OpenInExplorer(s[0]); break; }
    case 749: {
        std::wstring t;
        for (size_t i = 0; i < g_rows.size(); i++) if (g_sel[i]) t += (t.empty() ? L"" : L"\r\n") + g_fsItems[(size_t)g_rows[i].idx].path;
        SetClipText(t);
        break;
    }
    case 750: FsRefresh(); break;
    case 720: case 721: case 722: case 723: case 724: ApplySort(id); break;
    }
}
static void CmdContextMenu(POINT screen) {
    if (g_fs) { CmdFsContextMenu(screen); return; }
    if (!g_hasArc) return;
    int one = SingleSel(), n = SelCount();
    bool file = one >= 0 && g_rows[(size_t)one].kind == 2, dir = one >= 0 && g_rows[(size_t)one].kind == 1;
    bool ro = g_arc.readOnly;
    std::vector<MItem> v;
    if (file) {
        v.push_back({701, L"미리보기\tSpace", 0, false, true});
        v.push_back({702, L"바로 열기\tEnter", 0, false, true});
    }
    if (dir) v.push_back({707, L"열기\tEnter", 0, false, true});
    if (n) {
        v.push_back({703, L"선택 항목 추출…", 0, false, true});
        v.push_back({0, L"", 1, false, true});
        v.push_back({704, L"이름 바꾸기\tF2", 0, false, one >= 0 && !ro});
        v.push_back({705, L"삭제\tDel", 0, false, !ro});
        bool anyFile = false;
        for (size_t i = 0; i < g_rows.size(); i++) if (g_sel[i] && g_rows[i].kind == 2) anyFile = true;
        if (anyFile) { v.push_back({0, L"", 1, false, true}); v.push_back({706, L"SHA-256 확인", 0, false, true}); }
        v.push_back({0, L"", 1, false, true});
    }
    v.push_back({710, L"압축 파일 검사", 0, false, true});
    v.push_back({711, L"압축 파일 SHA-256", 0, false, true});
    if (g_arc.isZip) v.push_back({712, L"이름 글자 코드…", 0, false, true});
    v.push_back({0, L"", 1, false, true});
    v.push_back({725, L"정렬…", 0, false, true});
    RECT a{screen.x, screen.y, screen.x, screen.y};
    int id = RunPopup(g_hwnd, a, v);
    if (id == 725) id = SortMenu(a);
    switch (id) {
    case 701: CmdPreview(); break;
    case 702: CmdOpenFile(); break;
    case 707: if (dir) NavigateTo(g_rows[(size_t)one].idx); break;
    case 703: OnCommand(ID_EXTRACT_SEL); break;
    case 704: CmdRename(); break;
    case 705: CmdDelete(); break;
    case 706: CmdHash(false); break;
    case 710: CmdTest(); break;
    case 711: CmdHash(true); break;
    case 712: CmdNameCodepage(a); break;
    case 720: case 721: case 722: case 723: case 724: ApplySort(id); break;
    }
}

static void OnCommand(int id) {
    if (id >= ID_CRUMB) { NavigateTo(id - ID_CRUMB); return; }
    if (id >= ID_FSCRUMB && id < ID_FSCRUMB + 100) {
        size_t k = (size_t)(id - ID_FSCRUMB);
        if (k < g_fsCrumbs.size()) { std::wstring from = k + 1 < g_fsCrumbs.size() ? g_fsCrumbs[k + 1] : L""; FsNavigate(g_fsCrumbs[k], from); }
        return;
    }
    if (id >= ID_RECENT && id < ID_RECENT + 10) {
        auto rec = RecentList();
        std::vector<std::wstring> ex;
        for (auto& p : rec) if (FileExists(p)) ex.push_back(p);
        if (id - ID_RECENT < (int)ex.size() && !Busy()) OpenArchive(ex[(size_t)(id - ID_RECENT)]);
        return;
    }
    switch (id) {
    case ID_SEARCHCLR: ClearSearch(); ShowCur(); Relayout(); break;
    case ID_BROWSE:
        if (g_fs) FsNavigate(L"");
        else if (g_hasArc) EnterBrowse(ParentOf(g_arc.path), FileNameOf(g_arc.path));
        else EnterBrowse(RegReadStr(REG_KEY, L"BrowsePath"));
        break;
    case ID_COMPRESS: CmdCompress(false); break;
    case ID_SPLIT: CmdCompress(true); break;
    case ID_EXTRACT_ALL: CmdExtractAll(); break;
    case ID_ADD: CmdAdd(); break;
    case ID_EXTRACT_SEL: CmdExtractSel(); break;
    case ID_THEME: CmdTheme(); break;
    case ID_SETTINGS: RunSettings(false, nullptr, 0); Relayout(); break;
    case ID_BACK: GoUp(); break;
    case ID_CANCEL: if (g_job) g_job->job.cancel = true; break;
    case ID_SHOWPROG: ProgShow(); break;
    case ID_OPENFILE: CmdOpenFile(); break;
    case ID_VIEW_TILES: case ID_VIEW_LIST:
        g_view = id == ID_VIEW_TILES ? 0 : 1;
        RegSet(L"View", (DWORD)g_view);
        g_scrollY = 0;
        Relayout();
        break;
    case ID_OPEN: {
        if (Busy()) { MsgBox(L"압축 파일 열기", L"진행 중인 작업이 끝난 뒤에 다른 압축 파일을 열 수 있습니다."); break; }
        std::vector<std::wstring> a;
        if (PickPaths(a, false, false, L"압축 파일 열기", true)) OpenArchive(a[0]);
        break;
    }
    }
}

static void ShowMainAfterMenu() {
    if (!g_mainHidden) return;
    g_mainHidden = false;
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
}
// ---- 완료 알림 (Windows 알림. 누르면 결과 폴더/파일을 연다)
#define WM_APP_TRAY (WM_APP + 9)
static std::wstring g_notifyOpen;
static bool g_trayOn = false;
static void TrayRemove() {
    if (!g_trayOn) return;
    NOTIFYICONDATAW n{};
    n.cbSize = sizeof(n); n.hWnd = g_hwnd; n.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &n);
    g_trayOn = false;
}
static void Notify(const std::wstring& title, const std::wstring& text, const std::wstring& open) {
    NOTIFYICONDATAW n{};
    n.cbSize = sizeof(n); n.hWnd = g_hwnd; n.uID = 1;
    n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    n.uCallbackMessage = WM_APP_TRAY;
    n.hIcon = g_iconSmall;
    lstrcpynW(n.szTip, APP_NAME, 64);
    lstrcpynW(n.szInfoTitle, title.c_str(), 64);
    lstrcpynW(n.szInfo, text.c_str(), 256);
    n.dwInfoFlags = NIIF_INFO;
    if (!Shell_NotifyIconW(g_trayOn ? NIM_MODIFY : NIM_ADD, &n)) return;
    g_trayOn = true;
    n.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &n);
    g_notifyOpen = open;
}
static void OpenInExplorer(const std::wstring& p) {
    if (p.empty()) return;
    if (DirExists(p)) ShellExecuteW(g_hwnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else ShellExecuteW(g_hwnd, L"open", L"explorer.exe", (L"/select,\"" + p + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
}

static void ShowHashResult(JobState* j);
static void UpdAfterJob();
static void OnJobDone() {
    WaitForSingleObject(g_jobThread, INFINITE);
    CloseHandle(g_jobThread);
    g_jobThread = nullptr;
    HWND fg = GetForegroundWindow();
    bool away = g_progHidden || (fg != g_hwnd && fg != g_prog.h) || IsIconic(g_hwnd);  // 사용자가 다른 일을 하는 중
    ProgClose();
    JobState* j = g_job;
    g_job = nullptr;
    if (g_quitAfterJob) { delete j; DestroyWindow(g_hwnd); return; }
    bool cancelled = j->job.cancel;
    bool more = !g_queue.empty();
    bool quiet = (away || more) && !g_mainHidden;  // 대화상자 대신 알림으로
    // 다음 작업부터 시작해 두고 결과를 보여 준다
    if (more) { JobState* n = g_queue.front(); g_queue.erase(g_queue.begin()); StartJobNow(n); }
    Relayout();
    if (j->kind == JK_COMPRESS) {
        if (j->ok && j->sfx) {
            if (quiet) Notify(L"자동 실행 파일을 만들었습니다", FileNameOf(j->target), j->target);
            else {
                DlgSpec d;
                d.title = L"자동 실행 파일을 만들었습니다";
                d.msg = j->target + L"\n\n이 파일을 실행하면 차곡이 없는 PC 에서도 압축을 풀 수 있습니다.";
                d.buttons = {L"폴더 열기", L"닫기"};
                d.def = 0; d.cancel = 1;
                if (RunDialog(g_hwnd, d) == 0) OpenInExplorer(j->target);
            }
            if (g_menuCompress && g_mainHidden) { delete j; DestroyWindow(g_hwnd); return; }
        } else if (j->ok) {
            std::wstring made = FileExists(j->target) || !FileExists(j->target + L".001") ? j->target : j->target + L".001";
            if (g_fs && !g_mainHidden) {
                if (away || more) Notify(L"압축 완료", FileNameOf(made), made);
                if (!lstrcmpiW(FsNorm(ParentOf(made)).c_str(), g_fsPath.c_str())) FsRefresh(FileNameOf(made));
            } else if (more || Busy()) Notify(L"압축 완료", FileNameOf(made), made);  // 대기 중인 작업이 지금 압축 파일을 쓰므로 열지 않음
            else {
                if (away) Notify(L"압축 완료", FileNameOf(made), made);
                ShowMainAfterMenu();
                g_reopenPw = &j->reopenPw;
                OpenArchive(made);
                g_reopenPw = nullptr;
            }
        } else {
            if (!cancelled) MsgBox(L"압축 실패", j->err);
            if (!g_hasArc && g_menuCompress) { delete j; DestroyWindow(g_hwnd); return; }
        }
    } else if (j->kind == JK_ADD || j->kind == JK_MODIFY) {
        std::wstring dir = j->target;
        OpenArchive(j->arcPath, dir);
        if (!j->ok && !cancelled) MsgBox(j->kind == JK_ADD ? L"파일 추가 실패" : j->modifyWhat + L" 실패", j->err);
        else if (j->ok && away) Notify(j->kind == JK_ADD ? L"파일 추가 완료" : j->modifyWhat + L" 완료", FileNameOf(j->arcPath), j->arcPath);
    } else if (j->kind == JK_TEST) {
        if (!j->ok && !cancelled) MsgBox(L"압축 파일 검사", j->err);
        else if (j->ok) {
            std::wstring msg = j->failed ? std::to_wstring(j->failed) + L"개 항목에 문제가 있습니다.\n" + j->firstError
                                         : L"이상 없습니다. 파일 " + std::to_wstring(j->filesTotal) + L"개를 모두 확인했습니다.";
            if (quiet) Notify(j->failed ? L"압축 파일 검사: 문제 발견" : L"압축 파일 검사: 이상 없음", msg, L"");
            else MsgBox(j->failed ? L"압축 파일 검사: 문제 발견" : L"압축 파일 검사: 이상 없음", FileNameOf(j->arcPath) + L"\n\n" + msg);
        }
    } else if (j->kind == JK_HASH) {
        if (!j->ok && !cancelled) MsgBox(L"SHA-256", j->err);
        else if (j->ok) ShowHashResult(j);
        if (j->hashFiles.empty() && !j->dest.empty()) {  // 임시로 푼 파일 정리
            SHFILEOPSTRUCTW op{};
            std::wstring from = j->dest + L'\0';
            op.wFunc = FO_DELETE; op.pFrom = from.c_str(); op.fFlags = FOF_NO_UI;
            SHFileOperationW(&op);
        }
    } else if (j->kind == JK_EXTRACT) {
        if (!j->ok && !cancelled) MsgBox(L"압축 해제 실패", j->err);
        else if (j->ok && !j->openAfter.empty()) {
            if ((INT_PTR)ShellExecuteW(g_hwnd, L"open", j->openAfter.c_str(), nullptr, ParentOf(j->openAfter).c_str(), SW_SHOWNORMAL) <= 32)
                ShellExecuteW(g_hwnd, L"open", L"rundll32.exe", (L"shell32.dll,OpenAs_RunDLL " + j->openAfter).c_str(), nullptr, SW_SHOWNORMAL);
            delete j;
            return;
        } else if (j->ok && g_menuLaunch && g_quietDone && !j->failed) {
            // 여기에 풀기 / 새 폴더에 풀기: 탐색기에서 바로 보이므로 조용히 종료
        } else if (j->ok && quiet && !j->failed) {
            Notify(L"압축 해제 완료", j->dest, j->dest);
        } else if (j->ok) {
            DlgSpec d;
            d.title = j->failed ? L"일부 항목을 풀지 못했습니다" : L"압축 해제 완료";
            d.msg = j->failed ? std::to_wstring(j->failed) + L"개 항목을 풀지 못했습니다.\n" + j->firstError + L"\n\n위치: " + j->dest
                              : L"다음 위치에 압축을 풀었습니다.\n" + j->dest;
            d.buttons = {L"폴더 열기", L"닫기"};
            d.def = 0; d.cancel = 1;
            if (RunDialog(g_hwnd, d) == 0) ShellExecuteW(g_hwnd, L"open", j->dest.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (g_menuLaunch && !Busy()) { delete j; DestroyWindow(g_hwnd); return; }
    }
    if (!j->reopenPw.empty()) SecureZeroMemory(&j->reopenPw[0], j->reopenPw.size() * sizeof(wchar_t));
    delete j;
    UpdAfterJob();
}

// ============================================================ 자동 업데이트
// 확인 순서: ① GitHub 릴리즈(기본) → ② GitHub 에 연결되지 않으면 사내 위치(설정에서 넣은 공유 폴더·웹 주소)
// GitHub: 최신 릴리즈의 첨부 파일 chagok-update.ini 를 releases/latest/download/ 로 읽고,
//         설치 파일은 그 버전의 태그(v<버전>) 주소에서 받는다. 검증(SHA-256 + 사내 인증서 지문)은 사내 위치와 같다.
#define GH_REPO L"https://github.com/sinsul-jhs87/Chagok"
// 업데이트 위치(공유 폴더 \\서버\공유\차곡 또는 http(s)://주소/)에 두 파일을 둔다:
//   chagok-update.ini : version=1.12.2 / file=ChagokSetup-1.12.2.exe / sha256=<64자리> / notes=바뀐 점(\n 은 줄바꿈)
//   ChagokSetup-1.12.2.exe : 사내 인증서로 서명된 설치 프로그램
// 차곡을 켜면 뒤에서 확인하고, 새 버전이 있으면 묻는다 → 내려받아 SHA-256 과 서명(사내 인증서 지문)을 확인한 뒤
// 설치 프로그램을 조용히(/S /UPDATE) 실행하고 차곡은 닫힌다. 설치가 끝나면 설치 프로그램이 차곡을 다시 띄운다.
#ifndef CHAGOK_CERT_THUMB
#define CHAGOK_CERT_THUMB L""
#endif
struct UpdInfo { std::wstring version, file, sha256, notes, base, from; bool manual = false; std::wstring err; };
static UpdInfo* g_updPending = nullptr;   // 작업 중에 찾은 새 버전: 작업이 끝나면 묻는다
static bool g_updChecking = false;
static std::wstring AppVersion() {
    static std::wstring v;
    if (!v.empty()) return v;
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    DWORD h = 0, n = GetFileVersionInfoSizeW(exe, &h);
    std::vector<BYTE> b(n ? n : 1);
    VS_FIXEDFILEINFO* fi = nullptr; UINT len = 0;
    if (n && GetFileVersionInfoW(exe, 0, n, b.data()) && VerQueryValueW(b.data(), L"\\", (void**)&fi, &len) && fi)
        v = std::to_wstring(HIWORD(fi->dwFileVersionMS)) + L"." + std::to_wstring(LOWORD(fi->dwFileVersionMS)) + L"." + std::to_wstring(HIWORD(fi->dwFileVersionLS));
    else v = L"0.0.0";
    return v;
}
static int VerCmp(const std::wstring& a, const std::wstring& b) {  // 1.12.10 > 1.12.9
    const wchar_t *p = a.c_str(), *q = b.c_str();
    for (int i = 0; i < 4; i++) {
        long x = wcstol(p, (wchar_t**)&p, 10), y = wcstol(q, (wchar_t**)&q, 10);
        if (x != y) return x < y ? -1 : 1;
        if (*p == L'.') p++;
        if (*q == L'.') q++;
    }
    return 0;
}
static bool IsHttp(const std::wstring& s) { return !StrCmpNIW(s.c_str(), L"http://", 7) || !StrCmpNIW(s.c_str(), L"https://", 8); }
static std::wstring UpdJoin(const std::wstring& base, const std::wstring& name) {
    if (IsHttp(base)) return base + (base.back() == L'/' ? L"" : L"/") + name;
    return base + (base.back() == L'\\' || base.back() == L'/' ? L"" : L"\\") + name;
}
// 공유 폴더 파일 또는 웹 주소를 통째로 읽는다 (최대 512MB)
static bool FetchAll(const std::wstring& loc, std::string& out, std::wstring& err) {
    out.clear();
    if (IsHttp(loc)) {
        HINTERNET net = InternetOpenW(L"Chagok", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!net) { err = L"인터넷 기능을 쓸 수 없습니다."; return false; }
        DWORD to = 15000;
        InternetSetOptionW(net, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
        InternetSetOptionW(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
        HINTERNET u = InternetOpenUrlW(net, loc.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 0);
        bool ok = u != nullptr;
        if (ok) {
            DWORD code = 0, cl = sizeof(code);
            if (HttpQueryInfoW(u, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &cl, nullptr) && code >= 400) { ok = false; err = L"서버 응답 " + std::to_wstring(code) + L": " + loc; }
            char buf[65536];
            DWORD got = 0;
            while (ok && InternetReadFile(u, buf, sizeof(buf), &got) && got) { out.append(buf, got); if (out.size() > (512u << 20)) { ok = false; err = L"파일이 너무 큽니다."; } }
            InternetCloseHandle(u);
        } else err = L"연결할 수 없습니다: " + loc;
        InternetCloseHandle(net);
        return ok;
    }
    HANDLE h = CreateFileW(LongPath(loc).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE) { err = L"파일을 열 수 없습니다: " + loc; return false; }
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    bool ok = sz.QuadPart < (512ll << 20);
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        size_t pos = 0;
        while (ok && pos < out.size()) {
            DWORD want = (DWORD)min<size_t>(out.size() - pos, 1 << 20);
            if (!ReadFile(h, &out[pos], want, &got, nullptr) || !got) ok = false; else pos += got;
        }
    }
    CloseHandle(h);
    if (!ok) err = L"파일을 읽을 수 없습니다: " + loc;
    return ok;
}
static std::wstring Sha256Hex(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE h = nullptr;
    uint8_t d[32];
    std::wstring hex;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0 && BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0) == 0 && BCryptFinishHash(h, d, 32, 0) == 0) {
            wchar_t b[65];
            for (int i = 0; i < 32; i++) swprintf(b + i * 2, 3, L"%02x", d[i]);
            hex = b;
        }
        if (h) BCryptDestroyHash(h);
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return hex;
}
// 설치 파일이 사내 인증서로 서명됐고 서명 뒤에 바뀌지 않았는지 (사내 자체 인증서라 '뿌리 미신뢰'는 허용, 지문으로 확인)
static bool SignedByCompany(const std::wstring& file, std::wstring& why) {
    std::wstring want = CHAGOK_CERT_THUMB;
    if (want.empty()) { why = L"이 빌드에는 사내 인증서 지문이 없습니다."; return false; }
    WINTRUST_FILE_INFO fi{};
    fi.cbStruct = sizeof(fi);
    fi.pcwszFilePath = file.c_str();
    GUID act = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA wd{};
    wd.cbStruct = sizeof(wd);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    wd.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    LONG r = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &act, &wd);
    bool sigOk = r == ERROR_SUCCESS || r == (LONG)CERT_E_UNTRUSTEDROOT || r == (LONG)CERT_E_CHAINING;  // 서명·내용은 온전, 뿌리만 미신뢰
    std::wstring thumb;
    if (wd.hWVTStateData) {
        CRYPT_PROVIDER_DATA* pd = WTHelperProvDataFromStateData(wd.hWVTStateData);
        CRYPT_PROVIDER_SGNR* sg = pd ? WTHelperGetProvSignerFromChain(pd, 0, FALSE, 0) : nullptr;
        CRYPT_PROVIDER_CERT* pc = sg ? WTHelperGetProvCertFromChain(sg, 0) : nullptr;
        BYTE h[20]; DWORD hl = sizeof(h);
        if (pc && pc->pCert && CertGetCertificateContextProperty(pc->pCert, CERT_SHA1_HASH_PROP_ID, h, &hl))
            for (DWORD i = 0; i < hl; i++) { wchar_t b[3]; swprintf(b, 3, L"%02X", h[i]); thumb += b; }
    }
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &act, &wd);
    if (!sigOk) { wchar_t b[16]; swprintf(b, 16, L"%08X", (unsigned)r); why = std::wstring(L"서명이 없거나 손상되었습니다. (") + b + L")"; return false; }
    if (lstrcmpiW(thumb.c_str(), want.c_str())) { why = L"사내 인증서로 서명된 파일이 아닙니다."; return false; }
    return true;
}
static bool ParseUpdIni(const std::string& raw, UpdInfo& u) {
    std::wstring t = Utf8ToW(raw.data(), (int)raw.size()), line;
    if (!t.empty() && t[0] == 0xFEFF) t.erase(0, 1);
    t += L'\n';
    for (wchar_t c : t) {
        if (c != L'\n') { if (c != L'\r') line += c; continue; }
        size_t e = line.find(L'=');
        if (e != std::wstring::npos && line[0] != L';' && line[0] != L'#') {
            std::wstring k = line.substr(0, e), v = line.substr(e + 1);
            while (!k.empty() && k.back() == L' ') k.pop_back();
            while (!v.empty() && v[0] == L' ') v.erase(0, 1);
            if (!lstrcmpiW(k.c_str(), L"version")) u.version = v;
            else if (!lstrcmpiW(k.c_str(), L"file")) u.file = v;
            else if (!lstrcmpiW(k.c_str(), L"sha256")) u.sha256 = v;
            else if (!lstrcmpiW(k.c_str(), L"notes")) {
                for (size_t p; (p = v.find(L"\\n")) != std::wstring::npos;) v.replace(p, 2, L"\n");
                u.notes = v;
            }
        }
        line.clear();
    }
    CharLowerBuffW(u.sha256.empty() ? nullptr : &u.sha256[0], (DWORD)u.sha256.size());
    return !u.version.empty() && !u.file.empty() && u.file.find_first_of(L"\\/:") == std::wstring::npos;
}
// 뒤에서 확인 → 결과를 WM_APP_UPDATE 로 (새 버전이 없고 자동 확인이면 아무것도 보내지 않음)
// GitHub → 사내 위치 순서로 chagok-update.ini 를 읽어 u 를 채운다 (u->base 는 들어올 때 사내 위치, 나갈 때 설치 파일 위치)
static bool UpdFetch(UpdInfo* u, const wchar_t* ghRepo = GH_REPO) {
    std::string raw;
    std::wstring err, gerr;
    // ① GitHub
    std::wstring gh = ghRepo;
    bool ok = FetchAll(gh + L"/releases/latest/download/chagok-update.ini", raw, gerr) && ParseUpdIni(raw, *u);
    if (ok) { u->base = gh + L"/releases/download/v" + u->version + L"/"; u->from = L"GitHub"; }
    else {
        if (gerr.empty()) gerr = L"chagok-update.ini 내용이 올바르지 않습니다.";
        gerr = L"GitHub: " + gerr;
        // ② 사내 위치
        if (!u->base.empty()) {
            *u = UpdInfo{L"", L"", L"", L"", u->base, L"", u->manual, L""};
            ok = FetchAll(UpdJoin(u->base, L"chagok-update.ini"), raw, err) && ParseUpdIni(raw, *u);
            if (ok) u->from = L"사내 위치";
            else { if (err.empty()) err = L"chagok-update.ini 내용이 올바르지 않습니다."; err = gerr + L"\n사내 위치: " + err; }
        } else err = gerr + L"\n(사내 위치는 정해져 있지 않습니다)";
    }
    u->err = ok ? L"" : err;
    return ok;
}
static DWORD WINAPI UpdCheckThread(LPVOID p) {
    UpdInfo* u = (UpdInfo*)p;
    bool ok = UpdFetch(u);
    if (!ok && !u->manual) { delete u; u = nullptr; }
    else if (ok && !u->manual && (VerCmp(u->version, AppVersion()) <= 0 || u->version == RegReadStr(REG_KEY, L"UpdateSkip"))) { delete u; u = nullptr; }
    if (u) PostMessageW(g_hwnd, WM_APP_UPDATE, 0, (LPARAM)u); else g_updChecking = false;
    return 0;
}
static void UpdCheck(bool manual) {
    std::wstring base = RegReadStr(REG_KEY, L"UpdateUrl");  // 사내 위치 (비어 있으면 GitHub 만)
    if (g_updChecking) return;
    g_updChecking = true;
    UpdInfo* u = new UpdInfo();
    u->base = base;
    u->manual = manual;
    if (manual) {  // 직접 누른 경우: 결과를 기다리는 동안 모래시계
        HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        UpdCheckThread(u);
        SetCursor(old);
        return;
    }
    if (HANDLE t = CreateThread(nullptr, 0, UpdCheckThread, u, 0, nullptr)) CloseHandle(t); else { delete u; g_updChecking = false; }
}
// 내려받기·확인·설치 시작. 성공하면 차곡을 닫는다
static void UpdInstall(UpdInfo* u) {
    std::string data;
    std::wstring err, why;
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    bool ok = FetchAll(UpdJoin(u->base, u->file), data, err);
    SetCursor(old);
    if (!ok) { MsgBox(L"업데이트", L"설치 파일을 받지 못했습니다.\n" + err); return; }
    if (u->sha256.size() != 64 || Sha256Hex(data) != u->sha256) { MsgBox(L"업데이트", L"받은 설치 파일이 chagok-update.ini 의 SHA-256 과 다릅니다. 설치하지 않습니다."); return; }
    std::wstring dir = TempWorkDir(L"update"), setup = dir + L"\\" + u->file;
    HANDLE h = CreateFileW(LongPath(setup).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD wr = 0;
    ok = h != INVALID_HANDLE_VALUE && WriteFile(h, data.data(), (DWORD)data.size(), &wr, nullptr) && wr == data.size();
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (!ok) { MsgBox(L"업데이트", L"임시 폴더에 설치 파일을 쓸 수 없습니다."); return; }
    if (!SignedByCompany(setup, why)) { DeleteFileW(setup.c_str()); MsgBox(L"업데이트", L"설치 파일의 서명을 확인하지 못해 설치하지 않습니다.\n" + why); return; }
    RegStr(REG_KEY, L"UpdateNotes", u->notes);
    RegStr(REG_KEY, L"UpdateFrom", AppVersion());
    if ((INT_PTR)ShellExecuteW(nullptr, L"open", setup.c_str(), L"/S /UPDATE", dir.c_str(), SW_SHOWNORMAL) <= 32) {
        MsgBox(L"업데이트", L"설치 프로그램을 실행하지 못했습니다.");
        return;
    }
    DestroyWindow(g_hwnd);  // 설치 프로그램이 파일을 바꿀 수 있게 닫는다 (끝나면 다시 띄워 줌)
}
static void UpdAsk(UpdInfo* u) {
    if (!u->err.empty()) { MsgBox(L"업데이트 확인", L"새 버전을 확인하지 못했습니다.\n" + u->err); delete u; return; }
    if (VerCmp(u->version, AppVersion()) <= 0) { MsgBox(L"업데이트 확인", L"지금 쓰는 " + AppVersion() + L" 이 최신 버전입니다."); delete u; return; }
    DlgSpec d;
    d.title = L"새 버전이 있습니다";
    d.msg = APP_NAME L" " + AppVersion() + L" → " + u->version + (u->notes.empty() ? L"" : L"\n\n" + u->notes) +
            L"\n\n업데이트하면 " APP_NAME L"이 잠시 닫혔다가 설치가 끝나면 다시 열립니다.";
    d.msg += L"\n\n업데이트할까요?";
    d.buttons = {L"지금 업데이트", L"나중에", L"이 버전 건너뛰기"};
    d.def = 0; d.cancel = 1;
    int r = RunDialog(g_hwnd, d);
    if (r == 2) RegStr(REG_KEY, L"UpdateSkip", u->version);  // 이 버전은 다시 묻지 않음 (더 새 버전이 나오면 다시 물음)
    if (r == 0) UpdInstall(u);  // '나중에'는 아무것도 하지 않음 (다음에 켤 때 다시 물음)
    delete u;
}
// 작업 중이거나 다른 창이 떠 있으면 미뤘다가 묻는다
static void UpdArrived(UpdInfo* u) {
    g_updChecking = false;
    if (u->manual) { UpdAsk(u); return; }
    if (Busy() || g_mainHidden || !IsWindowEnabled(g_hwnd)) { delete g_updPending; g_updPending = u; return; }
    UpdAsk(u);
}
static void UpdAfterJob() {
    if (!g_updPending || Busy() || g_mainHidden || !IsWindowEnabled(g_hwnd)) return;
    UpdInfo* u = g_updPending;
    g_updPending = nullptr;
    UpdAsk(u);
}
static void UpdShowUpdated() {  // 업데이트 직후 한 번
    std::wstring from = RegReadStr(REG_KEY, L"UpdateFrom"), notes = RegReadStr(REG_KEY, L"UpdateNotes");
    RegDeleteKeyValueW(HKEY_CURRENT_USER, REG_KEY, L"UpdateFrom");
    RegDeleteKeyValueW(HKEY_CURRENT_USER, REG_KEY, L"UpdateNotes");
    MsgBox(L"업데이트 완료", APP_NAME L" " + AppVersion() + L" 로 업데이트했습니다." + (from.empty() ? L"" : L" (이전 " + from + L")") + (notes.empty() ? L"" : L"\n\n" + notes));
}

static bool IsArchivePath(const std::wstring& p) { return fmt::IsArchivePath(p); }

static void OnDropPaths(std::vector<std::wstring> files);
static void OnDrop(HDROP hd) {
    std::vector<std::wstring> files;
    UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < n; i++) {
        UINT len = DragQueryFileW(hd, i, nullptr, 0);
        std::wstring s(len, 0);
        DragQueryFileW(hd, i, &s[0], len + 1);
        files.push_back(s);
    }
    DragFinish(hd);
    OnDropPaths(files);
}
static void OnDropPaths(std::vector<std::wstring> files) {
    if (files.empty() || Busy()) return;
    SetForegroundWindow(g_hwnd);
    bool oneArc = files.size() == 1 && IsArchivePath(files[0]) && FileExists(files[0]);
    if (g_hasArc && g_arc.readOnly && !oneArc) {
        DlgSpec d;
        d.title = L"파일 추가 불가";
        d.msg = g_arc.readOnlyWhy + L"\n\n끌어다 놓은 항목으로 새 압축 파일을 만들까요?";
        d.buttons = {L"새로 압축", L"취소"};
        d.def = 0; d.cancel = 1;
        if (RunDialog(g_hwnd, d) == 0) StartCompress(files, false);
        return;
    }
    if (g_hasArc) {
        if (oneArc && g_arc.readOnly) { OpenArchive(files[0]); return; }
        if (oneArc) {
            DlgSpec d;
            d.title = FileNameOf(files[0]);
            d.msg = L"이 압축 파일을 열까요, 현재 압축 파일에 추가할까요?";
            d.buttons = {L"열기", L"추가", L"취소"};
            d.def = 0; d.cancel = 2;
            int r = RunDialog(g_hwnd, d);
            if (r == 0) { OpenArchive(files[0]); return; }
            if (r != 1) return;
        }
        StartAdd(files);
        return;
    }
    if (oneArc) { OpenArchive(files[0]); return; }
    DlgSpec d;
    d.title = L"새로 압축";
    d.msg = std::to_wstring(files.size()) + L"개 항목을 압축합니다.";
    d.buttons = {L"압축", L"분할 압축 (" + SplitLabel(g_splitMB) + L")", L"취소"};
    d.def = 0; d.cancel = 2;
    int r = RunDialog(g_hwnd, d);
    if (r == 0 || r == 1) StartCompress(files, r == 1);
}

// ============================================================ 목록 입력
static void EnsureVisible(int i) {
    if (i < 0) return;
    RECT r = ItemRect(i);
    int top = r.top - L.list.top + g_scrollY - (L.tiles ? L.gap / 2 : 0), bot = r.bottom - L.list.top + g_scrollY + (L.tiles ? L.gap / 2 : 0);
    if (top < g_scrollY) g_scrollY = top;
    else if (bot > g_scrollY + ListH()) g_scrollY = bot - ListH();
    ClampScroll();
}
static int TreeAt(POINT p) {
    if (!PtInRect(&L.tree, p)) return -1;
    for (size_t i = 0; i < L.trows.size(); i++) if (PtInRect(&L.trows[i].rc, p)) return (int)i;
    return -1;
}
static void SelectRow(int i, bool ctrl, bool shift) {
    if (i < 0) { if (!ctrl && !shift) std::fill(g_sel.begin(), g_sel.end(), 0); return; }
    if (shift && g_anchor >= 0) {
        if (!ctrl) std::fill(g_sel.begin(), g_sel.end(), 0);
        for (int k = min(g_anchor, i); k <= max(g_anchor, i); k++) g_sel[(size_t)k] = 1;
    } else if (ctrl) {
        g_sel[(size_t)i] = !g_sel[(size_t)i];
        g_anchor = i;
    } else {
        std::fill(g_sel.begin(), g_sel.end(), 0);
        g_sel[(size_t)i] = 1;
        g_anchor = i;
    }
    g_focus = i;
}
static int HitBtn(POINT p) {
    for (auto it = L.btns.rbegin(); it != L.btns.rend(); ++it)
        if (it->id && it->enabled && PtInRect(&it->rc, p)) return it->id;
    if (L.hasSb && PtInRect(&L.sbTrack, p)) return -2;
    return 0;
}

// ============================================================ 메인 창
static HBITMAP g_buf;
static HDC g_bufDC;
static int g_bufW, g_bufH;

static int g_dragRow = -1;
static POINT g_dragPt;
static bool g_dragDefer = false;
static WNDPROC g_searchOld;
static LRESULT CALLBACK SearchProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    if (m == WM_KEYDOWN) {
        if (w == VK_ESCAPE) { SetWindowTextW(h, L""); SetFocus(g_hwnd); return 0; }
        if (w == VK_RETURN || w == VK_DOWN) {
            SetFocus(g_hwnd);
            if (!g_rows.empty()) { SelectRow(0, false, false); EnsureVisible(0); Relayout(); }
            return 0;
        }
    }
    if (m == WM_CHAR && (w == L'\r' || w == 27)) return 0;  // 경고음 막기
    return CallWindowProcW(g_searchOld, h, m, w, lp);
}
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    switch (m) {
    case WM_CREATE:
        g_hwnd = h;
        ApplyTitleBar(h);
        DragAcceptFiles(h, TRUE);
        g_searchEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, h, (HMENU)(INT_PTR)50, nullptr, nullptr);
        SendMessageW(g_searchEdit, 0x1501 /*EM_SETCUEBANNER*/, FALSE, (LPARAM)L"이 압축 파일에서 찾기 (Ctrl+F)");
        g_searchOld = (WNDPROC)SetWindowLongPtrW(g_searchEdit, GWLP_WNDPROC, (LONG_PTR)SearchProc);
        SetTimer(h, 77, 700, nullptr);  // 둘러보는 폴더가 바뀌었는지 확인
        return 0;
    case WM_TIMER:
        if (w == 78) { KillTimer(h, 78); if (g_hoverId == ID_THEME && IsWindowEnabled(h) && GetForegroundWindow() == h) AlbumOpen(); return 0; }
        if (w == 79) { AlbumTick(); return 0; }
        if (w == 77) UpdAfterJob();  // 미뤄 둔 업데이트 질문 (작업·다른 창이 끝나면)
        if (w == 77 && g_fs && g_fsWatch != INVALID_HANDLE_VALUE && WaitForSingleObject(g_fsWatch, 0) == WAIT_OBJECT_0) {
            static ULONGLONG last = 0;  // 큰 파일을 쓰는 중이면 알림이 계속 오므로 1.5초에 한 번만
            if (GetTickCount64() - last >= 1500 && !g_dragging && GetCapture() != h) {
                FindNextChangeNotification(g_fsWatch);
                last = GetTickCount64();
                FsRefresh();
            }
        }
        return 0;
    case WM_DEVICECHANGE:  // USB 등을 꽂거나 뺐을 때 내 PC 목록 다시
        if (g_fs && (w == 0x8000 /*DBT_DEVICEARRIVAL*/ || w == 0x8004 /*DBT_DEVICEREMOVECOMPLETE*/)) {
            BuildPlaces();
            if (g_fsPath.empty()) FsRefresh(); else Relayout();
        }
        return TRUE;
    case WM_COMMAND:
        if ((HWND)lp == g_searchEdit) {
            if (HIWORD(w) == EN_CHANGE && !g_searchSilent && HasList()) {
                wchar_t b[256] = {};
                GetWindowTextW(g_searchEdit, b, 256);
                std::wstring q = b;
                while (!q.empty() && q.back() == L' ') q.pop_back();
                while (!q.empty() && q[0] == L' ') q.erase(0, 1);
                if (q != g_search) { g_search = q; ShowCur(); Relayout(); }
            }
            if (HIWORD(w) == EN_SETFOCUS || HIWORD(w) == EN_KILLFOCUS) InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_CTLCOLOREDIT: {
        static HBRUSH br = nullptr;
        static COLORREF brc = NONE;
        if (brc != P.btn) { if (br) DeleteObject(br); br = CreateSolidBrush(P.btn); brc = P.btn; }
        SetTextColor((HDC)w, P.text);
        SetBkColor((HDC)w, P.btn);
        return (LRESULT)br;
    }
    case WM_APP_TRAY:
        switch (LOWORD(lp)) {
        case NIN_BALLOONUSERCLICK: OpenInExplorer(g_notifyOpen); TrayRemove(); break;
        case NIN_BALLOONTIMEOUT: case NIN_BALLOONHIDE: TrayRemove(); break;
        case WM_LBUTTONUP: ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); TrayRemove(); break;
        }
        return 0;
    case WM_CONTEXTMENU: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if ((HWND)w != h || !HasList()) return 0;
        if (p.x == -1 && p.y == -1) {  // 키보드(메뉴 키 / Shift+F10)
            RECT r = g_focus >= 0 ? ItemRect(g_focus) : L.list;
            p = {r.left + S(40), r.top + S(10)};
            ClientToScreen(h, &p);
        } else {
            POINT c = p;
            ScreenToClient(h, &c);
            if (!PtInRect(&L.list, c)) return 0;
            int r = ItemAt(c);
            if (r >= 0 && !g_sel[(size_t)r]) { SelectRow(r, false, false); Relayout(); }
            else if (r < 0) { SelectRow(-1, false, false); Relayout(); }
        }
        CmdContextMenu(p);
        return 0;
    }
    case WM_SIZE: Relayout(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT cr; GetClientRect(h, &cr);
        if (!g_bufDC || g_bufW != cr.right || g_bufH != cr.bottom) {
            if (g_bufDC) { DeleteDC(g_bufDC); DeleteObject(g_buf); }
            g_bufDC = CreateCompatibleDC(dc);
            g_buf = CreateCompatibleBitmap(dc, max(1L, cr.right), max(1L, cr.bottom));
            SelectObject(g_bufDC, g_buf);
            g_bufW = cr.right; g_bufH = cr.bottom;
        }
        Paint(g_bufDC, cr.right, cr.bottom);
        BitBlt(dc, 0, 0, cr.right, cr.bottom, g_bufDC, 0, 0, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (g_dragRow >= 0 && (w & MK_LBUTTON) &&
            (abs(p.x - g_dragPt.x) > GetSystemMetrics(SM_CXDRAG) || abs(p.y - g_dragPt.y) > GetSystemMetrics(SM_CYDRAG))) {
            g_dragRow = -1; g_dragDefer = false;
            if (GetCapture() == h) ReleaseCapture();
            DoDragOut();
            return 0;
        }
        if (g_dragThumb) {
            int th = L.sbTrack.bottom - L.sbTrack.top, tb = L.sbThumb.bottom - L.sbThumb.top;
            int range = max(1, th - tb);
            g_scrollY = g_dragScroll0 + (int)((int64_t)(p.y - g_dragY0) * (ContentH() - ListH()) / range);
            Relayout();
            return 0;
        }
        int hid = HitBtn(p);
        int hr = (hid == 0 && HasList()) ? ItemAt(p) : -1;
        int ht = hid == 0 ? TreeAt(p) : -1;
        if (hid != g_hoverId && hid == ID_THEME && !g_album) SetTimer(h, 78, 350, nullptr);  // 잠시 머물면 테마 앨범
        if (hid != g_hoverId || hr != g_hoverRow || ht != g_hoverTree) { g_hoverId = hid; g_hoverRow = hr; g_hoverTree = ht; InvalidateRect(h, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_hoverId || g_hoverRow >= 0 || g_hoverTree >= 0) { g_hoverId = 0; g_hoverRow = -1; g_hoverTree = -1; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        SetFocus(h);
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int id = HitBtn(p);
        if (id == -2) {
            if (PtInRect(&L.sbThumb, p)) { g_dragThumb = true; g_dragY0 = p.y; g_dragScroll0 = g_scrollY; SetCapture(h); }
            else { g_scrollY += (p.y < L.sbThumb.top ? -1 : 1) * ListH(); Relayout(); }
            return 0;
        }
        if (id) { g_pressId = id; SetCapture(h); InvalidateRect(h, nullptr, FALSE); return 0; }
        int ti = TreeAt(p);
        if (ti >= 0) {
            const TreeRow& t = L.trows[(size_t)ti];
            int cx = t.rc.left + S(8) + t.depth * S(16);
            if (g_fs) { FsNavigate(g_places[(size_t)t.node].path); return 0; }
            if (t.kids && p.x >= cx - S(4) && p.x < cx + S(18)) g_expand[(size_t)t.node] = !g_expand[(size_t)t.node];
            else if (t.node != g_cur || !g_search.empty()) { ClearSearch(); ShowDir(t.node); }
            Relayout();
            return 0;
        }
        if (HasList() && !L.tiles && PtInRect(&L.header, p) && L.header.bottom > L.header.top) {  // 열 머리글: 정렬
            Cols c = GetCols();
            int col = p.x < c.size2 - S(84) ? 0 : p.x < c.size2 + S(6) ? 1 : p.x < c.csize2 + S(6) ? 2 : (c.date && p.x >= c.date1 - S(8)) ? 3 : -1;
            if (col >= 0) {
                if (col == g_sortCol) g_sortDesc = !g_sortDesc; else { g_sortCol = col; g_sortDesc = col != 0; }
                RegSet(L"SortCol", (DWORD)g_sortCol); RegSet(L"SortDesc", g_sortDesc ? 1 : 0);
                SortRows();
                std::fill(g_sel.begin(), g_sel.end(), 0);
                g_focus = g_anchor = -1;
                Relayout();
            }
            return 0;
        }
        if (HasList() && PtInRect(&L.list, p)) {
            int r = ItemAt(p);
            bool ctrl = (w & MK_CONTROL) != 0, shift = (w & MK_SHIFT) != 0;
            g_dragRow = r; g_dragPt = p;
            g_dragDefer = r >= 0 && g_sel[(size_t)r] && !ctrl && !shift;  // 여러 개 선택한 채로 끌 수 있게 선택은 놓을 때 정리
            if (!g_dragDefer) SelectRow(r, ctrl, shift);
            if (r >= 0) SetCapture(h);
            Relayout();
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (g_dragThumb) { g_dragThumb = false; ReleaseCapture(); InvalidateRect(h, nullptr, FALSE); return 0; }
        if (g_dragRow >= 0) {
            if (g_dragDefer) { SelectRow(g_dragRow, false, false); Relayout(); }
            g_dragRow = -1; g_dragDefer = false;
            if (GetCapture() == h) ReleaseCapture();
            return 0;
        }
        int id = g_pressId;
        g_pressId = 0;
        if (GetCapture() == h) ReleaseCapture();
        if (id && HitBtn(p) == id) OnCommand(id);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (HitBtn(p)) { SendMessageW(h, WM_LBUTTONDOWN, w, lp); return 0; }
        if (TreeAt(p) >= 0) { SendMessageW(h, WM_LBUTTONDOWN, w, lp); return 0; }
        if (HasList() && PtInRect(&L.list, p)) Activate(ItemAt(p));
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (HasList()) {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(h, &p);
            int step = GET_WHEEL_DELTA_WPARAM(w) * 3 / WHEEL_DELTA;
            if (PtInRect(&L.tree, p)) g_treeScroll -= step * L.treeRowH;
            else g_scrollY -= step * (L.tiles ? (L.tileH + L.gap) / 2 : L.rowH);
            Relayout();
        }
        return 0;
    case WM_KEYDOWN: {
        if (w == VK_ESCAPE && g_album) { AlbumClose(); return 0; }
        if (!HasList()) {
            if (w == 'O' && GetKeyState(VK_CONTROL) < 0) OnCommand(ID_OPEN);
            return 0;
        }
        bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
        int step = L.tiles ? L.cols : 1, rowPx = L.tiles ? L.tileH + L.gap : L.rowH;
        int n = (int)g_rows.size(), f = g_focus, page = max(1, ListH() / rowPx - 1) * step;
        int nf = f;
        switch (w) {
        case VK_UP: nf = f < 0 ? 0 : f - step; break;
        case VK_DOWN: nf = f < 0 ? 0 : f + step; break;
        case VK_LEFT: if (!L.tiles) return 0; nf = f - 1; break;
        case VK_RIGHT: if (!L.tiles) return 0; nf = f + 1; break;
        case VK_PRIOR: nf = f - page; break;
        case VK_NEXT: nf = f + page; break;
        case VK_HOME: nf = 0; break;
        case VK_END: nf = n - 1; break;
        case VK_RETURN: Activate(f); return 0;
        case VK_BACK: GoUp(); return 0;
        case VK_ESCAPE: if (!g_search.empty()) { ClearSearch(); ShowCur(); Relayout(); } return 0;
        case VK_F5: if (g_fs) FsRefresh(); return 0;
        case VK_SPACE: CmdPreview(); return 0;
        case VK_F2: CmdRename(); return 0;
        case VK_DELETE: CmdDelete(); return 0;
        case 'F': if (ctrl && g_searchEdit) { SetFocus(g_searchEdit); SendMessageW(g_searchEdit, EM_SETSEL, 0, -1); } return 0;
        case 'A': if (ctrl) { std::fill(g_sel.begin(), g_sel.end(), 1); Relayout(); } return 0;
        case 'O': if (ctrl) OnCommand(ID_OPEN); return 0;
        default: return 0;
        }
        if (n == 0) return 0;
        nf = max(0, min(n - 1, nf));
        if (ctrl && !shift) g_focus = nf; else SelectRow(nf, false, shift);
        EnsureVisible(nf);
        Relayout();
        return 0;
    }
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_DROPFILES:
        if (g_dragging) { DragFinish((HDROP)w); return 0; }  // 끌어서 꺼내는 중 자기 창에 놓은 경우
        OnDrop((HDROP)w);
        return 0;
    case WM_APP_JOBDONE: OnJobDone(); return 0;
    case WM_APP_UPDATE: UpdArrived((UpdInfo*)lp); return 0;
    case WM_APP_UPDATE + 1: UpdCheck(true); return 0;  // 설정 창의 '지금 확인' (설정 창을 닫은 뒤)
    case WM_APP_PASSWORD: return AskPassword(*(std::wstring*)lp, false) ? 1 : 0;
    case WM_APP_ASK: {
        const std::wstring& p = *(const std::wstring*)lp;
        DlgSpec d;
        d.title = L"같은 이름의 파일이 있습니다";
        d.msg = p + L"\n\n덮어쓸까요?";
        d.buttons = {L"덮어쓰기", L"모두 덮어쓰기", L"건너뛰기", L"모두 건너뛰기", L"취소"};
        d.def = 0; d.cancel = 4;
        int r = RunDialog(h, d);
        return r < 0 ? 4 : r;
    }
    case WM_SETTINGCHANGE:
        if (lp && !lstrcmpW((LPCWSTR)lp, L"ImmersiveColorSet") && g_mode == MODE_SYSTEM) {
            ApplyTheme(false);
            Relayout();
            if (g_prog.h) InvalidateRect(g_prog.h, nullptr, FALSE);
        }
        return 0;
    case WM_DPICHANGED: {
        g_dpi = HIWORD(w);
        MakeFonts();
        RECT* r = (RECT*)lp;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Relayout();
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize = {S(g_ly == TH_MINT ? 900 : 800), S(520)};
        return 0;
    }
    case WM_CLOSE:
        if (Busy()) {
            DlgSpec d;
            d.title = L"작업 중";
            d.msg = std::wstring(JobName()) + L" 작업이 진행 중입니다. 취소하고 종료할까요?";
            d.buttons = {L"취소하고 종료", L"계속 작업"};
            d.def = 1; d.cancel = 1;
            if (RunDialog(h, d) == 0 && g_job) { g_quitAfterJob = true; g_job->job.cancel = true; }
            return 0;
        }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: TrayRemove(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, lp);
}

// ============================================================ 다중 선택 모으기
// 탐색기는 여러 항목을 우클릭 실행하면 항목마다 프로세스를 하나씩 띄운다.
// 처음 뜬 프로세스가 '수집 창'을 만들고, 나머지는 경로를 WM_COPYDATA 로 넘기고 종료한다.
static std::vector<std::wstring>* g_collected;
static LRESULT CALLBACK CollectProc(HWND h, UINT m, WPARAM w, LPARAM lp) {
    if (m == WM_COPYDATA) {
        auto* cd = (COPYDATASTRUCT*)lp;
        if (cd->dwData == 0x5A50 && g_collected) {
            g_collected->push_back(std::wstring((const wchar_t*)cd->lpData, cd->cbData / sizeof(wchar_t) - 1));
            SetTimer(h, 1, 700, nullptr);  // 마지막 도착 후 0.7초 조용하면 끝
        }
        return TRUE;
    }
    if (m == WM_TIMER) { KillTimer(h, 1); PostMessageW(h, WM_USER, 0, 0); return 0; }
    return DefWindowProcW(h, m, w, lp);
}
// 반환: true = 이 프로세스가 대표(목록 처리), false = 다른 프로세스에 넘겼으니 종료
static bool CollectInvocations(const std::wstring& verb, std::vector<std::wstring>& list) {
    std::wstring name = L"Local\\ChagokCollect_" + verb;
    HANDLE mx = CreateMutexW(nullptr, FALSE, name.c_str());
    bool exists = GetLastError() == ERROR_ALREADY_EXISTS;
    if (exists) {
        for (int i = 0; i < 100; i++) {  // 최대 5초 대기
            HWND w = FindWindowExW(HWND_MESSAGE, nullptr, L"ChagokCollector", verb.c_str());
            if (w) {
                COPYDATASTRUCT cd{0x5A50, (DWORD)((list[0].size() + 1) * sizeof(wchar_t)), (PVOID)list[0].c_str()};
                DWORD_PTR r = 0;
                if (SendMessageTimeoutW(w, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 3000, &r) && r) { CloseHandle(mx); return false; }
            }
            Sleep(50);
        }
    }
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = CollectProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ChagokCollector";
    RegisterClassExW(&wc);
    g_collected = &list;
    HWND h = CreateWindowExW(0, L"ChagokCollector", verb.c_str(), 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    SetTimer(h, 1, 700, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == h && msg.message == WM_USER) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DestroyWindow(h);
    g_collected = nullptr;
    CloseHandle(mx);
    // 같은 항목 중복 제거
    std::sort(list.begin(), list.end(), [](const std::wstring& a, const std::wstring& b) { return lstrcmpiW(a.c_str(), b.c_str()) < 0; });
    list.erase(std::unique(list.begin(), list.end(), [](const std::wstring& a, const std::wstring& b) { return !lstrcmpiW(a.c_str(), b.c_str()); }), list.end());
    return true;
}

#ifdef CHAGOK_SHOTS
#include "../test/shots.inc"
#include "../test/func.inc"
#endif
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    g_theme = (int)min<DWORD>(TH_COUNT - 1, RegGet(L"Theme", TH_BASIC));
    g_mode = (int)min<DWORD>(2, RegGet(L"ColorMode", MODE_SYSTEM));
    g_view = RegGet(L"View", 0) ? 1 : 0;
    g_opacity = (int)max<DWORD>(40, min<DWORD>(100, RegGet(L"Opacity", 100)));
    g_preset = (int)min<DWORD>(2, RegGet(L"Preset", 0));
    g_sortCol = (int)min<DWORD>(3, RegGet(L"SortCol", 0));
    g_sortDesc = RegGet(L"SortDesc", 0) != 0;
    g_splitMB = max<DWORD>(1, RegGet(L"SplitMB", 100));

    // 명령줄: Chagok.exe [파일] | /compress 경로 | /split 경로 | /x-here|/x-folder|/x-to 압축파일 | /register | /unregister
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring verb;
    std::vector<std::wstring> paths;
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a.size() > 1 && a[0] == L'/' && verb.empty() && paths.empty()) { verb = a.substr(1); continue; }
        if (a.size() > 1 && a[0] == L'@') {  // 우클릭 메뉴가 넘긴 목록 파일 (UTF-16)
            HANDLE h = CreateFileW(a.c_str() + 1, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
                std::wstring body((size_t)sz.QuadPart / sizeof(wchar_t), 0);
                DWORD got = 0;
                if (!body.empty()) ReadFile(h, &body[0], (DWORD)sz.QuadPart, &got, nullptr);
                CloseHandle(h);
                DeleteFileW(a.c_str() + 1);
                size_t p = 0;
                while (p < body.size()) {
                    size_t e = body.find(L'\n', p);
                    if (e == std::wstring::npos) e = body.size();
                    std::wstring line = body.substr(p, e - p);
                    while (!line.empty() && (line.back() == L'\r' || line[0] == 0xFEFF)) { if (line.back() == L'\r') line.pop_back(); else line.erase(0, 1); }
                    if (!line.empty()) paths.push_back(line);
                    p = e + 1;
                }
            }
            continue;
        }
        wchar_t full[4096];
        GetFullPathNameW(a.c_str(), 4096, full, nullptr);
        paths.push_back(full);
    }
    LocalFree(argv);
    if (verb == L"register") return ShellRegister() ? 0 : 1;
    if (verb == L"unregister") { ShellUnregister(); return 0; }
    if (verb == L"register-assoc") { AssocRegister(); return 0; }
    if (verb == L"unregister-assoc") { AssocUnregister(); return 0; }
    if (verb == L"default-apps") { AssocRegister(); OpenDefaultAppsSettings(); return 0; }
    if ((verb == L"compress" || verb == L"split") && !paths.empty())
        if (!CollectInvocations(verb, paths)) return 0;

    OleInitialize(nullptr);  // 끌어서 꺼내기(드래그 앤 드롭)에 필요
    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, nullptr);
    g_mdc = CreateCompatibleDC(nullptr);
    g_dpi = GetDpiForSystem();
    ApplyTheme(false);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    g_iconSel = (int)min<DWORD>(2, RegGet(L"Icon", 1));
    LoadAppIcons();
    wc.hIcon = g_iconBig;
    wc.hIconSm = g_iconSmall;
    wc.lpszClassName = L"ChagokMain";
    RegisterClassExW(&wc);
    WNDCLASSEXW pc{sizeof(pc)};
    pc.style = CS_DROPSHADOW;
    pc.lpfnWndProc = PopupProc;
    pc.hInstance = hi;
    pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pc.lpszClassName = L"ChagokPopup";
    RegisterClassExW(&pc);
    pc.lpfnWndProc = DlgProc;
    pc.lpszClassName = L"ChagokDialog";
    pc.hIcon = wc.hIcon;
    RegisterClassExW(&pc);
    pc.lpfnWndProc = ProgProc;
    pc.lpszClassName = L"ChagokProgress";
    RegisterClassExW(&pc);
    pc.lpfnWndProc = SetProc;
    pc.lpszClassName = L"ChagokSettings";
    RegisterClassExW(&pc);
    pc.lpfnWndProc = PreviewProc;
    pc.lpszClassName = L"ChagokPreview";
    RegisterClassExW(&pc);
    pc.lpfnWndProc = AlbumProc;
    pc.lpszClassName = L"ChagokAlbum";
    pc.hIcon = nullptr;
    RegisterClassExW(&pc);

    int W = S(1100), H = S(700);
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    W = min(W, (int)(wa.right - wa.left)); H = min(H, (int)(wa.bottom - wa.top));
    HWND hw = CreateWindowExW(WS_EX_ACCEPTFILES, L"ChagokMain", APP_NAME, WS_OVERLAPPEDWINDOW,
                              wa.left + (wa.right - wa.left - W) / 2, wa.top + (wa.bottom - wa.top - H) / 2, W, H,
                              nullptr, nullptr, hi, nullptr);
    g_dpi = GetDpiForWindow(hw);
    MakeFonts();
    UpdateTitle();
    DoLayout();
    ApplyOpacity(hw);
    // 우클릭 메뉴의 압축/풀기는 메인 창 없이 설정 창·진행 창만 띄운다
    bool menuJob = verb == L"compress" || verb == L"split" || verb == L"x-here" || verb == L"x-folder" || verb == L"x-to" || verb == L"x-smart";
    g_mainHidden = menuJob;
    if (!menuJob) { ShowWindow(hw, show); UpdateWindow(hw); }
    ShellRefreshPath();
    CleanTemp();
    if (verb == L"compress" || verb == L"split") {
        if (paths.empty()) DestroyWindow(hw);
        else { g_menuCompress = true; StartCompress(paths, verb == L"split"); }
        if (IsWindow(hw) && !Busy()) DestroyWindow(hw);  // 설정 창에서 취소
    } else if (verb == L"x-here" || verb == L"x-folder" || verb == L"x-to" || verb == L"x-smart") {
        g_menuLaunch = true;
        g_quietDone = verb != L"x-to";
        if (paths.empty() || !OpenArchive(paths[0]) ||
            !ExtractAllTo(verb == L"x-smart" ? XT_SMART : verb == L"x-here" ? XT_HERE : verb == L"x-folder" ? XT_FOLDER : XT_PICK))
            DestroyWindow(hw);
    } else if (verb == L"open") {
        // 우클릭 '차곡으로 열기': 압축 파일 하나면 열고, 그 밖이면 끌어다 놓은 것처럼 압축할지 묻는다
        if (paths.size() == 1 && IsArchivePath(paths[0]) && FileExists(paths[0])) OpenArchive(paths[0]);
        else if (!paths.empty()) OnDropPaths(paths);
    } else if (!paths.empty() && FileExists(paths[0])) {
        OpenArchive(paths[0]);
    }
    if (verb == L"updated") UpdShowUpdated();
    if (!menuJob && RegGet(L"UpdateAuto", 1)) UpdCheck(false);
#ifdef CHAGOK_SHOTS
    if (verb.empty() && !EnvW(L"SHOT_DIR").empty() && EnvW(L"OW_ARC").empty()) ShotsRun();
    else if (verb.empty() && !EnvW(L"FUNC_LOG").empty()) FuncRun();
    else if (verb.empty() && !EnvW(L"OW_ARC").empty()) OwRun();
#endif
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    GdiplusShutdown(g_gdiplusToken);
    OleUninitialize();
    return 0;
}

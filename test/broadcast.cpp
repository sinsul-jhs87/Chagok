// 테스트용: 시스템 테마 변경 알림(WM_SETTINGCHANGE "ImmersiveColorSet") 브로드캐스트
#include <windows.h>
int main() {
    DWORD_PTR r;
    HWND h = FindWindowW(L"ChagokMain", nullptr);
    SendMessageTimeoutW(h ? h : HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 2000, &r);
    return 0;
}

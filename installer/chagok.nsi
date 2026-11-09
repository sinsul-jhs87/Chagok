; 차곡 설치 프로그램 (NSIS 3)
; 사용자 단위 설치: %LOCALAPPDATA%\Programs\Chagok 에 설치하고 탐색기 우클릭 메뉴를 자동 등록한다.
; Windows 11 에서는 사내 인증서로 서명한 스파스 패키지를 등록해 우클릭 첫 화면에 '차곡'을 넣는다.
; 이때 인증서를 PC 의 '신뢰할 수 있는 사용자' 저장소에 넣는 한 단계만 관리자 확인(UAC)이 뜬다.
Unicode true
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

!ifndef VERSION
  !define VERSION "1.18.0"
!endif
!define APPNAME   "차곡"
!define APPID     "Chagok"
!define PUBLISHER "주식회사신설"
!define EXE       "Chagok.exe"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPID}"
!define PKGNAME   "Sinsul.Chagok"
; 서명 인증서 지문(SHA-1) — build_mingw.sh 가 installer/cert/chagok_sign.cer 에서 계산해 넘긴다
!ifndef CERT_THUMB
  !define CERT_THUMB "FFE829C8F450C57FF22230832006B0CCE5168213"
!endif
Var IsWin11
Var Update   ; 1 = 차곡의 자동 업데이트가 /S /UPDATE 로 실행함 (질문 없이 설치하고 끝나면 차곡을 다시 띄움)
Var PS

Name "${APPNAME}"
OutFile "..\build\ChagokSetup-${VERSION}.exe"
InstallDir "$LOCALAPPDATA\Programs\${APPID}"
InstallDirRegKey HKCU "${UNINSTKEY}" "InstallLocation"
RequestExecutionLevel user
ManifestDPIAware true
BrandingText "${APPNAME} ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1042 "ProductName" "${APPNAME}"
VIAddVersionKey /LANG=1042 "FileDescription" "${APPNAME} 설치"
VIAddVersionKey /LANG=1042 "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=1042 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1042 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1042 "LegalCopyright" "© ${PUBLISHER}"

!define MUI_ICON "..\res\app.ico"
!define MUI_UNICON "..\res\app.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "${APPNAME} 실행"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "기본 압축 프로그램으로 설정 (Windows 설정 열기)"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION OpenDefaultApps

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Korean"

; 실행 중인 차곡 창을 닫는다 (창 클래스 ChagokMain. 1.13.4 이하에서 올리는 경우를 위해 옛 클래스 이름도 찾는다)
!macro CLOSE_RUNNING_FN PFX
Function ${PFX}CloseRunning
  StrCpy $1 0
  check:
    FindWindow $0 "ChagokMain" ""
    StrCmp $0 0 0 found
    FindWindow $0 "ZipperMain" ""   ; 1.13.4 이하
    StrCmp $0 0 done
  found:
    IntCmp $1 0 ask send send
  ask:
    MessageBox MB_OKCANCEL|MB_ICONINFORMATION "${APPNAME}이(가) 실행 중입니다. 확인을 누르면 종료하고 계속합니다." /SD IDOK IDOK send
    Abort
  send:
    SendMessage $0 0x0010 0 0 /TIMEOUT=2000
    Sleep 300
    IntOp $1 $1 + 1
    IntCmp $1 20 fail check fail
  fail:
    MessageBox MB_OK|MB_ICONSTOP "${APPNAME}을(를) 종료할 수 없습니다. 작업을 마친 뒤 다시 시도하세요." /SD IDOK
    Abort
  done:
FunctionEnd
!macroend
!insertmacro CLOSE_RUNNING_FN ""
!insertmacro CLOSE_RUNNING_FN "un."

; 탐색기가 물고 있는(로드한) 메뉴 DLL 은 지울 수 없으므로 임시 폴더로 옮겨 둔다(같은 드라이브면 이동 가능).
!macro MOVE_LOCKED_DLL_FN PFX
Function ${PFX}MoveLockedDll
  Delete "$TEMP\ChagokShell-old-*.dll"
  IfFileExists "$INSTDIR\ChagokShell.dll" 0 done
  Delete "$INSTDIR\ChagokShell.dll"
  IfFileExists "$INSTDIR\ChagokShell.dll" 0 done
  System::Call 'kernel32::GetTickCount()i.r0'
  Rename "$INSTDIR\ChagokShell.dll" "$TEMP\ChagokShell-old-$0.dll"
  done:
FunctionEnd
!macroend
!insertmacro MOVE_LOCKED_DLL_FN ""
!insertmacro MOVE_LOCKED_DLL_FN "un."

Function OpenDefaultApps
  Exec '"$INSTDIR\${EXE}" /default-apps'
FunctionEnd

; ---------------------------------------------------------------- Windows 11 첫 화면 메뉴 (스파스 패키지)
!macro FIND_PS_FN PFX
Function ${PFX}FindPowerShell
  ; 32비트 설치 프로그램에서도 64비트 PowerShell 을 쓴다 (Appx 명령)
  StrCpy $PS "$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
  IfFileExists $PS +2 0
  StrCpy $PS "$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
FunctionEnd
!macroend
!insertmacro FIND_PS_FN ""
!insertmacro FIND_PS_FN "un."

Function DetectWin11
  StrCpy $IsWin11 0
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CurrentBuildNumber"
  IntCmp $0 22000 0 +2 0
  StrCpy $IsWin11 1
FunctionEnd

; 결과 $0 = 1 이면 인증서가 신뢰됨
Function CertTrusted
  nsExec::ExecToStack `"$PS" -NoProfile -NonInteractive -Command "if (Test-Path Cert:\LocalMachine\TrustedPeople\${CERT_THUMB}) { exit 0 } else { exit 1 }"`
  Pop $0
  Pop $1
  ${If} $0 == 0
    StrCpy $0 1
  ${Else}
    StrCpy $0 0
  ${EndIf}
FunctionEnd

Function InstallCert
  Call CertTrusted
  ${If} $0 == 1
    Return
  ${EndIf}
  MessageBox MB_OK|MB_ICONINFORMATION "Windows 11 우클릭 첫 화면에 '${APPNAME}' 메뉴를 넣으려면 사내 인증서(Sinsul Chagok)를 이 PC 에 등록해야 합니다.$\r$\n$\r$\n다음에 뜨는 관리자 확인 창에서 '예'를 눌러 주세요." /SD IDOK
  ExecShellWait "runas" "$SYSDIR\certutil.exe" `-addstore TrustedPeople "$INSTDIR\Chagok.cer"` SW_HIDE
  Call CertTrusted
  ${If} $0 == 1
    WriteRegDWORD HKCU "${UNINSTKEY}" "CertAdded" 1
  ${Else}
    MessageBox MB_OK|MB_ICONEXCLAMATION "인증서를 등록하지 못해 예전 방식 메뉴로 설치합니다.$\r$\n(우클릭 → '추가 옵션 표시' 안에 '${APPNAME}'이 나타납니다)" /SD IDOK
  ${EndIf}
FunctionEnd

; 결과 $0 = 1 이면 패키지 등록 성공. 실패하면 이유를 보여 준다
Function RegisterPackage
  nsExec::ExecToStack `"$PS" -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "$$ErrorActionPreference = 'Stop'; Get-AppxPackage -Name ${PKGNAME} | Remove-AppxPackage; Add-AppxPackage -Path '$INSTDIR\ChagokShell.msix' -ExternalLocation '$INSTDIR'"`
  Pop $0
  Pop $1
  ; 종료 코드만 믿지 않고 실제로 등록됐는지 확인
  Call PackageInstalled
  ${If} $0 != 1
    MessageBox MB_OK|MB_ICONEXCLAMATION "Windows 11 첫 화면 메뉴를 등록하지 못해 예전 방식 메뉴로 설치합니다.$\r$\n$\r$\n$1" /SD IDOK
  ${EndIf}
FunctionEnd

; 결과 $0 = 1 이면 Sinsul.Chagok 패키지가 이 사용자에게 등록되어 있음
Function PackageInstalled
  nsExec::ExecToStack `"$PS" -NoProfile -NonInteractive -Command "if (Get-AppxPackage -Name ${PKGNAME}) { exit 0 } else { exit 1 }"`
  Pop $0
  Pop $2
  ${If} $0 == 0
    StrCpy $0 1
  ${Else}
    StrCpy $0 0
  ${EndIf}
FunctionEnd

Function .onInit
  StrCpy $Update 0
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/UPDATE" $R1
  IfErrors +2 0
  StrCpy $Update 1
  ; 자동 업데이트: 차곡이 스스로 닫히는 동안 잠깐 기다린다
  ${If} $Update == 1
    Sleep 800
  ${EndIf}
  Call CloseRunning
  Call DetectWin11
  Call FindPowerShell
FunctionEnd

Section "${APPNAME} 프로그램 (필수)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  Call MoveLockedDll
  File "..\build\${EXE}"
  File "..\build\ChagokShell.dll"
  File "..\build\7z.dll"
  File "..\build\ChagokSFX.bin"
  File "..\build\7-Zip_License.txt"
  File "..\README.md"
  File "..\src\miniz_LICENSE.txt"
  File "..\src\zlib-ng_LICENSE.txt"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateShortCut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0

  ; ZIP '연결 프로그램' 후보 + '기본 앱' 목록에 등록 (실제 기본값 선택은 사용자가 설정에서)
  ExecWait '"$INSTDIR\${EXE}" /register-assoc'

  ; 이전 이름(Zipper) 시절 우클릭 메뉴 정리
  DeleteRegKey HKCU "Software\Classes\*\shell\ZipperCompress"
  DeleteRegKey HKCU "Software\Classes\Directory\shell\ZipperCompress"
  DeleteRegKey HKCU "Software\Classes\SystemFileAssociations\.zip\shell\ZipperExtract"
  DeleteRegKey HKCU "Software\Classes\SystemFileAssociations\.z01\shell\ZipperExtract"
  DeleteRegKey HKCU "Software\Classes\SystemFileAssociations\.001\shell\ZipperExtract"

  ; 제어판 '앱 및 기능' 등록
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${EXE},0"
  WriteRegStr HKCU "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize" $0
  ; 사내 업데이트 위치 (빌드할 때 CHAGOK_UPDATE_URL 로 정한 경우)
!ifdef UPDATE_URL
  WriteRegStr HKCU "Software\Sinsul\Chagok" "UpdateUrl" "${UPDATE_URL}"
!endif
SectionEnd

Section "탐색기 우클릭 메뉴 등록" SecMenu
  StrCpy $0 0
  ${If} $IsWin11 == 1
    SetOutPath "$INSTDIR"
    File "..\build\ChagokShell.msix"
    File "/oname=Chagok.cer" "cert\chagok_sign.cer"
    ${If} $Update == 1
      Call CertTrusted   ; 자동 업데이트 중에는 관리자 확인(UAC)을 띄우지 않는다: 이미 신뢰된 경우에만 첫 화면 메뉴
    ${Else}
      Call InstallCert
    ${EndIf}
    ${If} $0 == 1
      DetailPrint "Windows 11 메뉴 패키지 등록 중…"
      Call RegisterPackage
    ${EndIf}
  ${EndIf}
  ${If} $0 == 1
    ; 새 메뉴는 '추가 옵션 표시' 안에도 함께 나오므로 예전 방식 메뉴는 지워 중복을 막는다
    ExecWait '"$INSTDIR\${EXE}" /unregister'
    DetailPrint "Windows 11 우클릭 첫 화면 메뉴 등록 완료"
    MessageBox MB_YESNO|MB_ICONQUESTION "우클릭 첫 화면에 '${APPNAME}'이 바로 보이도록 Windows 탐색기를 다시 시작할까요?$\r$\n(열려 있는 탐색기 창이 닫히고 작업 표시줄이 잠깐 사라졌다 돌아옵니다)" /SD IDNO IDNO skipRestart
      nsExec::Exec `"$SYSDIR\taskkill.exe" /f /im explorer.exe`
      Pop $1
      Exec "$WINDIR\explorer.exe"
    skipRestart:
  ${Else}
    ExecWait '"$INSTDIR\${EXE}" /register'
  ${EndIf}
SectionEnd

Function .onInstSuccess
  ${If} $Update == 1
    Exec '"$INSTDIR\${EXE}" /updated'
  ${EndIf}
FunctionEnd

Section /o "바탕 화면 바로 가기" SecDesk
  CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0
SectionEnd

Section "Uninstall"
  Call un.CloseRunning
  Call un.FindPowerShell
  nsExec::ExecToLog `"$PS" -NoProfile -NonInteractive -Command "Get-AppxPackage -Name ${PKGNAME} | Remove-AppxPackage"`
  Pop $0
  ReadRegDWORD $0 HKCU "${UNINSTKEY}" "CertAdded"
  ${If} $0 == 1
    ExecShellWait "runas" "$SYSDIR\certutil.exe" `-delstore TrustedPeople ${CERT_THUMB}` SW_HIDE
  ${EndIf}
  Delete "$INSTDIR\ChagokShell.msix"
  Delete "$INSTDIR\Chagok.cer"
  IfFileExists "$INSTDIR\${EXE}" 0 +2
    ExecWait '"$INSTDIR\${EXE}" /unregister'
  IfFileExists "$INSTDIR\${EXE}" 0 +2
    ExecWait '"$INSTDIR\${EXE}" /unregister-assoc'
  DeleteRegKey HKCU "Software\Classes\Chagok.zip"
  DeleteRegKey HKCU "Software\Classes\Chagok.archive"
  DeleteRegKey HKCU "Software\Classes\Applications\Chagok.exe"
  DeleteRegKey HKCU "Software\Sinsul\ChagokApp"
  DeleteRegValue HKCU "Software\RegisteredApplications" "Chagok"
  DeleteRegValue HKCU "Software\Classes\.zip\OpenWithProgids" "Chagok.zip"
  DeleteRegValue HKCU "Software\Classes\.z01\OpenWithProgids" "Chagok.zip"
  DeleteRegValue HKCU "Software\Classes\.001\OpenWithProgids" "Chagok.zip"
  ; 실행 파일이 없더라도 메뉴 키는 직접 지운다
  DeleteRegKey HKCU "Software\Classes\AllFilesystemObjects\shellex\ContextMenuHandlers\Chagok"
  DeleteRegKey HKCU "Software\Classes\CLSID\{8D0C9DD2-90A8-492A-AEE8-45AF3C6D4EFD}"
  DeleteRegKey HKCU "Software\Classes\*\shell\ChagokCompress"
  DeleteRegKey HKCU "Software\Classes\Directory\shell\ChagokCompress"
  DeleteRegKey HKCU "Software\Classes\SystemFileAssociations\.zip\shell\ChagokExtract"
  DeleteRegKey HKCU "Software\Classes\SystemFileAssociations\.z01\shell\ChagokExtract"
  DeleteRegKey HKCU "Software\Classes\SystemFileAssociations\.001\shell\ChagokExtract"
  DeleteRegKey HKCU "Software\Sinsul\Chagok"
  DeleteRegKey /ifempty HKCU "Software\Sinsul"
  Delete "$SMPROGRAMS\${APPNAME}.lnk"
  Delete "$DESKTOP\${APPNAME}.lnk"
  Delete "$INSTDIR\${EXE}"
  Call un.MoveLockedDll
  Delete "$INSTDIR\7z.dll"
  Delete "$INSTDIR\ChagokSFX.bin"
  Delete "$INSTDIR\7-Zip_License.txt"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\miniz_LICENSE.txt"
  Delete "$INSTDIR\zlib-ng_LICENSE.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${UNINSTKEY}"
SectionEnd

cmake_minimum_required(VERSION 3.16)
project(Chagok C CXX RC)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # 정적 CRT: 런타임 설치 불필요

add_executable(Chagok WIN32 src/main.cpp src/zipcore.cpp src/zipcrypt.cpp src/szarc.cpp src/zeng.cpp src/miniz.c res/app.rc)  # MSVC: zeng 은 miniz 로 (zlib-ng 는 build_mingw.sh)
target_compile_definitions(Chagok PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
  MINIZ_NO_ARCHIVE_APIS MINIZ_NO_TIME MINIZ_NO_STDIO MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_link_libraries(Chagok PRIVATE gdiplus dwmapi shlwapi ole32 oleaut32 uuid comdlg32 shell32 bcrypt)
if(MSVC)
  target_compile_options(Chagok PRIVATE /utf-8 /W3 $<$<CONFIG:Release>:/O1 /GL /Gy>)
  target_link_options(Chagok PRIVATE /MANIFEST:NO $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /c65001")
else()
  target_compile_options(Chagok PRIVATE -Os -fno-exceptions -fno-rtti)
  target_link_options(Chagok PRIVATE -static -s -municode)
endif()

# 탐색기 우클릭 메뉴 확장
add_library(ChagokShell SHARED src/shellext.cpp src/shellext.def res/shell.rc)
target_compile_definitions(ChagokShell PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(ChagokShell PRIVATE shlwapi shell32 ole32 uuid gdi32 user32)
if(MSVC)
  target_compile_options(ChagokShell PRIVATE /utf-8 $<$<CONFIG:Release>:/O1>)
else()
  set_target_properties(ChagokShell PROPERTIES PREFIX "")
  target_link_options(ChagokShell PRIVATE -static -s)
endif()

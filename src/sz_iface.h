// sz_iface.h - 7-Zip(7z.dll) 플러그인 인터페이스 최소 정의
// 7-Zip 소스(IArchive.h, IStream.h, IProgress.h, IPassword.h, PropID.h)의 공개 COM 레이아웃과 같다.
#pragma once
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <cstdint>

typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef int32_t Int32;
typedef int64_t Int64;

// {23170F69-40C1-278A-0000-00gg00ii0000}
#define SZ_IID(name, g, i) static const GUID name = {0x23170F69, 0x40C1, 0x278A, {0, 0, 0, g, 0, i, 0, 0}}
SZ_IID(IID_IProgress_, 0x00, 0x05);
SZ_IID(IID_ISequentialInStream_, 0x03, 0x01);
SZ_IID(IID_ISequentialOutStream_, 0x03, 0x02);
SZ_IID(IID_IInStream_, 0x03, 0x03);
SZ_IID(IID_IOutStream_, 0x03, 0x04);
SZ_IID(IID_IStreamGetSize_, 0x03, 0x06);
SZ_IID(IID_ICryptoGetTextPassword_, 0x05, 0x10);
SZ_IID(IID_ICryptoGetTextPassword2_, 0x05, 0x11);
SZ_IID(IID_ISetProperties_, 0x06, 0x03);
SZ_IID(IID_IArchiveOpenCallback_, 0x06, 0x10);
SZ_IID(IID_IArchiveExtractCallback_, 0x06, 0x20);
SZ_IID(IID_IArchiveOpenVolumeCallback_, 0x06, 0x30);
SZ_IID(IID_IInArchive_, 0x06, 0x60);
SZ_IID(IID_IArchiveUpdateCallback_, 0x06, 0x80);
SZ_IID(IID_IOutArchive_, 0x06, 0xA0);

struct ISequentialInStream_ : IUnknown {
    STDMETHOD(Read)(void* data, UInt32 size, UInt32* processed) = 0;
};
struct ISequentialOutStream_ : IUnknown {
    STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processed) = 0;
};
struct IInStream_ : ISequentialInStream_ {
    STDMETHOD(Seek)(Int64 offset, UInt32 origin, UInt64* newPos) = 0;
};
struct IOutStream_ : ISequentialOutStream_ {
    STDMETHOD(Seek)(Int64 offset, UInt32 origin, UInt64* newPos) = 0;
    STDMETHOD(SetSize)(UInt64 newSize) = 0;
};
struct IStreamGetSize_ : IUnknown {
    STDMETHOD(GetSize)(UInt64* size) = 0;
};
struct IProgress_ : IUnknown {
    STDMETHOD(SetTotal)(UInt64 total) = 0;
    STDMETHOD(SetCompleted)(const UInt64* completeValue) = 0;
};
struct ICryptoGetTextPassword_ : IUnknown {
    STDMETHOD(CryptoGetTextPassword)(BSTR* password) = 0;
};
struct ICryptoGetTextPassword2_ : IUnknown {
    STDMETHOD(CryptoGetTextPassword2)(Int32* passwordIsDefined, BSTR* password) = 0;
};
struct ISetProperties_ : IUnknown {
    STDMETHOD(SetProperties)(const wchar_t* const* names, const PROPVARIANT* values, UInt32 numProps) = 0;
};
struct IArchiveOpenCallback_ : IUnknown {
    STDMETHOD(SetTotal)(const UInt64* files, const UInt64* bytes) = 0;
    STDMETHOD(SetCompleted)(const UInt64* files, const UInt64* bytes) = 0;
};
struct IArchiveExtractCallback_ : IProgress_ {
    STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream_** outStream, Int32 askExtractMode) = 0;
    STDMETHOD(PrepareOperation)(Int32 askExtractMode) = 0;
    STDMETHOD(SetOperationResult)(Int32 opRes) = 0;
};
struct IInArchive_ : IUnknown {
    STDMETHOD(Open)(IInStream_* stream, const UInt64* maxCheckStartPosition, IArchiveOpenCallback_* cb) = 0;
    STDMETHOD(Close)() = 0;
    STDMETHOD(GetNumberOfItems)(UInt32* numItems) = 0;
    STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT* value) = 0;
    STDMETHOD(Extract)(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback_* cb) = 0;
    STDMETHOD(GetArchiveProperty)(PROPID propID, PROPVARIANT* value) = 0;
    STDMETHOD(GetNumberOfProperties)(UInt32* numProps) = 0;
    STDMETHOD(GetPropertyInfo)(UInt32 index, BSTR* name, PROPID* propID, VARTYPE* varType) = 0;
    STDMETHOD(GetNumberOfArchiveProperties)(UInt32* numProps) = 0;
    STDMETHOD(GetArchivePropertyInfo)(UInt32 index, BSTR* name, PROPID* propID, VARTYPE* varType) = 0;
};
struct IArchiveUpdateCallback_ : IProgress_ {
    STDMETHOD(GetUpdateItemInfo)(UInt32 index, Int32* newData, Int32* newProps, UInt32* indexInArchive) = 0;
    STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT* value) = 0;
    STDMETHOD(GetStream)(UInt32 index, ISequentialInStream_** inStream) = 0;
    STDMETHOD(SetOperationResult)(Int32 operationResult) = 0;
};
struct IOutArchive_ : IUnknown {
    STDMETHOD(UpdateItems)(ISequentialOutStream_* outStream, UInt32 numItems, IArchiveUpdateCallback_* cb) = 0;
    STDMETHOD(GetFileTimeType)(UInt32* type) = 0;
};

// 항목 속성 번호 (PropID.h)
enum {
    kpidPath = 3, kpidName = 4, kpidIsDir = 6, kpidSize = 7, kpidPackSize = 8, kpidAttrib = 9,
    kpidCTime = 10, kpidATime = 11, kpidMTime = 12, kpidEncrypted = 15, kpidCRC = 19, kpidIsAnti = 21,
    kpidMethod = 22, kpidIsAltStream = 63, kpidPhySize = 44
};
// 형식 핸들러 속성 (GetHandlerProperty2)
enum { kHName = 0, kHClassID = 1, kHExtension = 2, kHAddExtension = 3, kHUpdate = 4, kHFlags = 11 };
// 압축 해제 모드 / 결과
enum { kAskExtract = 0, kAskTest = 1, kAskSkip = 2 };
enum { kOpOK = 0, kOpUnsupportedMethod = 1, kOpDataError = 2, kOpCRCError = 3, kOpUnavailable = 4,
       kOpUnexpectedEnd = 5, kOpDataAfterEnd = 6, kOpIsNotArc = 7, kOpHeadersError = 8, kOpWrongPassword = 9 };

typedef HRESULT(WINAPI* Func_CreateObject)(const GUID* clsid, const GUID* iid, void** outObject);
typedef HRESULT(WINAPI* Func_GetNumberOfFormats)(UInt32* numFormats);
typedef HRESULT(WINAPI* Func_GetHandlerProperty2)(UInt32 formatIndex, PROPID propID, PROPVARIANT* value);

// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

#include <codecvt>
#include <string>
#define SAORIPROXY_DLLEXPORT
#include "proxy.h"
#include "base64.h"

using namespace std;

#ifndef SAORIPROXY_DYNAMIC_SAORI

#define var auto
#define BUFSIZE 4096
std::wstring SaoriDir;
std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
HANDLE pstdin_r, pstdin_w, pstdout_r, pstdout_w;

BOOL SAORIPROXY_LIBRARY_MODE load(HGLOBAL h, long len) {
	SaoriDir = converter.from_bytes(base64_encode((const unsigned char*)h, len));
	GlobalFree(h);
	return true;
}

BOOL SAORIPROXY_LIBRARY_MODE unload() {
	return true;
}

#endif

#define Error(msg) L"SAORI/1.0 200 OK\r\nResult: "##msg##"\r\nValue0: "##msg##"\r\nCharset: UTF-8\r\n\r\n"
#define ErrorExit(msg) { return Error(msg); }

wstring proxy(const string& req) {
    var idx = req.find("Argument0: ");
    if (idx == string::npos)
        ErrorExit("More parameters required");
    var st = idx + 11;
    idx = req.find("\r\n", st);
    var len = (idx == string::npos ? req.length() : idx) - st;
    var file = converter.from_bytes(string(req.c_str() + st, len));



    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&pstdout_r, &pstdout_w, &saAttr, 0))
        ErrorExit("Redirect stdout failed");
    if (!SetHandleInformation(pstdout_r, HANDLE_FLAG_INHERIT, 0))
        ErrorExit("Set stdout handle information failed");
    if (!CreatePipe(&pstdin_r, &pstdin_w, &saAttr, 0))
        ErrorExit("Redirect stdin failed");
    if (!SetHandleInformation(pstdin_w, HANDLE_FLAG_INHERIT, 0))
        ErrorExit("Set stdin handle information failed");

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = pstdout_w;
    si.hStdOutput = pstdout_w;
    si.hStdInput = pstdin_r;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    var pargs = L" " + converter.from_bytes(base64_encode((const unsigned char*)req.c_str(), req.length()));
    var ok = CreateProcessW(file.c_str(),
        const_cast<LPWSTR>(pargs.c_str()),   // command line 
        NULL,           // process security attributes 
        NULL,           // primary thread security attributes 
        TRUE,           // handles are inherited 
        0,              // creation flags 
        NULL,           // use parent's environment 
        NULL,           // use parent's current directory 
        &si,            // STARTUPINFO pointer 
        &pi);           // receives PROCESS_INFORMATION 

    wstring result;
    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        char buf[BUFSIZE];
        DWORD len;
        string ret;
        while (ReadFile(pstdout_r, buf, BUFSIZE, &len, NULL) && len != 0) {
            ret += string(buf, len);
            if (len < BUFSIZE)
                break;
        }

        result = converter.from_bytes(base64_decode(ret.c_str(), ret.length()));
    }
    else result = Error("Create process failed");
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(pstdout_w);
    CloseHandle(pstdin_r);
    return result;
}

HGLOBAL SAORIPROXY_LIBRARY_MODE request(HGLOBAL h, long* len) {
    string req((const char*)h, *len);
    GlobalFree(h);
    wstring result = proxy(req);
    var rep = converter.to_bytes(result);
    h = GlobalAlloc(GMEM_FIXED, rep.length());
    memcpy_s(h, rep.length(), rep.c_str(), result.length());
    *len = (long)rep.length();
    return h;
}

#ifndef __UKAGAKA_SAORI_PROXY_H__
#define __UKAGAKA_SAORI_PROXY_H__

#pragma once

#ifdef SAORIPROXY_DLLEXPORT
#define SAORIPROXY_LIB __declspec(dllexport)
#else
#define SAORIPROXY_LIB __declspec(dllimport)
#endif

// SAORI总是使用cdecl
#ifdef SAORIPROXY_LIBRARY_MODE_STDCALL
#define SAORIPROXY_LIBRARY_MODE __stdcall
#else
#define SAORIPROXY_LIBRARY_MODE __cdecl
#endif

#ifndef SAORIPROXY_AS_DLL

#ifndef SAORIPROXY_DYNAMIC_SAORI
EXTERN_C SAORIPROXY_LIB BOOL	SAORIPROXY_LIBRARY_MODE load(HGLOBAL h, long len);
EXTERN_C SAORIPROXY_LIB BOOL	SAORIPROXY_LIBRARY_MODE unload();
#endif
EXTERN_C SAORIPROXY_LIB HGLOBAL	SAORIPROXY_LIBRARY_MODE request(HGLOBAL h, long* len);

#else

#ifndef SAORIPROXY_DYNAMIC_SAORI
typedef BOOL	(SAORIPROXY_LIBRARY_MODE* _saoriproxy_saori_load	FAR)(HGLOBAL, long);
typedef BOOL	(SAORIPROXY_LIBRARY_MODE* _saoriproxy_saori_unload	FAR)();
#endif
typedef HGLOBAL	(SAORIPROXY_LIBRARY_MODE* _saoriproxy_saori_request	FAR)(HGLOBAL, long*);

static HINSTANCE _SaoriProxyInstance;
#ifndef SAORIPROXY_DYNAMIC_SAORI
static _saoriproxy_saori_load		saoriproxy_saori_load;
static _saoriproxy_saori_unload		saoriproxy_saori_unload;
#endif
static _saoriproxy_saori_request	saoriproxy_saori_request;
static void SaoriProxyDllFree() {
	if (_SaoriProxyInstance == NULL)
		return;
#ifndef SAORIPROXY_DYNAMIC_SAORI
	saoriproxy_saori_load = NULL;
	saoriproxy_saori_unload = NULL;
#endif
	saoriproxy_saori_request = NULL;
	FreeLibrary(_SaoriProxyInstance);
	_SaoriProxyInstance = NULL;
}
static bool SaoriProxyDllLoad(const TCHAR* path) {
	_SaoriProxyInstance = LoadLibrary(path);
	if (_SaoriProxyInstance == NULL)
		return false;
#ifndef SAORIPROXY_DYNAMIC_SAORI
	saoriproxy_saori_load    = (_saoriproxy_saori_load   )GetProcAddress(_SaoriProxyInstance, "load");
	saoriproxy_saori_unload  = (_saoriproxy_saori_unload )GetProcAddress(_SaoriProxyInstance, "unload");
#endif
	saoriproxy_saori_request = (_saoriproxy_saori_request)GetProcAddress(_SaoriProxyInstance, "request");

	if (saoriproxy_saori_request == NULL
#ifndef SAORIPROXY_DYNAMIC_SAORI
		|| saoriproxy_saori_load == NULL
		|| saoriproxy_saori_unload == NULL
#endif
		) {
		SaoriProxyDllFree();
		return false;
	}
	return true;
}

#endif

#define nameof(x) #x

#endif

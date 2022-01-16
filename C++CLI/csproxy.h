#ifndef __CSPROXY_H__
#define __CSPROXY_H__

#pragma once

#ifdef CSPROXY_DLLEXPORT
#define CSPROXY_LIB __declspec(dllexport)
#else
#define CSPROXY_LIB __declspec(dllimport)
#endif

// SAORI总是使用cdecl
#ifdef CSPROXY_LIBRARY_MODE_STDCALL
#define CSPROXY_LIBRARY_MODE __stdcall
#else
#define CSPROXY_LIBRARY_MODE __cdecl
#endif

#ifndef CSPROXY_AS_DLL

#ifndef CSPROXY_DYNAMIC_SAORI
EXTERN_C CSPROXY_LIB BOOL		CSPROXY_LIBRARY_MODE load		(HGLOBAL h, long len);
EXTERN_C CSPROXY_LIB BOOL		CSPROXY_LIBRARY_MODE unload		();
#endif
EXTERN_C CSPROXY_LIB HGLOBAL	CSPROXY_LIBRARY_MODE request	(HGLOBAL h, long* len);

#else

#ifndef CSPROXY_DYNAMIC_SAORI
typedef BOOL	(CSPROXY_LIBRARY_MODE* _csproxy_saori_load		FAR )(HGLOBAL, long);
typedef BOOL	(CSPROXY_LIBRARY_MODE* _csproxy_saori_unload	FAR )();
#endif
typedef HGLOBAL	(CSPROXY_LIBRARY_MODE* _csproxy_saori_request	FAR )(HGLOBAL, long*);

static HINSTANCE _CSSaoriProxyInstance;
#ifndef CSPROXY_DYNAMIC_SAORI
static _csproxy_saori_load		csproxy_saori_load;
static _csproxy_saori_unload	csproxy_saori_unload;
#endif
static _csproxy_saori_request	csproxy_saori_request;
static void CSSaoriProxyDllFree() {
	if (_CSSaoriProxyInstance == NULL)
		return;
#ifndef CSPROXY_DYNAMIC_SAORI
	csproxy_saori_load = NULL;
	csproxy_saori_unload = NULL;
#endif
	csproxy_saori_request = NULL;
	FreeLibrary(_CSSaoriProxyInstance);
	_CSSaoriProxyInstance = NULL;
}
static bool CSSaoriProxyDllLoad(const TCHAR* path) {
	_CSSaoriProxyInstance = LoadLibrary(path);
	if (_CSSaoriProxyInstance == NULL)
		return false;
#ifndef CSPROXY_DYNAMIC_SAORI
	csproxy_saori_load		= (_csproxy_saori_load		)GetProcAddress(_CSSaoriProxyInstance, "load");
	csproxy_saori_unload	= (_csproxy_saori_unload	)GetProcAddress(_CSSaoriProxyInstance, "unload");
#endif
	csproxy_saori_request	= (_csproxy_saori_request	)GetProcAddress(_CSSaoriProxyInstance, "request");

	if (csproxy_saori_request == NULL
#ifndef CSPROXY_DYNAMIC_SAORI
		|| csproxy_saori_load == NULL
		|| csproxy_saori_unload == NULL
#endif
		) {
		CSSaoriProxyDllFree();
		return false;
	}
	return true;
}

#endif

#define nameof(x) #x

#endif
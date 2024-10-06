 /*
 *  aescrypt.cpp
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This module contains the main entry points for the DLL that are
 *      called by the Windows shell (Explorer) and by the aescrypt32.exe
 *      program via the published functions defined herein.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "aescrypt.h"
#include "worker_threads.h"
#include "file_list.h"

// Defines the ATL-based module used by the shell extension
class AESCryptModule : public ATL::CAtlDllModuleT<AESCryptModule>
{
    public:
        DECLARE_LIBID(LIBID_AESCryptLib)
        DECLARE_REGISTRY_APPID_RESOURCEID(
                                    IDR_AESCRYPT,
                                    "{BACE464C-A450-46A7-BC98-F441BCE45CE9}")
};

// Create an ATL module instance
AESCryptModule AES_Crypt_Module;

// Worker Threads class to perform encrytion/decryption in the background
WorkerThreads Worker_Threads;

#ifdef _MANAGED
#pragma managed(push, off)
#endif

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance,
                               DWORD dwReason,
                               LPVOID lpReserved)
{
    return AES_Crypt_Module.DllMain(dwReason, lpReserved);
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

// Used to determine whether the DLL can be unloaded by OLE
__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow()
{
    // Ensure worker threads are not running
    if (Worker_Threads.IsBusy()) return S_FALSE;

    return AES_Crypt_Module.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
_Check_return_
STDAPI  DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
    return AES_Crypt_Module.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer()
{
    ATL::CRegKey reg;
    LSTATUS result;

    result = reg.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell "
                      L"Extensions\\Approved",
                      KEY_SET_VALUE);

    if (result!= ERROR_SUCCESS) return E_ACCESSDENIED;

    result = reg.SetStringValue(L"{35872D53-3BD4-45FA-8DB5-FFC47D4235E7}",
                                L"aescrypt");

    if (result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(result);

    return AES_Crypt_Module.DllRegisterServer(FALSE);
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer()
{
    ATL::CRegKey reg;
    LSTATUS result;

    result = reg.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell "
                      L"Extensions\\Approved",
                      KEY_SET_VALUE);

    if (result == ERROR_SUCCESS)
    {
        reg.DeleteValue(L"{35872D53-3BD4-45FA-8DB5-FFC47D4235E7}");
    }

    HRESULT hr = AES_Crypt_Module.DllUnregisterServer(FALSE);

    return hr;
}

// Exported function that allows aescrypt32.exe use this library to encrypt or
// decrypt a list of files
__declspec(dllexport) void __cdecl ProcessFiles(FileList &file_list,
                                                bool encrypt)
{
   Worker_Threads.ProcessFiles(file_list, encrypt);
}

// Exported function that allows aescrypt32.exe determine if all active
// encryption / decryption threads have completed work
__declspec(dllexport) bool __cdecl AESLibraryBusy()
{
   return Worker_Threads.IsBusy();
}

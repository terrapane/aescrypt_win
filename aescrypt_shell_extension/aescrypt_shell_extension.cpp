 /*
 *  aescrypt_shell_extension.cpp
 *
 *  Copyright (C) 2006-2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This module contains the main entry points for the DLL that are called
 *      by the Windows shell (Explorer) and by the aescrypt_launcher.exe
 *      program via the published functions defined herein.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <atlbase.h>
#include <string>
#include "resource.h"
#include "aescrypt_shell_extension.h"

namespace
{

// Registry key used for the context menu
const wchar_t *RegistryKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\"
                             L"Shell Extensions\\Approved";

/*
 *  GetModulePath()
 *
 *  Description:
 *      Get the path to the directory holding the module identified by
 *      the module handle provided.
 *
 *  Parameters:
 *      module [in]
 *          Module handle for which to get the path.
 *
 *  Returns:
 *      A string containing the path to the module or an empty string if there
 *      was an error.
 *
 *  Comments:
 *      None.
 */
std::wstring GetModulePath(HMODULE module)
{
    std::wstring pathname(512, L'\0');

    while (true)
    {
        DWORD length = GetModuleFileName(module,
                                         pathname.data(),
                                         static_cast<DWORD>(pathname.size()));

        if (length == 0) return {};

        if (length < pathname.size() - 1)
        {
            pathname.resize(length);
            return pathname;
        }

        // Buffer was too small, so grow and retry
        pathname.resize(pathname.size() * 2);
    }
}

// Defines the ATL-based module used by the shell extension
class AESCryptModule : public ATL::CAtlDllModuleT<AESCryptModule>
{
    public:
        DECLARE_LIBID(LIBID_AESCryptShellExtensionLib)
        DECLARE_REGISTRY_APPID_RESOURCEID(
                                    IDR_AESCRYPT_SHELL_EXTENSION,
                                    "{BACE464C-A450-46A7-BC98-F441BCE45CE9}")
};

// Create an ATL module instance
AESCryptModule AES_Crypt_Module;

} // namespace

// DLL Entry Point
extern "C" BOOL WINAPI DllMain([[maybe_unused]] HINSTANCE hInstance,
                               DWORD dwReason,
                               LPVOID lpReserved)
{
    return AES_Crypt_Module.DllMain(dwReason, lpReserved);
}

// Used to determine whether the DLL can be unloaded by OLE
__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow()
{
    return AES_Crypt_Module.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
_Check_return_ STDAPI DllGetClassObject(_In_ REFCLSID rclsid,
                                        _In_ REFIID riid,
                                        _Outptr_ LPVOID FAR *ppv)
{
    return AES_Crypt_Module.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer()
{
    ATL::CRegKey reg;

    LSTATUS result = reg.Open(HKEY_LOCAL_MACHINE, RegistryKey, KEY_SET_VALUE);

    if (result!= ERROR_SUCCESS) return E_ACCESSDENIED;

    // Register the context menu as an approved shell extension
    result = reg.SetStringValue(L"{35872D53-3BD4-45FA-8DB5-FFC47D4235E7}",
                                L"AES Crypt Context Menu");

    if (result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(result);

    return AES_Crypt_Module.DllRegisterServer(FALSE);
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer()
{
    ATL::CRegKey reg;

    LSTATUS result = reg.Open(HKEY_LOCAL_MACHINE, RegistryKey, KEY_SET_VALUE);

    if (result == ERROR_SUCCESS)
    {
        // Remove the entry for the AES Crypt context menu from the registry
        reg.DeleteValue(L"{35872D53-3BD4-45FA-8DB5-FFC47D4235E7}");
    }

    const HRESULT hr = AES_Crypt_Module.DllUnregisterServer(FALSE);

    return hr;
}

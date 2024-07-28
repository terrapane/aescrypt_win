/*
 *  aescrypt_shell_extension.h
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This defines the C++ class for integrating with the Windows shell.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#pragma once

#include <Windows.h>
#include <shlobj.h>
#include "resource.h"
#include "file_list.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

// AESCryptShellExtension class declaration
class ATL_NO_VTABLE AESCryptShellExtension :
        public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
        public ATL::CComCoClass<AESCryptShellExtension, &CLSID_AESCryptShellExtensionCom>,
        public IShellExtInit,
        public IContextMenu
{
    public:
        AESCryptShellExtension();
        ~AESCryptShellExtension();

        // IShellExtInit
        STDMETHOD(Initialize)(LPCITEMIDLIST, LPDATAOBJECT, HKEY);

        // IContextMenu
#ifdef _M_X64
        STDMETHOD(GetCommandString)(UINT_PTR, UINT, UINT*, LPSTR, UINT);
#else
        STDMETHOD(GetCommandString)(UINT, UINT, UINT*, LPSTR, UINT);
#endif
        STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO);
        STDMETHOD(QueryContextMenu)(HMENU, UINT, UINT, UINT, UINT);

        DECLARE_REGISTRY_RESOURCEID(IDR_AESCRYPTSHELLEXTENSION)
        DECLARE_NOT_AGGREGATABLE(AESCryptShellExtension)

        BEGIN_COM_MAP(AESCryptShellExtension)
            COM_INTERFACE_ENTRY(IShellExtInit)
            COM_INTERFACE_ENTRY_IID(IID_IContextMenu, IContextMenu)
        END_COM_MAP()

        DECLARE_PROTECT_FINAL_CONSTRUCT()

        HRESULT FinalConstruct() { return S_OK; }

        void FinalRelease() {}

    protected:
        HBITMAP context_bitmap;
        bool aes_files;
        bool non_aes_files;
        FileList file_list;
};

OBJECT_ENTRY_AUTO(__uuidof(AESCryptShellExtensionCom), AESCryptShellExtension)

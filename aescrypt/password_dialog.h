/*
 *  password_dialog.h
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines a simple dialog box class for prompting the user for
 *      a password.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#pragma once

#include <Windows.h>
#include <atlhost.h>
#include <string>
#include <terra/secutil/secure_string.h>
#include "resource.h"
#include "globals.h"

class PasswdDialog : public ATL::CAxDialogImpl<PasswdDialog>
{
    public:
        PasswdDialog(const std::wstring &window_title);
        ~PasswdDialog();

        enum { IDD = IDD_PASSWDDIALOG };

        BEGIN_MSG_MAP(PasswdDialog)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
            COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
            CHAIN_MSG_MAP(CAxDialogImpl<PasswdDialog>)
            COMMAND_HANDLER(IDC_SHOWPASSWORD,
                            STN_CLICKED,
                            OnClickedShowPassword)
        END_MSG_MAP()

        LRESULT OnInitDialog(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL &bHandled);

        LRESULT OnClickedOK(WORD wNotifyCode,
                            WORD wID,
                            HWND hWndCtl,
                            BOOL &bHandled);

        LRESULT OnClickedCancel(WORD wNotifyCode,
                                WORD wID,
                                HWND hWndCtl,
                                BOOL &bHandled);

        LRESULT OnClickedShowPassword(WORD wNotifyCode,
                                      WORD wID,
                                      HWND hWndCtl,
                                      BOOL &bHandled);

        Terra::SecUtil::SecureWString GetPassword();

    protected:
        void DeterminePasswordCharacter();
        void SetSunkenWindowStyle(HWND control_handle, bool sunken);
        void ShowEyeIcon(HICON eye);

        std::wstring window_title;
        wchar_t password_char;
        bool encrypting;
        bool show_password;
        HICON hIconLock;
        HICON hIconEyeVisible;
        HICON hIconEyeHidden;
        Terra::SecUtil::SecureWString password;
};

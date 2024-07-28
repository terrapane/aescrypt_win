/*
 *  progress_dialog.h
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines a simple progress box class for showing the progress
 *      of file encryption and decryption.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#pragma once

#include <Windows.h>
#include <functional>
#include <atlhost.h>
#include "resource.h"

class ProgressDialog : public ATL::CAxDialogImpl<ProgressDialog>
{
    public:
        ProgressDialog(std::function<void()> notify_cancel = {});
        ~ProgressDialog();

        enum { IDD = IDD_PROGRESSDIALOG };

        BEGIN_MSG_MAP(ProgressDialog)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
            CHAIN_MSG_MAP(CAxDialogImpl<ProgressDialog>)
        END_MSG_MAP()

        LRESULT OnInitDialog(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL &bHandled);

        LRESULT OnClickedCancel(WORD wNotifyCode,
                                WORD wID,
                                HWND hWndCtl,
                                BOOL &bHandled);

        bool WasCancelPressed();

    protected:
        bool cancel_pressed;
        HICON hIcon;
        std::function<void()> notify_cancel;
};

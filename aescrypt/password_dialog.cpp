/*
 *  password_dialog.cpp
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements a simple dialog box class for prompting the user
 *      for a password.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include "password_dialog.h"
#include "secure_containers.h"
#include "globals.h"

/*
 *  PasswdDialog::PasswdDialog()
 *
 *  Description:
 *      Constructor for the PasswdDialog object.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
PasswdDialog::PasswdDialog(const std::wstring &window_title) :
    ATL::CAxDialogImpl<PasswdDialog>(),
    window_title{window_title},
    encrypting{false},
    hIcon{}
{
    // Load the icon to show on the system menu
    hIcon =
        static_cast<HICON>(LoadImage(ATL::_AtlBaseModule.GetResourceInstance(),
                                     MAKEINTRESOURCEW(IDI_LOCK),
                                     IMAGE_ICON,
                                     0,
                                     0,
                                     LR_DEFAULTCOLOR | LR_DEFAULTSIZE));
}

/*
 *  PasswdDialog::~PasswdDialog()
 *
 *  Description:
 *      Destructor for the PasswdDialog object.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
PasswdDialog::~PasswdDialog()
{
    // Delete the icon object
    if (hIcon != NULL) DestroyIcon(hIcon);
}

/*
 *  PasswdDialog::OnInitDialog()
 *
 *  Description:
 *      Called when the dialog box is initialized.
 *
 *  Parameters:
 *      uMsg [in]
 *          The associated Windows message. This is usually WM_INITDIALOG,
 *          but really insignificant.
 *
 *      wParam [in]
 *          Word parameter, but not used by this function.
 *
 *      lParam [in]
 *          This parameters will hold a non-zero value if encrypting files and
 *          zero value if not encrypting files.  This dictates how the cotrols
 *          will be rendered (in particular, password verification controls).
 *
 *      bHandled [out]
 *          This is set to true if this function handles the message.
 *
 *  Returns:
 *      A non-zero return value indicates the system should set control focus,
 *      while a 0 indicates the programmer set control focus.
 *
 *  Comments:
 *      None.
 */
LRESULT PasswdDialog::OnInitDialog(UINT uMsg,
                                   WPARAM wParam,
                                   LPARAM lParam,
                                   BOOL &bHandled)
{
    HWND window_handle;

    CAxDialogImpl<PasswdDialog>::OnInitDialog(uMsg, wParam, lParam, bHandled);
    bHandled = TRUE;

    // Are we encrypting?
    encrypting = (lParam != 0) ? true : false;

    // If the lock icon is available, show it
    if (hIcon != NULL) SetIcon(hIcon);

    // Position the dialog
    CenterWindow(GetForegroundWindow());

    // Limit the length of text in the password boxes to
    // Max_Passwd_Length characters
    SendDlgItemMessage(IDC_PASSWD, EM_SETLIMITTEXT, Max_Password_Length, 0);

    if (encrypting)
    {
        SendDlgItemMessage(IDC_PASSWDCONFIRM,
                           EM_SETLIMITTEXT,
                           Max_Password_Length,
                           0);
    }
    else
    {
        // Hide the password confirmation controls
        window_handle = GetDlgItem(IDC_PASSWDCONFIRM);
        ::ShowWindow(window_handle, SW_HIDE);
        window_handle = GetDlgItem(IDC_ENTERPASSWDCONFIRM);
        ::ShowWindow(window_handle, SW_HIDE);
    }

    return 1;
}

/*
 *  PasswdDialog::OnClickedOK()
 *
 *  Description:
 *      Actions to take when the user presses OK on the dialog.
 *
 *  Parameters:
 *      wNotifyCode [in]
 *          The notification code.
 *
 *      wID [in]
 *          The identifier of the menu item, control, or accelerator.  Here,
 *          is it is the dialog identifier.
 *
 *      hWndCtl [in]
 *          A handle to a window control.
 *
 *      bHandled [out]
 *          Set to true if this message is handled and false if not.
 *
 *  Returns:
 *      Zero indicates success, non-zero indicates failure.
 *
 *  Comments:
 *      None.
 */
LRESULT PasswdDialog::OnClickedOK(WORD wNotifyCode,
                                  WORD wID,
                                  HWND hWndCtl,
                                  BOOL &bHandled)
{
    bHandled = TRUE;

    // Reserve space for the password (+1 for null terminator)
    password.resize(Max_Password_Length + 1, L'\0');

    // Retrieve the password from the dialog
    GetDlgItemText(IDC_PASSWD, password.data(), Max_Password_Length + 1);

    // Determine the actual text length
    password.resize(wcslen(password.data()));

    // If we are encrypting files, check to make sure that the
    // password entered into the conformation field matches.
    // If decrypting, the confirmation is not necessary.
    if (encrypting)
    {
        SecureWString password_confirm(Max_Password_Length + 1, L'\0');

        // Retrieve the password confirmation from the dialog
        GetDlgItemText(IDC_PASSWDCONFIRM,
                       password_confirm.data(),
                       Max_Password_Length);

        // Determine the actual text length
        password_confirm.resize(wcslen(password_confirm.data()));

        // Check to see if the passwords match
        if (password != password_confirm)
        {
            MessageBox(L"Password confirmation check failed.\nVerify that the "
                       L"passwords match.",
                       window_title.c_str(),
                       MB_OK);
            return 0;
        }

        // Ensure we got a password
        if (password.empty())
        {
            MessageBox(L"You failed to enter a password.",
                       window_title.c_str(),
                       MB_OK);
            return 0;
        }

        // Close the dialog box
        EndDialog(wID);

        return 0;
    }

    // Ensure we got a password for decrypting
    if (password.empty())
    {
        MessageBox(L"You failed to enter a password.",
                   window_title.c_str(),
                   MB_OK);
        return 0;
    }

    // Close the dialog box
    EndDialog(wID);

    return 0;
}

/*
 *  PasswdDialog::OnClickedCancel()
 *
 *  Description:
 *      Actions to take when the user presses cancel or closes the dialog.
 *
 *  Parameters:
 *      wNotifyCode [in]
 *          The notification code.
 *
 *      wID [in]
 *          The identifier of the menu item, control, or accelerator.  Here,
 *          is it is the dialog identifier.
 *
 *      hWndCtl [in]
 *          A handle to a window control.
 *
 *      bHandled [out]
 *          Set to true if this message is handled and false if not.
 *
 *  Returns:
 *      Zero indicates success, non-zero indicates failure.
 *
 *  Comments:
 *      None.
 */
LRESULT PasswdDialog::OnClickedCancel(WORD wNotifyCode,
                                      WORD wID,
                                      HWND hWndCtl,
                                      BOOL &bHandled)
{
    bHandled = TRUE;
    EndDialog(wID);
    return 0;
}

/*
 *  PasswdDialog::GetPassword()
 *
 *  Description:
 *      Return the password provided by the user via the dialog box.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      The password entered by the user.
 *
 *  Comments:
 *      None.
 */
Terra::SecUtil::SecureWString PasswdDialog::GetPassword()
{
    return password;
}

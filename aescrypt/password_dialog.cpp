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

#include <algorithm>
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
 *      window_title [in]
 *          The title of the window used to display error messages.
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
    password_char{'*'},
    encrypting{false},
    show_password{false},
    hFont{NULL},
    hIconLock{NULL},
    hIconEyeVisible{NULL},
    hIconEyeHidden{NULL}
{
    // Load the icon to show on the system menu
    hIconLock =
        static_cast<HICON>(LoadImage(ATL::_AtlBaseModule.GetResourceInstance(),
                                     MAKEINTRESOURCEW(IDI_AESCRYPT_LOCK),
                                     IMAGE_ICON,
                                     0,
                                     0,
                                     LR_DEFAULTCOLOR | LR_DEFAULTSIZE));


    // Load icons that are 16x16 or larger to facilitate scaling
    int cxIcon = std::max(GetSystemMetrics(SM_CXSMICON), 16);
    int cyIcon = std::max(GetSystemMetrics(SM_CYSMICON), 16);

    // Load the "visible" eye icon to use when the user wishes to show passwords
    hIconEyeVisible =
        static_cast<HICON>(LoadImage(ATL::_AtlBaseModule.GetResourceInstance(),
                                     MAKEINTRESOURCEW(IDI_EYE_VISIBLE),
                                     IMAGE_ICON,
                                     cxIcon,
                                     cyIcon,
                                     LR_DEFAULTCOLOR));

    // Load the "hidden" eye icon to use when the user wishes to hide passwords
    hIconEyeHidden =
        static_cast<HICON>(LoadImage(ATL::_AtlBaseModule.GetResourceInstance(),
                                     MAKEINTRESOURCEW(IDI_EYE_HIDDEN),
                                     IMAGE_ICON,
                                     cxIcon,
                                     cyIcon,
                                     LR_DEFAULTCOLOR));
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
    // Delete the various handles
    if (hFont != NULL) DeleteObject(hFont);
    if (hIconLock != NULL) DestroyIcon(hIconLock);
    if (hIconEyeVisible != NULL) DestroyIcon(hIconEyeVisible);
    if (hIconEyeHidden != NULL) DestroyIcon(hIconEyeHidden);
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
    CAxDialogImpl<PasswdDialog>::OnInitDialog(uMsg, wParam, lParam, bHandled);

    // Are we encrypting?
    encrypting = (lParam != 0) ? true : false;

    // If the lock icon is available, show it
    if (hIconLock != NULL) SetIcon(hIconLock);

    // Set the icon on the button
    ShowEyeIcon(hIconEyeVisible);

    // Position the dialog
    CenterWindow(NULL);

    // Determine the default password character
    DeterminePasswordCharacter();

    // Attempt to select Consolas, 8pt for password controls
    HDC hDC = ::GetDC(NULL);
    if (hDC)
    {
        hFont = CreateFont(
            -MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72),
            0, 0, 0,                        // Width, escapement, orientation
            FW_NORMAL,                      // Weight
            FALSE, FALSE, FALSE,            // Italic, underline, strikeout
            DEFAULT_CHARSET,                // Character set
            OUT_TT_PRECIS,                  // Output precision
            CLIP_DEFAULT_PRECIS,            // Clipping precision
            CLEARTYPE_QUALITY,              // Quality
            DEFAULT_PITCH | FF_MODERN,      // Pitch and family
            L"Consolas");                   // Font name
        ::ReleaseDC(NULL, hDC);
    }

    // Get handles for the password and password confirm edit controls
    HWND password_handle = GetDlgItem(IDC_PASSWD);
    HWND password_confirm_handle = GetDlgItem(IDC_PASSWDCONFIRM);

    // Attempt to apply the selected font to edit controls
    if ((hFont != NULL) && (password_handle != NULL) &&
        (password_confirm_handle != NULL))
    {
        SendMessage(password_handle, WM_SETFONT, (WPARAM) hFont, TRUE);
        SendMessage(password_confirm_handle, WM_SETFONT, (WPARAM) hFont, TRUE);
    }

    // If not encrypting, hide the password confirmation controls
    if (!encrypting && (password_confirm_handle != NULL))
    {
        // Hide the password confirmation controls (edit window and label)
        ::ShowWindow(password_confirm_handle, SW_HIDE);
        HWND text_handle = GetDlgItem(IDC_ENTERPASSWDCONFIRM);
        if (text_handle != NULL) ::ShowWindow(text_handle, SW_HIDE);
    }

    // Indicate message was handled
    bHandled = TRUE;

    // Returning 1 sets focus the first control with WS_TABSTOP set
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
LRESULT PasswdDialog::OnClickedOK([[maybe_unused]] WORD wNotifyCode,
                                  WORD wID,
                                  [[maybe_unused]] HWND hWndCtl,
                                  BOOL &bHandled)
{
    // Determine the length of the input
    int password_length = static_cast<int>(
        std::max(SendDlgItemMessage(IDC_PASSWD, WM_GETTEXTLENGTH, 0, 0),
                 LRESULT(0)));

    // Reserve space for the password (+1 for null terminator)
    password.resize(password_length + 1, L'\0');

    // Retrieve the password from the dialog
    GetDlgItemText(IDC_PASSWD, password.data(), password_length + 1);

    // Determine the actual text length
    password.resize(wcslen(password.data()));

    // Ensure we got a password for encrypting or decrypting
    if (password.empty())
    {
        MessageBox(L"A password was not entered.",
                   window_title.c_str(),
                   MB_OK | MB_ICONWARNING);
        return 0;
    }

    // If encrypting files, check to make sure that the password entered into
    // the conformation field matches
    if (encrypting)
    {
        int password_confirm_length = static_cast<int>(std::max(
            SendDlgItemMessage(IDC_PASSWDCONFIRM, WM_GETTEXTLENGTH, 0, 0),
            LRESULT(0)));

        SecureWString password_confirm(password_confirm_length + 1, L'\0');

        // Retrieve the password confirmation from the dialog
        GetDlgItemText(IDC_PASSWDCONFIRM,
                       password_confirm.data(),
                       password_confirm_length + 1);

        // Determine the actual text length
        password_confirm.resize(wcslen(password_confirm.data()));

        // Ensure we got a password confirmation
        if (password_confirm.empty())
        {
            MessageBox(L"A password confirmation was not entered.",
                       window_title.c_str(),
                       MB_OK | MB_ICONWARNING);
            return 0;
        }

        // Check to see if the passwords match
        if (password != password_confirm)
        {
            MessageBox(L"Password confirmation check failed.\nVerify that the "
                       L"passwords match.",
                       window_title.c_str(),
                       MB_OK | MB_ICONWARNING);
            return 0;
        }
    }

    // Close the window
    EndDialog(wID);

    // Indicate that the message was handled
    bHandled = TRUE;

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
LRESULT PasswdDialog::OnClickedCancel([[maybe_unused]] WORD wNotifyCode,
                                      WORD wID,
                                      [[maybe_unused]] HWND hWndCtl,
                                      BOOL &bHandled)
{
    // Close the window
    EndDialog(wID);

    // Indicate that the message was handled
    bHandled = TRUE;

    return 0;
}

/*
 *  PasswdDialog::OnClickedShowPassword()
 *
 *  Description:
 *      Actions to take when the user presses the button to reveal password.
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
LRESULT PasswdDialog::OnClickedShowPassword([[maybe_unused]] WORD wNotifyCode,
                                            [[maybe_unused]] WORD wID,
                                            HWND hWndCtl,
                                            BOOL &bHandled)
{
    // Only respond to clicks
    if (wNotifyCode != STN_CLICKED) return 0;

    // Toggle the password state
    show_password = !show_password;

    // Get edit controls
    HWND hPasswd = GetDlgItem(IDC_PASSWD);
    HWND hPasswdConfirm = GetDlgItem(IDC_PASSWDCONFIRM);

    // Toggle password visibility
    if (hPasswd != NULL && hPasswdConfirm != NULL)
    {
        wchar_t mask = show_password ? 0 : password_char;
        ::SendMessage(hPasswd, EM_SETPASSWORDCHAR, (WPARAM) mask, 0);
        ::SendMessage(hPasswdConfirm, EM_SETPASSWORDCHAR, (WPARAM) mask, 0);

        // Force edit controls to redraw
        ::InvalidateRect(hPasswd, NULL, TRUE);
        ::InvalidateRect(hPasswdConfirm, NULL, TRUE);

        // Set the icon on the button
        ShowEyeIcon(show_password ? hIconEyeHidden : hIconEyeVisible);

        // Make the window control appear sunken if showing the password
        SetSunkenWindowStyle(hWndCtl, show_password);
    }

    // Indicate that the message was handled
    bHandled = TRUE;

    return 0;
}

/*
 *  PasswdDialog::GetPasswordCharacter()
 *
 *  Description:
 *      Determine what character the system uses for hiding passwords.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      The character to use for hiding passwords.
 *
 *  Comments:
 *      None.
 */
void PasswdDialog::DeterminePasswordCharacter()
{
    HWND hPasswd = GetDlgItem(IDC_PASSWD);

    // Unable to get the window handle, so give up
    if (!hPasswd) return;

    // Ask the password input control what the password character is
    password_char =
        static_cast<wchar_t>(::SendMessage(hPasswd, EM_GETPASSWORDCHAR, 0, 0));

    // Fallback to '*' if query fails
    if (password_char == 0) password_char = '*';
}

/*
 *  PasswdDialog::SetSunkenWindowStyle()
 *
 *  Description:
 *      This function will set the WS_SUNKEN window style (or remove it) to
 *      lend to the illusion of a button press.
 *
 *  Parameters:
 *      control_handle [in]
 *          The handle to the window control to modify.
 *
 *      sunken [in]
 *          True if it should appear sunken, false if not.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void PasswdDialog::SetSunkenWindowStyle(HWND control_handle, bool sunken)
{
    // Do nothing if the handle is NULL
    if (control_handle == NULL) return;

    // Get the current window style
    LONG_PTR style = ::GetWindowLongPtr(control_handle, GWL_EXSTYLE);

    // Apply the SS_SUNKEN stype as requested
    if (sunken)
    {
        style |= WS_EX_CLIENTEDGE;
    }
    else
    {
        style &= ~WS_EX_CLIENTEDGE;
    }
    ::SetWindowLongPtr(control_handle, GWL_EXSTYLE, style);

    // Redraw the control to reflect the style change

    // Force style change to take effect
    ::SetWindowPos(control_handle,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED |
                       SWP_NOACTIVATE);

    ::InvalidateRect(control_handle, nullptr, TRUE);
    ::UpdateWindow(control_handle);
}

/*
 *  PasswdDialog::ShowEyeIcon()
 *
 *  Description:
 *      This function will render the selected eye icon when the user toggles
 *      between showing and hiding the password text.
 *
 *  Parameters:
 *      icon [in]
 *          The handle to the icon that should be rendered in the control.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void PasswdDialog::ShowEyeIcon(HICON icon)
{
    // Do nothing if the icon handle is invalid
    if (icon == NULL) return;

    // Set the icon on the button
    HWND hShowPasswordButton = GetDlgItem(IDC_SHOWPASSWORD);

    // Just return if we cannot get the control handle
    if (hShowPasswordButton == NULL) return;

    // Apply the desired icon
    ::SendMessage(hShowPasswordButton,
                  STM_SETIMAGE,
                  (WPARAM) IMAGE_ICON,
                  (LPARAM) icon);
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

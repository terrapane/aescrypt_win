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
#include <uxtheme.h>
#include <vssym32.h>
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
    hIconEyeHidden{NULL},
    cxIcon{},
    cyIcon{}
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
    cxIcon = std::max(GetSystemMetrics(SM_CXSMICON), 16);
    cyIcon = std::max(GetSystemMetrics(SM_CYSMICON), 16);

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
        SendMessage(password_handle,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(hFont),
                    TRUE);
        SendMessage(password_confirm_handle,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(hFont),
                    TRUE);
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
 *  PasswdDialog::OnDrawItem()
 *
 *  Description:
 *      This function is called to draw draw elements of the dialog window,
 *      but the only one that must be handled is the owner-drawn "eye" button.
 *
 *  Parameters:
 *      uMsg [in]
 *          The associated Windows message, which should be WM_DRAWITEM but
 *          this can be safely ignored.
 *
 *      wParam [in]
 *          Word parameter, but not used by this function, which indicates
            what needs to be drawn.
 *
 *      lParam [in]
 *          This parameters will hold the LPDRAWITEMSTRUCT of what needs to be
 *          drawn.
 *
 *      bHandled [out]
 *          This is set to true if this function handles the message.
 *
 *  Returns:
 *      Return FALSE if not handled, TRUE if it is.
 *
 *  Comments:
 *      None.
 */
LRESULT PasswdDialog::OnDrawItem([[maybe_unused]] UINT uMsg,
                                 WPARAM wParam,
                                 LPARAM lParam,
                                 BOOL &bHandled)
{
    // This function will only draw the button to reveal the password
    if (wParam != IDC_SHOWPASSWORD)
    {
        bHandled = FALSE;
        return FALSE;
    }

    LPDRAWITEMSTRUCT pDIS = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
    HDC hDC = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    // Fill background with dialog's COLOR_3DFACE
    HBRUSH hBrush = reinterpret_cast<HBRUSH>(GetSysColorBrush(COLOR_3DFACE));
    FillRect(hDC, &rc, hBrush);

    // Area where to draw the icon
    RECT rectIcon = rc;

    // Draw themed button border (rounded corners)
    HTHEME hTheme = OpenThemeData(pDIS->hwndItem, L"Button");
    if (hTheme != NULL)
    {
        // Get content rectangle (excludes border)
        RECT rect{};
        GetThemeBackgroundContentRect(hTheme,
                                      hDC,
                                      BP_PUSHBUTTON,
                                      PBS_NORMAL,
                                      &rc,
                                      &rect);

        // Reassign the icon drawing area
        rectIcon = rect;

        // Clip out the content area to preserve COLOR_3DFACE background
        ExcludeClipRect(hDC, rect.left, rect.top, rect.right, rect.bottom);

        // Draw themed border based on state
        int iStateId = show_password ? PBS_PRESSED : PBS_NORMAL;
        if (pDIS->itemState & ODS_DISABLED)
        {
            iStateId = PBS_DISABLED;
        }
        else if (pDIS->itemState & ODS_HOTLIGHT)
        {
            iStateId = PBS_HOT;
        }
        DrawThemeBackground(hTheme, hDC, BP_PUSHBUTTON, iStateId, &rc, NULL);

        // Restore full clipping region for icon and focus rectangle
        SelectClipRgn(hDC, NULL);
        CloseThemeData(hTheme);
    }
    else
    {
        // Draw rectangular edge if themes are disabled
        if (show_password)
        {
            DrawEdge(hDC, &rc, EDGE_SUNKEN, BF_RECT);
        }
        else
        {
            DrawEdge(hDC, &rc, EDGE_ETCHED, BF_FLAT);
        }
    }

    // Draw the icon centered
    HICON hIcon = show_password ? hIconEyeHidden : hIconEyeVisible;
    if (hIcon)
    {
        int x = rectIcon.left + (rectIcon.right - rectIcon.left - cxIcon) / 2;
        int y = rectIcon.top + (rectIcon.bottom - rectIcon.top - cyIcon) / 2;
        UINT di_flags = (pDIS->itemState & ODS_DISABLED) ? DI_IMAGE : DI_NORMAL;
        DrawIconEx(hDC, x, y, hIcon, cxIcon, cyIcon, 0, NULL, di_flags);
    }

    // Draw focus rectangle if focused
    if (pDIS->itemState & ODS_FOCUS)
    {
        RECT rcFocus = rc;
        InflateRect(&rcFocus, -3, -3);  // Inset to fit inside themed border
        DrawFocusRect(hDC, &rcFocus);
    }

    // Indicate the message was handled
    bHandled = TRUE;

    return TRUE;
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
    if (wNotifyCode != BN_CLICKED) return 0;

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

        // Cause the eye control to redraw
        ::InvalidateRect(hWndCtl, NULL, TRUE);
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

/*
 *  progress_dialog.cpp
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements a simple progress box class for showing the progress
 *      of file encryption and decryption.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <VersionHelpers.h>
#include "progress_dialog.h"

/*
 *  ProgressDialog::ProgressDialog()
 *
 *  Description:
 *      Constructor for the ProgressDialog object.
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
ProgressDialog::ProgressDialog(std::function<void()> notify_cancel) :
    ATL::CAxDialogImpl<ProgressDialog>(),
    cancel_pressed{false},
    hIcon{},
    notify_cancel{notify_cancel}
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
 *  ProgressDialog::~ProgressDialog()
 *
 *  Description:
 *      Destructor for the ProgressDialog object.
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
ProgressDialog::~ProgressDialog()
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
 *          zero value if not encrypting files.  This dictates what text is
 *          rendered while the program is working.
 *
 *      bHandled [out]
 *          This is set to true if this function handles the message.
 *
 *  Returns:
 *      A non-zero return value indicates the system should set control focus,
 *      while a 0 indicates the programmer set control focus.
 *
 *  Comments:
 *      The progress meter has a range of 0..100 by default and can be changed
 *      using the PBM_SETRANGE message, though this function does not change
 *      the default range.
 */
LRESULT ProgressDialog::OnInitDialog(UINT uMsg,
                                     WPARAM wParam,
                                     LPARAM lParam,
                                     BOOL &bHandled)
{
    CAxDialogImpl<ProgressDialog>::OnInitDialog(uMsg, wParam, lParam, bHandled);
    bHandled = TRUE;

    // Are we encrypting?
    bool encrypting = (lParam != 0) ? true : false;

    // If the lock icon is available, show it
    if (hIcon != NULL) SetIcon(hIcon);

    // Position the dialog
    CenterWindow(GetForegroundWindow());

    // Set the encrpypting / decrypting message
    if (encrypting)
    {
        SetDlgItemText(IDC_ENCRYPTINGMSG, L"Encrypting...");
    }
    else
    {
        SetDlgItemText(IDC_ENCRYPTINGMSG, L"Decrypting...");
    }

    // If older than Windows XP (major OS version less than 5), then set the
    // status bar color manually, else system uses its defaults
    if (!IsWindowsXPOrGreater())
    {
        // Set the progress bar color
        SendDlgItemMessage(
            IDC_PROGRESSBAR,
            PBM_SETBARCOLOR,
            0,
            static_cast<LPARAM>(static_cast<COLORREF>(RGB(0, 102, 204))));
    }

    return 1;
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
LRESULT ProgressDialog::OnClickedCancel(WORD wNotifyCode,
                                        WORD wID,
                                        HWND hWndCtl,
                                        BOOL &bHandled)
{
    bHandled = TRUE;

    // Indicate that processing was cancelled
    cancel_pressed = true;

    // Issue the notification callback if defined
    if (notify_cancel) notify_cancel();

    return 0;
}

/*
 *  ProgressDialog::WasCancelPressed()
 *
 *  Description:
 *      Returns a boolean indicating whether the user pressed cancel or
 *      requested to close the dialog box.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      True is cancelled was pressed, false if not.
 *
 *  Comments:
 *      None.
 */
bool ProgressDialog::WasCancelPressed()
{
    return cancel_pressed;
}

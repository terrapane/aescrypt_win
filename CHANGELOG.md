# Change Log

v4.3.0

- Updates to core libraries, including performance improvements to libaes

v4.2.4

- To avoid confusion, any partial output file will be removed before presenting
  the user with a dialog window indicating there was an error
- Improved robustness of file closure to catch errors flushing the
  output buffer
- Updated CLI version to 4.2.4
- Updated all library dependencies

v4.2.3

- Updated to the latest command-line version
- All binaries are now properly labeled with the version numbers, company name,
  and digitally signed

v4.2.2

- Fixed a bug where a directory name could be specified instead of a file name
  in the command-line version of AES Crypt
- Updated all library dependencies

v4.2.0

- No significant changes in GUI code, but there are minor changes made to
  several dependencies

v4.1.0

- Updated library dependencies

v4.0.6

- Various improvements / cleanup
- Updated library dependencies

v4.0.5

- Removed use of constexpr for global strings used to hold the version number

v4.0.4

- Updated library dependencies
- Updated the AES Crypt CLI version to the latest
- Improved progress window responsiveness
- Various minor code improvements

v4.0.3

- Update CLI version to 4.0.2, which addressed key file handling issues

v4.0.2

- Increased the limit of passwords to Windows' default input text
  size of 32767 characters

v4.0.1

- Fix for legacy key file handling

v4.0.0

- Initial release of AES Crypt v4

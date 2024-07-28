# AES Crypt for Windows

This project contains the AES Crypt code for Windows, which includes shell
extension code and the command-line interface (CLI) executable.

## Build Instructions

The C++ language components for Visual Studio need to be installed, including
the Active Template Library (ATL) components.

The AES Crypt solution file has dependencies on several libraries that must
first be built and "installed" before attempting to build the solution file.

### Option 1 - Build from the command-line

Open a command prompt having the Visual Studio x64 environment configured.
For example, open via "x64 Native Tools Command Prompt for VS..." shortcut
or run Visual Studio's `vcvars64.bat` batch file from a command prompt.

From within the AES Crypt for Windows root source directory, type
`build_dependencies` to configure, build, and locally (i.e., within the
source directory) the required dependencies.

Building the rest of AES Crypt will require compiling using Visual Studio
as per Option 2 by opening the solution file as per step 7 below and
following the balance of the instructions.

### Option 2 - Build from within Visual Studio

1. Open Visual Studio and choose File -> Open -> CMake.
2. Navigate to the AES Crypt for Windows source directory and open the
   CMakeLists.txt file. Give Visual Studio time to open the CMake file,
   download all of the dependencies, and generate build files.  When finished,
   the output pane on the IDE should say "Cmake generation finished."  Ensure
   there were no errors before proceeding.
3. Select "x64 Release" as the build configuration in the dropdown list at
   the top of the IDE window.  This may result in the build configuration
   being re-generated; wait for it to complete successfully.
4. Open the CMakeLists.txt file in the editor.  At the top of Visual Studio,
   change the "Select Startup Item" to "Current Document", and it should then
   indicate that in the menu.  Be sure the Project Configuration next to
   that is "Release".
5. Choose Build -> Build All menu option to build dependencies.
6. Choose Build -> Install aescrypt_dependencies.  This will install the
   required header files and libraries in the project directory in a
   sub-directory named out\install.  These will be consumed by the main
   AES Crypt for Windows solution.
7. Double-click on the "aescrypt.sln" file to cause Visual Studio to open
   the AES Crypt for Windows solution, as we're done with the dependencies.
8. Select Release and x64 from the build configuration settings at the
   top of the Visual Studio IDE.
9. Select Build->Build Solution.  This will build the AES Crypt for Windows
   with an output aescrypt.msi in the Setup\Release folder.

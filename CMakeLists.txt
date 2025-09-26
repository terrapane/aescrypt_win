cmake_minimum_required (VERSION 3.21)

# Enable Hot Reload for MSVC compilers if supported
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Define the dependencies project
project(aescrypt_win
        VERSION 4.4.0.0
        DESCRIPTION  "AES Crypt for Windows Dependencies"
        HOMEPAGE_URL "https://www.aescrypt.com"
        LANGUAGES CXX)

set(COPYRIGHT_TEXT "Copyright (C) 2025")
set(AUTHOR_TEXT    "Terrapane Corporation")

# Produce output files with version information for consumption by the GUI
configure_file(version.h.in version.h @ONLY)
configure_file(version.rc.in version.rc @ONLY)

# Due to change Microsoft introduced, the MSVC runtime libraries can cause
# a crash when using std::mutex unless the user has the latest C++ libraries
# from Microsoft.  The following side-steps this problem. See:
# https://developercommunity.visualstudio.com/t/Access-violation-in-_Thrd_yield-after-up/10664660#T-N10668856
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

# Include dependencies
add_subdirectory("dependencies")

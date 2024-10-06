cmake_minimum_required (VERSION 3.21)

# Enable Hot Reload for MSVC compilers if supported.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Define the dependencies project
project(aescrypt_dependencies
        VERSION 4.0.6.0
        DESCRIPTION  "AES Crypt for Windows Dependencies"
        HOMEPAGE_URL "https://www.aescrypt.com")

# Include dependencies
add_subdirectory("dependencies")

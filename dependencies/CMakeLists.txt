# Enable fetching content
include(FetchContent)

# Set the root of the Terrapane source repositories
if(DEFINED ENV{TERRAPANE_SOURCE_URI})
    set(TERRAPANE_SOURCE_URI "$ENV{TERRAPANE_SOURCE_URI}")
else()
    set(TERRAPANE_SOURCE_URI "https://github.com/terrapane")
endif()

# Fetch the AES Crypt CLI repository
FetchContent_Declare(aescrypt_cli
    GIT_REPOSITORY ${TERRAPANE_SOURCE_URI}/aescrypt_cli.git
    GIT_TAG        v4.7.0
    GIT_SHALLOW    false)

# Install AES Crypt CLI dependencies to get libraries
set(aescrypt_cli_DEPENDENCIES_INSTALL ON)

# Enable unit tests for AES Crypt CLI and all dependencies
set(aescrypt_cli_BUILD_TESTS ON)
set(stf_BUILD_TESTS ON)
set(aescrypt_engine_BUILD_TESTS ON)
set(aescrypt_lm_BUILD_TESTS ON)
set(bitutil_BUILD_TESTS ON)
set(charutil_BUILD_TESTS ON)
set(conio_BUILD_TESTS ON)
set(libaes_BUILD_TESTS ON)
set(libhash_BUILD_TESTS ON)
set(libjson_BUILD_TESTS ON)
set(libkdf_BUILD_TESTS ON)
set(logger_BUILD_TESTS ON)
set(program_options_BUILD_TESTS ON)
set(random_BUILD_TESTS ON)
set(secutil_BUILD_TESTS ON)

# Make dependencies available
FetchContent_MakeAvailable(aescrypt_cli)

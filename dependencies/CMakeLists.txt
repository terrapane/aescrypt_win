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
    GIT_TAG        v4.0.7
    GIT_SHALLOW    true)

# Install AES Crypt CLI dependencies to get libraries
set(aescrypt_cli_DEPENDENCIES_INSTALL ON)

# Make dependencies available
FetchContent_MakeAvailable(aescrypt_cli)

# Enable fetching content
include(FetchContent)

# Set the root of the Terrapane source repositories
if(DEFINED ENV{TERRAPANE_SOURCE_URI})
    set(TERRAPANE_SOURCE_URI "$ENV{TERRAPANE_SOURCE_URI}")
else()
    set(TERRAPANE_SOURCE_URI "https://github.com/terrapane")
endif()

# Fetch the AES Crypt Engine
FetchContent_Declare(aescrypt_engine
    GIT_REPOSITORY ${TERRAPANE_SOURCE_URI}/aescrypt_engine.git
    GIT_TAG        v4.0.1
    GIT_SHALLOW    true)

# Fetch the AES Crypt CLI repository
FetchContent_Declare(aescrypt_cli
    GIT_REPOSITORY ${TERRAPANE_SOURCE_URI}/aescrypt_cli.git
    GIT_TAG        v4.0.3
    GIT_SHALLOW    true)

# Make dependencies available
FetchContent_MakeAvailable(aescrypt_engine aescrypt_cli)

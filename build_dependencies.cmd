@echo off
@rem
@rem build_dependencies
@rem
@rem This script will build the AES Crypt for Windows dependencies.  This must
@rem be from from within the source directory and with the Visual Studio
@rem environment properly configured.
@rem

if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set PLATFORM=arm64
) else if /i "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set PLATFORM=x64
) else (
    echo ERROR: Unsupported architecture "%ARCH%"
    exit /b 1
)

@rem Configure CMake
cmake.exe -S . --preset %PLATFORM%-Release || (echo cmake configuration failed && exit /b 1)

@rem Build the source code
cmake --build out/build/%PLATFORM%-Release --parallel || (echo cmake build failed && exit /b 1)

@rem Run unit tests
ctest --test-dir out/build/%PLATFORM%-Release --parallel || (echo Testing failed && exit /b 1)

@rem Install the dependencies locally
cmake --install out/build/%PLATFORM%-Release || (echo cmake install failed && exit /b 1)

echo Dependency build successful

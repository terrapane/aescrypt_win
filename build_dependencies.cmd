@echo off
@rem
@rem build_dependencies
@rem
@rem This script will build the AES Crypt for Windows dependencies.  This must
@rem be from from within the source directory and with the Visual Studio x64
@rem environment properly configured.
@rem

@rem Configure CMake
cmake.exe -S . --preset x64-Release || ^
    (echo cmake configuration failed && exit /b 1)

@rem Build the source code
cmake --build out/build/x64-Release --parallel || (echo cmake build failed && exit /b 1)

@rem Install the dependencies locally
cmake --install out/build/x64-Release || (echo cmake install failed && exit /b 1)

echo Build and installation successful

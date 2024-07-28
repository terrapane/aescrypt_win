@echo off
@rem
@rem build_dependencies
@rem
@rem This script will build the AES Crypt for Windows dependencies.  This must
@rem be from from within the source directory and with the Visual Studio x64
@rem environment properly configured.
@rem

@rem Configure CMake
cmake.exe -S . -B out/build -G Ninja -DCMAKE_C_COMPILER:STRING=cl.exe -DCMAKE_CXX_COMPILER:STRING=cl.exe -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=out/install/x64-release -DCMAKE_MAKE_PROGRAM=ninja.exe || (echo cmake configuration failed && exit /b 1)

@rem Build the source code
cmake --build out/build --parallel || (echo cmake build failed && exit /b 1)

@rem Install the dependencies locally
cmake --install out/build || (echo cmake install failed && exit /b 1)

echo Build and installation successful

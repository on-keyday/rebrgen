# PowerShell equivalent of build.bat
# Set variables
$BUILD_TYPE = "Debug"
$BUILD_MODE = "native"
$INSTALL_PREFIX = "."
$env:FUTILS_DIR = "C:/workspace/utils_backup"
$EMSDK_PATH = "C:\\workspace\\emsdk\\emsdk_env.ps1"

# Configure and build for native
cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" -D CMAKE_BUILD_TYPE="$BUILD_TYPE" -S . -B "./built/$BUILD_MODE/$BUILD_TYPE"
cmake --build "./built/$BUILD_MODE/$BUILD_TYPE"
cmake --install "./built/$BUILD_MODE/$BUILD_TYPE" --component Unspecified
cmake --install ./built/native/Debug --component gen_template
return

# Configure and build for web (using emsdk)
$BUILD_MODE = "web"
& $EMSDK_PATH # Source the emsdk environment script
emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX/web" -S . -B ./built/$BUILD_MODE/$BUILD_TYPE
cmake --build ./built/$BUILD_MODE/$BUILD_TYPE
cmake --install ./built/$BUILD_MODE/$BUILD_TYPE --component Unspecified

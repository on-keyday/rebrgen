# PowerShell equivalent of gen_template.bat
param (
    [string]$LANG_NAME
)

if (-not $LANG_NAME) {
    Write-Host "Usage: $PSScriptRoot\$MyInvocation.MyCommand.Name LANG_NAME"
    exit 1
}

$env:FUTILS_DIR = "C:/workspace/utils_backup"


cmake --build ./built/native/Debug --target gen_template
cmake --install ./built/native/Debug --component gen_template
Write-Host "Generating template for $LANG_NAME"
$DEBUG_OPTION = ""

& "./tool/gen_template" --lang $LANG_NAME --header --hook-dir "src/bm2$LANG_NAME/hook" $DEBUG_OPTION | Out-File "src/bm2$LANG_NAME/bm2$LANG_NAME.hpp" -Encoding utf8
& "./tool/gen_template" --config-file "src/bm2$LANG_NAME/config.json" --hook-dir "src/bm2$LANG_NAME/hook" $DEBUG_OPTION | Out-File "src/bm2$LANG_NAME/bm2$LANG_NAME.cpp" -Encoding utf8
& "./tool/gen_template" --lang $LANG_NAME --main --hook-dir "src/bm2$LANG_NAME/hook" $DEBUG_OPTION | Out-File "src/bm2$LANG_NAME/main.cpp" -Encoding utf8
& "./tool/gen_template" --lang $LANG_NAME --cmake --hook-dir "src/bm2$LANG_NAME/hook" $DEBUG_OPTION | Out-File "src/bm2$LANG_NAME/CMakeLists.txt" -Encoding utf8


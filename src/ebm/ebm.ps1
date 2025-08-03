# PowerShell equivalent of script/binary_module.bat
$TOOL_PATH = "C:\workspace\shbrgen\brgen\tool"
& $TOOL_PATH\src2json src/ebm/extended_binary_module.bgn | Out-File save/ebm.json -Encoding utf8
& $TOOL_PATH\json2cpp2 -f save/ebm.json --mode header_file --add-visit --enum-stringer --use-error --dll-export | Out-File src/ebm/extended_binary_module.hpp -Encoding utf8
& $TOOL_PATH\json2cpp2 -f save/ebm.json --mode source_file --enum-stringer --use-error --dll-export | Out-File src/ebm/extended_binary_module.cpp -Encoding utf8
#& $TOOL_PATH\json2cpp2 -f save/ebm.json --mode header_only | Out-File save/binary_module2.hpp -Encoding utf8

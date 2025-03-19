# PowerShell equivalent of generate.bat
.\script\gen_template.ps1 c
.\script\gen_template.ps1 python
.\script\gen_template.ps1 haskell
.\script\gen_template.ps1 go
python script/collect_cmake.py
.\tool\gen_template --template-docs=markdown > docs/template_parameters.md
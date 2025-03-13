@echo off
setlocal 
call .\script\gen_template.bat c
call .\script\gen_template.bat python
call .\script\gen_template.bat haskell
call .\script\gen_template.bat go
python script/collect_cmake.py
.\tool\gen_template --template-docs=markdown > docs/template_parameters.md

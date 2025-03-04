@echo off
setlocal 
call .\script\gen_template.bat c
call .\script\gen_template.bat py
call .\script\gen_template.bat haskell
call .\script\gen_template.bat go
@echo off
copy objs\doom3.exe e:\jeux\doom2\doom3.exe
pushd
cdd e:\jeux\doom2
rem -nosound -nomusic -nocd
DOOM3  %&
popd

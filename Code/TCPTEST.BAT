@echo off
copy objs\doom3.exe e:\jeux\doom2\doom3.exe
e:
cd \jeux\doom2
ipxcopy doom3.exe
doom3 -server 2 -debugfile %1 %2 %3 %4
d:

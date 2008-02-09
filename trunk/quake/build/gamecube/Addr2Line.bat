@echo off
set PATH=C:\devkitPro\devkitPPC\bin;C:\devkitPro\msys\bin;%PATH%
set ADDRS=80049168 8004943c 8000f0e0
@echo on
powerpc-gekko-addr2line --exe ../../obj/gamecube/Quake.elf %ADDRS%
@echo off
pause

@echo off
setlocal
if "%MSMPI_INC%"=="" set "MSMPI_INC=C:\Program Files (x86)\Microsoft SDKs\MPI\Include\"
if "%MSMPI_LIB64%"=="" set "MSMPI_LIB64=C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64\"

gcc -O2 -Wall -Wextra -o berkeley.exe berkeley.c -I"%MSMPI_INC%." -L"%MSMPI_LIB64%." -lmsmpi -lm
if errorlevel 1 (
    echo ERRO na compilacao.
    exit /b 1
)
echo Compilado: berkeley.exe
echo Executar:  mpiexec -n 5 berkeley.exe
endlocal

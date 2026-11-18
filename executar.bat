@echo off
chcp 65001 >nul
set "PATH=C:\Program Files\Microsoft MPI\Bin;%PATH%"
cd /d "%~dp0"

if not exist berkeley.exe (
    echo berkeley.exe nao encontrado - compilando...
    call .\build.bat || exit /b 1
)

echo.
echo ###### DEMO 1 - Exemplo classico do livro (3 processos) ######
echo  Relogios 3:00, 3:25 e 2:50  -  todos devem ir para 3:05
echo  Comando: mpiexec -n 3 berkeley.exe -e -r 1
pause
mpiexec -n 3 berkeley.exe -e -r 1

echo.
echo ###### DEMO 2 - Quantidade qualquer de processos (5) ######
echo  Comando: mpiexec -n 5 berkeley.exe -r 2
pause
mpiexec -n 5 berkeley.exe -r 2

echo.
echo ###### DEMO 3 - Tolerancia a falhas (rank 3 adiantado 1 hora) ######
echo  Comando: mpiexec -n 6 berkeley.exe -f 3 -r 1
pause
mpiexec -n 6 berkeley.exe -f 3 -r 1

echo.
echo ###### DEMO 4 - Muitos processos (16), so o resultado ######
echo  Comando: mpiexec -n 16 berkeley.exe -r 3
pause
mpiexec -n 16 berkeley.exe -r 3 | findstr /C:"Processos" /C:"Dispersao" /C:"RESULTADO"

echo.
echo ###### DEMO 5 - Outro coordenador (rank 2) ######
echo  Comando: mpiexec -n 4 berkeley.exe -c 2 -r 1
pause
mpiexec -n 4 berkeley.exe -c 2 -r 1

echo.
echo Fim da demonstracao.

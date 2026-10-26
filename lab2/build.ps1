$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    & g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic main.cpp algorithms.cpp -o matrix_lab.exe
    if ($LASTEXITCODE -ne 0) { throw 'Ошибка сборки' }
    & .\matrix_lab.exe --test
    if ($LASTEXITCODE -ne 0) { throw 'Тесты не пройдены' }
} finally {
    Pop-Location
}

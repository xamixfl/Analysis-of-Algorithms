param([switch]$Report)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    & g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic main.cpp algorithms.cpp -o reverse_number.exe
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }
    & g++ -std=c++17 -O0 -Wall -Wextra -Wpedantic tests/test_algorithms.cpp algorithms.cpp -o tests/test_algorithms.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
    & ./tests/test_algorithms.exe
    if ($LASTEXITCODE -ne 0) { throw 'Algorithm tests failed' }
    & python tests/verify_cli.py
    if ($LASTEXITCODE -ne 0) { throw 'CLI tests failed' }
    & python tests/verify_models.py
    if ($LASTEXITCODE -ne 0) { throw 'Graph model checks failed' }
    if ($Report) {
        1..2 | ForEach-Object {
            & xelatex -interaction=nonstopmode -halt-on-error report.tex
            if ($LASTEXITCODE -ne 0) { throw 'PDF build failed' }
        }
    }
} finally {
    Pop-Location
}

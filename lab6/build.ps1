[CmdletBinding()]
param(
    [string]$Compiler = 'g++',
    [switch]$Benchmark,
    [switch]$Report
)

$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Path build, output/pdf -Force | Out-Null
    & $Compiler -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp src/algorithms.cpp -o lab6.exe
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }

    $testOutput = & python tests/verify.py
    if ($LASTEXITCODE -ne 0) { throw 'Functional tests failed' }
    $testOutput | Set-Content -LiteralPath results/tests.txt -Encoding ASCII
    $testOutput | Write-Output

    if ($Benchmark) {
        & python scripts/graphs.py
        if ($LASTEXITCODE -ne 0) { throw 'Graph generation failed' }
        & python scripts/experiment.py --compiler $Compiler
        if ($LASTEXITCODE -ne 0) { throw 'Experiment failed' }
    }

    & python tests/verify_results.py
    if ($LASTEXITCODE -ne 0) { throw 'Experiment verification failed' }

    if ($Report) {
        1..3 | ForEach-Object {
            & xelatex -interaction=nonstopmode -halt-on-error -output-directory=build report.tex
            if ($LASTEXITCODE -ne 0) { throw 'PDF build failed' }
        }
        Copy-Item -LiteralPath build/report.pdf -Destination 'output/pdf/report.pdf'
    }
} finally {
    Pop-Location
}

param(
    [switch]$Test,
    [switch]$Benchmark,
    [switch]$Report
)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build,results | Out-Null
    & gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -pthread src/main.c src/pipeline.c src/image.c -o lab5.exe
    if ($LASTEXITCODE -ne 0) { throw 'C build failed' }
    if ($Test) {
        $summary = & python tests/verify.py
        if ($LASTEXITCODE -ne 0) { throw 'Functional tests failed' }
        $summary | Set-Content -Encoding utf8 results/tests.txt
        $summary
    }
    if ($Benchmark) {
        & python scripts/benchmark.py
        if ($LASTEXITCODE -ne 0) { throw 'Benchmark failed' }
    }
    if ($Report) {
        New-Item -ItemType Directory -Force build,output/pdf | Out-Null
        1..2 | ForEach-Object {
            & xelatex -interaction=nonstopmode -halt-on-error -output-directory=build report.tex
            if ($LASTEXITCODE -ne 0) { throw 'Report build failed' }
        }
        Copy-Item -LiteralPath build/report.pdf -Destination output/pdf/report.pdf
    }
} finally {
    Pop-Location
}

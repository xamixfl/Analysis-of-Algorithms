param([switch]$Report, [switch]$Benchmark)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Force results,output/pdf | Out-Null
    & gcc -std=c11 -O0 -Wall -Wextra -Wpedantic -pthread src/main.c src/segmentation.c src/image.c -o segment.exe
    if ($LASTEXITCODE -ne 0) { throw 'C build failed' }
    & gcc -std=c11 -O0 -Wall -Wextra -Wpedantic -pthread -Isrc tests/test_api.c src/segmentation.c src/image.c -o tests/test_api.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
    $api = & ./tests/test_api.exe
    if ($LASTEXITCODE -ne 0) { throw 'API tests failed' }
    & gcc -std=c11 -O0 -Wall -Wextra -Wpedantic -pthread -Isrc tests/test_launch_failure.c -o tests/test_launch_failure.exe
    if ($LASTEXITCODE -ne 0) { throw 'Failure test build failed' }
    $failure = & ./tests/test_launch_failure.exe
    if ($LASTEXITCODE -ne 0) { throw 'Failure tests failed' }
    $cli = & python tests/verify.py
    if ($LASTEXITCODE -ne 0) { throw 'Independent tests failed' }
    @($api) + @($failure) + @($cli) | Set-Content -Encoding utf8 results/tests.txt
    if ($Benchmark) {
        & python scripts/environment.py
        & ./segment.exe bench results/raw.csv 100
        if ($LASTEXITCODE -ne 0) { throw 'Benchmark failed' }
        & python scripts/analyze.py
        if ($LASTEXITCODE -ne 0) { throw 'Analysis failed' }
        & python scripts/check_artifacts.py
        if ($LASTEXITCODE -ne 0) { throw 'Measurement verification failed' }
    }
    if ($Report) {
        1..3 | ForEach-Object {
            & xelatex -interaction=nonstopmode -halt-on-error -output-directory=output/pdf report.tex
            if ($LASTEXITCODE -ne 0) { throw 'Report build failed' }
        }
    }
} finally { Pop-Location }

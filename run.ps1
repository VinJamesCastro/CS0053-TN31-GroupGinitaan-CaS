$ErrorActionPreference = 'Stop'

$compiler = 'C:\msys64\ucrt64\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    $found = Get-Command g++ -ErrorAction SilentlyContinue
    if (-not $found) {
        throw 'MSYS2 UCRT64 g++ was not found. Install it or add g++ to PATH.'
    }
    $compiler = $found.Source
}

$sources = @(
    'main.cpp',
    'MenuController.cpp',
    'TransactionEngine.cpp',
    'FileStorage.cpp',
    'InputValidator.cpp',
    'ReportGenerator.cpp',
    'Authentication.cpp'
)
$program = Join-Path $PSScriptRoot 'cas_app.exe'

Push-Location $PSScriptRoot
try {
    & $compiler -std=c++17 -Wall -Wextra @sources -lbcrypt -o $program
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    & $program
    if ($LASTEXITCODE -ne 0) {
        throw "Program exited with code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

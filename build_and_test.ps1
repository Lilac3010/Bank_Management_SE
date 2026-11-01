# NFR-5 build check: builds every active test suite SEPARATELY, runs each one, and compiles
# the bank.exe sources. Used locally and by the GitHub Actions workflow.
#
#   powershell -ExecutionPolicy Bypass -File build_and_test.ps1              (Windows)
#   pwsh ./build_and_test.ps1 -Compiler gcc                                   (Linux / macOS)
#
# -Compiler auto (default): MSVC on Windows when it is installed, otherwise g++.
# MSVC is found with vswhere, so a normal PowerShell window works (no Developer prompt needed).
# Suites that use storage.cpp need <filesystem>: MSVC 2019+ or g++ 8+. With an older g++ they
# are SKIPPED with a message, not failed.
# Output goes to build/<compiler>/ (ignored by git). Exit code 0 only if nothing failed.
param(
    [ValidateSet('auto', 'msvc', 'gcc')]
    [string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Continue'
Set-Location $PSScriptRoot
$onWindows = [System.Environment]::OSVersion.Platform -eq 'Win32NT'

# One entry per suite, each with its own file list. Never merge two lists: every test file
# defines main() and its own fakes. FR-3 (tests_fr3.cpp) is parked; see README.
$suites = @(
    @{ Name = 'tests';      Req = 'FR-1';  NeedsFs = $false; Sources = @('tests.cpp', 'account.cpp') },
    @{ Name = 'tests_fr2';  Req = 'FR-2';  NeedsFs = $false; Sources = @('tests_fr2.cpp', 'account.cpp', 'account_number.cpp') },
    @{ Name = 'tests_fr4';  Req = 'FR-4';  NeedsFs = $false; Sources = @('tests_fr4.cpp', 'transactions.cpp') },
    @{ Name = 'tests_fr5';  Req = 'FR-5';  NeedsFs = $false; Sources = @('tests_fr5.cpp', 'balance_enquiry.cpp') },
    @{ Name = 'tests_fr6';  Req = 'FR-6';  NeedsFs = $false; Sources = @('tests_fr6.cpp', 'mini_statement.cpp') },
    @{ Name = 'tests_fr7';  Req = 'FR-7';  NeedsFs = $false; Sources = @('tests_fr7.cpp', 'account_search.cpp') },
    @{ Name = 'tests_fr8';  Req = 'FR-8';  NeedsFs = $false; Sources = @('tests_fr8.cpp', 'account_update.cpp') },
    @{ Name = 'tests_fr9';  Req = 'FR-9';  NeedsFs = $false; Sources = @('tests_fr9.cpp', 'account_closure.cpp') },
    @{ Name = 'tests_sec4'; Req = 'SEC-4'; NeedsFs = $false; Sources = @('tests_sec4.cpp', 'transactions.cpp') },
    @{ Name = 'tests_fr13'; Req = 'FR-13'; NeedsFs = $true;  Sources = @('tests_fr13.cpp', 'storage.cpp', 'account.cpp', 'account_number.cpp', 'transactions.cpp', 'balance_enquiry.cpp', 'mini_statement.cpp') },
    @{ Name = 'tests_nfr5'; Req = 'NFR-5'; NeedsFs = $true;  Sources = @('tests_nfr5.cpp', 'storage.cpp', 'account.cpp', 'account_number.cpp', 'transactions.cpp', 'balance_enquiry.cpp', 'mini_statement.cpp', 'account_search.cpp', 'account_update.cpp', 'account_closure.cpp') }
)

# bank.exe sources. auth.cpp (FR-10 / FR-11) is linked in automatically once it exists;
# until then these files are compiled but not linked.
$appSources = @('main.cpp', 'account.cpp', 'account_number.cpp', 'transactions.cpp',
                'balance_enquiry.cpp', 'mini_statement.cpp', 'storage.cpp', 'account_search.cpp',
                'account_update.cpp', 'account_closure.cpp')

# Loads the MSVC build environment (vcvars64.bat) into this PowerShell session.
function Import-MsvcEnvironment {
    if (Get-Command cl -ErrorAction SilentlyContinue) { return $true }
    if (-not $onWindows) { return $false }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $false }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { return $false }
    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { return $false }
    cmd /c "call `"$vcvars`" >nul 2>nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
    }
    return [bool](Get-Command cl -ErrorAction SilentlyContinue)
}

# ---- Pick the compiler ----
if ($Compiler -eq 'auto') {
    if ($onWindows -and (Import-MsvcEnvironment)) { $Compiler = 'msvc' } else { $Compiler = 'gcc' }
}

$filesystemOk = $true
$fsLibs = @()
if ($Compiler -eq 'msvc') {
    if (-not (Import-MsvcEnvironment)) {
        Write-Host 'ERROR: MSVC (cl) not found. Install Visual Studio 2019+ with the C++ workload.'
        exit 2
    }
    $banner = cmd /c 'cl 2>&1' | Select-Object -First 1
    $compilerInfo = "MSVC: $banner"
} else {
    if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
        Write-Host 'ERROR: g++ not found on PATH.'
        exit 2
    }
    $gccVersion = (& g++ -dumpversion).Trim()
    $gccMajor = [int]($gccVersion.Split('.')[0])
    $compilerInfo = "g++ $gccVersion"
    if ($gccMajor -lt 8) {
        $filesystemOk = $false
    } elseif ($gccMajor -eq 8) {
        $fsLibs = @('-lstdc++fs')   # g++ 8 keeps std::filesystem in a separate library
    }
}

$outDir = Join-Path 'build' $Compiler
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$fsSkipReason = "needs <filesystem>: g++ 8+ or MSVC 2019+ (this is $compilerInfo)"

Write-Host "NFR-5 build check"
Write-Host "Compiler: $compilerInfo"
Write-Host "Output:   $outDir"

# Compiles and links one program. Returns $true on success.
function Build-Program([string]$name, [string[]]$sources) {
    $exe = Join-Path $outDir "$name.exe"
    if ($Compiler -eq 'msvc') {
        $objDir = Join-Path $outDir "obj_$name"
        New-Item -ItemType Directory -Force -Path $objDir | Out-Null
        & cl /nologo /std:c++17 /EHsc /W4 @sources "/Fo$objDir\" "/Fe$exe" | Out-Host
    } else {
        & g++ -std=c++17 -Wall -Wextra -o $exe @sources @fsLibs | Out-Host
    }
    return ($LASTEXITCODE -eq 0)
}

# Runs one program from the repository root. Returns the exit code, or $null if it could not start.
function Invoke-Program([string]$name) {
    $exe = Join-Path (Join-Path $PSScriptRoot $outDir) "$name.exe"
    try {
        & $exe | Out-Host
        return $LASTEXITCODE
    } catch {
        Write-Host "Could not start ${exe}: $($_.Exception.Message)"
        return $null
    }
}

$results = @()
$anyFailed = $false

foreach ($suite in $suites) {
    Write-Host ""
    Write-Host "===== $($suite.Req): $($suite.Name) ($($suite.Sources -join ' ')) ====="
    $row = [ordered]@{ Suite = $suite.Name; Req = $suite.Req; Build = ''; Run = '' }

    if ($suite.NeedsFs -and -not $filesystemOk) {
        Write-Host "SKIPPED: $fsSkipReason"
        $row.Build = 'SKIPPED'
        $row.Run = 'SKIPPED'
    } elseif (-not (Build-Program $suite.Name $suite.Sources)) {
        $row.Build = 'FAILED'
        $row.Run = 'not run'
        $anyFailed = $true
    } else {
        $row.Build = 'ok'
        $code = Invoke-Program $suite.Name
        if ($null -eq $code) {
            $row.Run = 'NOT RUN (could not start)'
            $anyFailed = $true
        } elseif ($code -eq 0) {
            $row.Run = 'passed'
        } else {
            $row.Run = "FAILED (exit $code)"
            $anyFailed = $true
        }
    }
    $results += [pscustomobject]$row
}

# ---- bank.exe ----
Write-Host ""
Write-Host "===== bank.exe sources ====="
$appRow = [ordered]@{ Suite = 'bank.exe'; Req = 'NFR-5'; Build = ''; Run = 'not run (interactive)' }
$haveAuth = Test-Path 'auth.cpp'
if (-not $filesystemOk) {
    Write-Host "SKIPPED: storage.cpp $fsSkipReason"
    $appRow.Build = 'SKIPPED'
    $appRow.Run = 'SKIPPED'
} elseif ($haveAuth) {
    if (Build-Program 'bank' ($appSources + 'auth.cpp')) { $appRow.Build = 'linked' } else { $appRow.Build = 'FAILED'; $anyFailed = $true }
} else {
    Write-Host 'auth.cpp (FR-10 / FR-11) does not exist yet: compiling every bank.exe source, link skipped.'
    $objDir = Join-Path $outDir 'obj_bank'
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    $compiled = $true
    if ($Compiler -eq 'msvc') {
        & cl /nologo /std:c++17 /EHsc /W4 /c @appSources "/Fo$objDir\" | Out-Host
        if ($LASTEXITCODE -ne 0) { $compiled = $false }
    } else {
        foreach ($src in $appSources) {
            $obj = Join-Path $objDir ([System.IO.Path]::ChangeExtension($src, '.o'))
            & g++ -std=c++17 -Wall -Wextra -c $src -o $obj | Out-Host
            if ($LASTEXITCODE -ne 0) { $compiled = $false }
        }
    }
    if ($compiled) { $appRow.Build = 'compiled, link skipped (no auth.cpp)' } else { $appRow.Build = 'FAILED'; $anyFailed = $true }
}
$results += [pscustomobject]$appRow

Write-Host ""
Write-Host "===== Summary ($compilerInfo) ====="
$results | Format-Table -AutoSize | Out-String -Width 200 | Write-Host

if ($anyFailed) {
    Write-Host 'RESULT: FAILED'
    exit 1
}
Write-Host 'RESULT: all built suites passed'
exit 0

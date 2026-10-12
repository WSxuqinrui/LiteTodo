param(
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja',
    [string]$Toolchain = '',
    [string]$Python = 'python'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
if($Toolchain){$env:PATH=$Toolchain+';'+$env:PATH}
& $Python (Join-Path $PSScriptRoot 'fetch-deps.py')
if($LASTEXITCODE -ne 0){throw 'Dependency validation failed'}
$taskArgs=@('-S',$taskRoot,'-B',(Join-Path $taskRoot 'build-windows'),'-G','Ninja','-DCMAKE_BUILD_TYPE=MinSizeRel',('-DCMAKE_MAKE_PROGRAM='+$Ninja))
if($Toolchain){$taskArgs+=('-DCMAKE_C_COMPILER='+ (Join-Path $Toolchain 'gcc.exe'));$taskArgs+=('-DCMAKE_CXX_COMPILER='+ (Join-Path $Toolchain 'g++.exe'))}
& $CMake @taskArgs
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
& $CMake --build (Join-Path $taskRoot 'build-windows') --target LiteTodo store_tests reliability_tests ui_regression_tests --parallel 6
if($LASTEXITCODE -ne 0){throw 'Build failed'}
$taskCTest=Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
if(-not(Test-Path -LiteralPath $taskCTest)){$taskCTest='ctest'}
& $taskCTest --test-dir (Join-Path $taskRoot 'build-windows') --output-on-failure
if($LASTEXITCODE -ne 0){throw 'Tests failed'}
& $Python (Join-Path $taskRoot 'tests\package_tests.py')
if($LASTEXITCODE -ne 0){throw 'Package isolation regression failed'}
& $Python (Join-Path $taskRoot 'tests\instance_process_tests.py') --binary (Join-Path $taskRoot 'build-windows\LiteTodo.exe')
if($LASTEXITCODE -ne 0){throw 'Single-instance process regression failed'}
& $Python (Join-Path $PSScriptRoot 'package.py') --binary (Join-Path $taskRoot 'build-windows\LiteTodo.exe') --platform windows-x64
if($LASTEXITCODE -ne 0){throw 'Packaging/size check failed'}

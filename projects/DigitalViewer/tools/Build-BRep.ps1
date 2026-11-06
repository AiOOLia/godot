param(
    [string]$BRepSdkPath = 'E:\WorkSpace\NDS\GitLab\ndsbrepexpoter',
    [string]$GodotCppPath = 'E:\WorkSpace\Gitea\DigitalViewer\ThirdPart\godot-cpp',
    [switch]$GenerateTestFixtures
)
$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$nativePath = Join-Path $projectPath 'addons/brep/native'
$outputPath = Join-Path $projectPath 'addons/brep/bin'
$manifest = Get-Content -LiteralPath (Join-Path $nativePath 'sdk/manifest.json') -Raw | ConvertFrom-Json
foreach ($file in $manifest.files.PSObject.Properties) {
    $path = Join-Path $BRepSdkPath $file.Name
    if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $file.Value) {
        throw "BREP SDK does not match the pinned ABI: $path. Supply the matching SDK; do not mix current headers with older DLLs."
    }
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x64 tools are required.' }
$devcmd = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$environment = & $env:ComSpec /c "call `"$devcmd`" -no_logo -arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not configure MSVC environment.' }
foreach ($entry in $environment) {
    if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$ninja = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$buildPath = Join-Path $projectPath 'build/brep/native'
& cmake -S $nativePath -B $buildPath -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release "-DBREP_SDK_PATH=$BRepSdkPath" "-DGODOT_CPP_PATH=$GodotCppPath" "-DBREP_OUTPUT_PATH=$outputPath"
if ($LASTEXITCODE -ne 0) { throw 'BREP extension configuration failed.' }
& cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'BREP extension compilation failed.' }
foreach ($runtimeFile in $manifest.runtime_files.PSObject.Properties) {
    Copy-Item -LiteralPath (Join-Path $BRepSdkPath $runtimeFile.Value) -Destination (Join-Path $outputPath $runtimeFile.Name)
}
# App-local release CRT; the exported viewer must not depend on a developer PATH.
$crt = Get-ChildItem -LiteralPath (Join-Path $installation 'VC/Redist/MSVC') -Directory | Where-Object { $_.Name -match '^\d+\.' } | Sort-Object Name -Descending | Select-Object -First 1
$crtDirectory = Get-ChildItem -LiteralPath (Join-Path $crt.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crtDirectory) { throw 'MSVC release CRT not found.' }
Get-ChildItem -LiteralPath $crtDirectory.FullName -Filter '*.dll' | Copy-Item -Destination $outputPath
Copy-Item -LiteralPath (Join-Path $GodotCppPath 'LICENSE.md') -Destination (Join-Path $outputPath 'GODOT-CPP-LICENSE.md')
Copy-Item -LiteralPath (Join-Path $nativePath 'sdk/manifest.json') -Destination (Join-Path $outputPath 'sdk-manifest.json')
$runtimeHashes = [ordered]@{}
Get-ChildItem -LiteralPath $outputPath -Filter '*.dll' | ForEach-Object { $runtimeHashes[$_.Name] = (Get-FileHash -LiteralPath $_.FullName).Hash }
[ordered]@{ sdk = $manifest; runtime_sha256 = $runtimeHashes; godot_cpp_sha256 = (Get-FileHash -LiteralPath (Join-Path $GodotCppPath 'lib/godot-cpp.windows.release.64.lib')).Hash } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputPath 'runtime-manifest.json') -Encoding utf8
if ($GenerateTestFixtures) {
    & cmake --build $buildPath --target BRepFixture
    if ($LASTEXITCODE -ne 0) { throw 'BREP fixture generator compilation failed.' }
    $fixtureBinaryPath = Join-Path $buildPath 'fixture'
    Copy-Item -LiteralPath (Join-Path $outputPath 'BRepCore.dll') -Destination $fixtureBinaryPath
    $fixturePath = Join-Path $projectPath 'build/brep/fixtures'
    New-Item -ItemType Directory -Force -Path $fixturePath | Out-Null
    foreach ($mode in @('units', 'split-shell', 'split-solid', 'nurbs', 'zero-id', 'duplicate-id', 'shell-duplicate-id', 'tessellation')) {
        & (Join-Path $fixtureBinaryPath 'BRepFixture.exe') $fixturePath $mode
        if ($LASTEXITCODE -ne 0) { throw "BREP fixture generation failed: $mode" }
        [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $fixturePath "$mode.brep"))) | Set-Content -LiteralPath (Join-Path $projectPath "tests/fixtures/$mode.brep.base64.txt") -Encoding ascii
        if ($mode -eq 'tessellation') {
            [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $fixturePath "$mode.geom"))) | Set-Content -LiteralPath (Join-Path $projectPath "tests/fixtures/$mode.geom.base64.txt") -Encoding ascii
        }
    }
}
Write-Output 'DIGITALVIEWER_BREP_BUILT (existing CAD DLLs copied without recompilation)'

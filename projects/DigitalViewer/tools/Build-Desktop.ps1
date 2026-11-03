param(
    [string]$Editor = '',
    [ValidateSet('Windows Desktop', 'Windows Validation', 'Linux/X11', 'macOS')][string]$Preset = 'Windows Desktop',
    [switch]$PackOnly
)
$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repoPath = [IO.Path]::GetFullPath((Join-Path $projectPath '../..'))
if (-not $Editor) { $Editor = Join-Path $repoPath 'bin/godot.windows.editor.dev.x86_64.console.exe' }
if (-not (Test-Path -LiteralPath $Editor)) { throw "Editor not found: $Editor" }
$version = (& $Editor --version | Out-String).Trim()
if ($version -notmatch '^4\.7\.3\.rc') { throw "Expected custom Godot 4.7.3-rc, found $version" }
$outputDirectory = Join-Path $projectPath $(if ($Preset -eq 'Windows Validation') { 'build/validation' } else { 'build/desktop' })
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
# Build outputs and screenshots are not project assets.
New-Item -ItemType File -Force -Path (Join-Path $projectPath 'build/.gdignore') | Out-Null
$extension = if ($PackOnly) { 'pck' } elseif ($Preset -like 'Windows*') { 'exe' } elseif ($Preset -eq 'macOS') { 'zip' } else { 'x86_64' }
$outputPath = Join-Path $outputDirectory "DigitalViewer.$extension"
$mode = if ($PackOnly) { '--export-pack' } else { '--export-release' }
$log = & $Editor --headless --path $projectPath $mode $Preset $outputPath 2>&1
$code = $LASTEXITCODE
$log | Write-Output
if ($code -ne 0 -or ($log -join "`n") -match 'SCRIPT ERROR|Export failed|Cannot export|Failed to export') { throw "Export failed (exit $code)." }
if (-not (Test-Path -LiteralPath $outputPath)) { throw 'Export did not create an artifact.' }
if ($Preset -eq 'Windows Desktop' -and -not $PackOnly) {
    $startupOutput = Join-Path $projectPath 'build/desktop-startup.log'
    $startupErrors = Join-Path $projectPath 'build/desktop-startup-errors.log'
    $startup = Start-Process -FilePath $outputPath -ArgumentList '--headless', '--quit-after', '8' -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $startupOutput -RedirectStandardError $startupErrors
    $startupText = (Get-Content -LiteralPath $startupOutput, $startupErrors -Raw) -join "`n"
    if ($startup.ExitCode -ne 0 -or $startupText -match 'SCRIPT ERROR|ERROR:|Parse Error') { throw "Exported application startup failed: $startupText" }
}
Copy-Item -LiteralPath (Join-Path $repoPath 'LICENSE.txt') -Destination (Join-Path $outputDirectory 'GODOT-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repoPath 'COPYRIGHT.txt') -Destination (Join-Path $outputDirectory 'THIRD-PARTY-LICENSES.txt')
Copy-Item -LiteralPath (Join-Path $repoPath 'thirdparty/osgdb/src/osgDB/deprecated-dotosg/osg/LICENSE.txt') -Destination (Join-Path $outputDirectory 'OSG-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $projectPath 'docs/desktop-v1.md') -Destination (Join-Path $outputDirectory 'IMPLEMENTATION-STATUS.md')
Copy-Item -LiteralPath (Join-Path $projectPath 'docs/windows-package.md') -Destination (Join-Path $outputDirectory 'README.md')
$runtimeFiles = @('project.godot', 'export_presets.cfg', 'scenes/main.tscn', 'themes/digitalviewer_theme.tres', 'assets/icon.svg', 'scripts/app_state.gd', 'scripts/main_ui.gd')
$runtimeFiles += Get-ChildItem -LiteralPath (Join-Path $projectPath 'scripts/desktop'), (Join-Path $projectPath 'scripts/scripting') -Filter '*.gd' -File | ForEach-Object { [IO.Path]::GetRelativePath($projectPath, $_.FullName).Replace('\', '/') }
$sourceHashes = [ordered]@{}
foreach ($relative in ($runtimeFiles | Sort-Object)) {
    $sourceHashes[$relative] = (Get-FileHash -LiteralPath (Join-Path $projectPath $relative) -Algorithm SHA256).Hash
}
$manifest = [ordered]@{
    product = 'DigitalViewer'; version = '1.0.0'; status = 'development preview'; preset = $Preset
    editor = $version; artifact = [IO.Path]::GetFileName($outputPath)
    sha256 = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    pck_sha256 = if (Test-Path -LiteralPath (Join-Path $outputDirectory 'DigitalViewer.pck')) { (Get-FileHash -LiteralPath (Join-Path $outputDirectory 'DigitalViewer.pck') -Algorithm SHA256).Hash } else { $null }
    built_at_utc = [DateTime]::UtcNow.ToString('o')
    source_commit = (& git -C $repoPath rev-parse HEAD | Out-String).Trim()
    source_dirty = [bool](& git -C $repoPath status --porcelain | Out-String).Trim()
    runtime_sources_sha256 = $sourceHashes
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputDirectory 'build-manifest.json') -Encoding utf8
if ($Preset -eq 'Windows Desktop' -and -not $PackOnly) {
    $packageFiles = @('DigitalViewer.exe', 'DigitalViewer.pck', 'GODOT-LICENSE.txt', 'THIRD-PARTY-LICENSES.txt', 'OSG-LICENSE.txt', 'IMPLEMENTATION-STATUS.md', 'README.md', 'build-manifest.json') | ForEach-Object { Join-Path $outputDirectory $_ }
    Compress-Archive -LiteralPath $packageFiles -DestinationPath (Join-Path $projectPath 'build/DigitalViewer-1.0.0-windows-x86_64-preview.zip') -Force
}
Write-Output "Created $outputPath"

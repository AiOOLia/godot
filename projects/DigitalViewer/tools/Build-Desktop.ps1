param(
    [string]$Editor = '',
    [ValidateSet('Windows Desktop', 'Windows Validation', 'Linux/X11', 'macOS')][string]$Preset = 'Windows Desktop',
    [ValidateSet('ALL', 'CAD')][string]$Profile = 'ALL',
    [string]$Template = '',
    [switch]$BuildTemplate,
    [switch]$BuildBRep,
    [switch]$PackOnly
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Profile-Common.ps1')
$configuration = Get-DigitalViewerProfile $Profile
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repoPath = [IO.Path]::GetFullPath((Join-Path $projectPath '../..'))
if ($BuildBRep) { & (Join-Path $PSScriptRoot 'Build-BRep.ps1') }
if ($configuration.features.brep) {
    if ($Preset -notlike 'Windows*') { throw 'The current BREP SDK supports Windows x86_64 only.' }
    if (-not (Test-Path -LiteralPath (Join-Path $projectPath 'addons/brep/bin/runtime-manifest.json'))) {
        throw 'BREP runtime is missing. Run Build-BRep.ps1 first, or pass -BuildBRep.'
    }
}
if ($configuration.features.brep) {
    $brepManifest = Get-Content -LiteralPath (Join-Path $projectPath 'addons/brep/bin/runtime-manifest.json') -Raw | ConvertFrom-Json
    foreach ($entry in $brepManifest.runtime_sha256.PSObject.Properties) {
        $runtimeFile = Join-Path $projectPath "addons/brep/bin/$($entry.Name)"
        if (-not (Test-Path -LiteralPath $runtimeFile) -or (Get-FileHash -LiteralPath $runtimeFile).Hash -ne $entry.Value) {
            throw "BREP runtime hash mismatch: $($entry.Name). Rebuild with Build-BRep.ps1."
        }
    }
}
if (-not $Editor) { $Editor = Join-Path $repoPath 'bin/godot.windows.editor.dev.x86_64.console.exe' }
if (-not (Test-Path -LiteralPath $Editor)) { throw "Editor not found: $Editor" }
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$version = (& $Editor --version | Out-String).Trim()
if ($version -notmatch '^4\.7\.3\.rc') { throw "Expected custom Godot 4.7.3-rc, found $version" }
if ($BuildTemplate) {
    if ($Preset -notlike 'Windows*') { throw '-BuildTemplate currently supports Windows x86_64 only.' }
    & (Join-Path $PSScriptRoot 'Build-Template.ps1') -Profile $Profile
}
if ($Preset -like 'Windows*' -and -not $PackOnly) {
    if (-not $Template) {
        $Template = Join-Path $repoPath "bin/godot.windows.template_release.x86_64.$($configuration.template_suffix).exe"
        # Keep existing ALL templates usable, but probe their native modules below.
        if ($Profile -eq 'ALL' -and -not (Test-Path -LiteralPath $Template)) {
            $Template = Join-Path $repoPath 'bin/godot.windows.template_release.x86_64.exe'
        }
    }
    if (-not (Test-Path -LiteralPath $Template)) {
        throw "Matching $Profile template not found: $Template. Run Build-Template.ps1 -Profile $Profile, or use -BuildTemplate."
    }
}
if ($Template) {
    $Template = (Resolve-Path -LiteralPath $Template).Path
}
if ($Profile -eq 'CAD' -and $Preset -notlike 'Windows*' -and -not $PackOnly -and -not $Template) {
    throw 'CAD requires an explicitly supplied platform template with osgdb, 3dtiles and tiled_mesh_3d disabled.'
}
$outputDirectory = Join-Path $projectPath "$(if ($Preset -eq 'Windows Validation') { 'build/validation' } else { 'build/desktop' })/$Profile"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
if ($configuration.features.brep) {
    # Godot prefers the original res:// library path when it exists on disk.
    # An old nested adapter can otherwise load the new app-local SDK and crash.
    $legacyRuntime = [IO.Path]::GetFullPath((Join-Path $outputDirectory 'addons/brep'))
    if (Test-Path -LiteralPath $legacyRuntime) {
        $resolvedLegacy = (Resolve-Path -LiteralPath $legacyRuntime).Path
        $resolvedOutput = (Resolve-Path -LiteralPath $outputDirectory).Path
        if ($resolvedLegacy -ne (Join-Path $resolvedOutput 'addons/brep') -or
            -not $resolvedOutput.StartsWith($projectPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Legacy BREP runtime is outside the export directory: $resolvedLegacy"
        }
        foreach ($candidate in @($outputDirectory, (Join-Path $outputDirectory 'addons'), $legacyRuntime)) {
            if ((Get-Item -LiteralPath $candidate).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing to move a linked export directory: $candidate"
            }
        }
        $retiredRoot = Join-Path $projectPath 'build/retired-brep'
        New-Item -ItemType Directory -Force -Path $retiredRoot | Out-Null
        $retiredPath = [IO.Path]::GetFullPath((Join-Path $retiredRoot "$Profile-$([Guid]::NewGuid().ToString('N'))"))
        if (-not $retiredPath.StartsWith([IO.Path]::GetFullPath($retiredRoot) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Invalid legacy BREP backup path.'
        }
        Move-Item -LiteralPath $resolvedLegacy -Destination $retiredPath
        Write-Output "Retired legacy BREP runtime to $retiredPath"
    }
}
New-Item -ItemType File -Force -Path (Join-Path $projectPath 'build/.gdignore') | Out-Null
# A fresh staging directory prevents profile switches or parallel exports from
# changing project.godot, export presets, imports or branding in the source tree.
$stagePath = Join-Path $projectPath "build/staging/$Profile-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $stagePath | Out-Null
foreach ($directory in @('scripts', 'scenes', 'themes', 'assets', 'profiles')) {
    Copy-Item -LiteralPath (Join-Path $projectPath $directory) -Destination $stagePath -Recurse
}
if ($configuration.features.brep) {
    $addonPath = Join-Path $stagePath 'addons/brep'
    New-Item -ItemType Directory -Force -Path $addonPath | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectPath 'addons/brep/brep.gdextension') -Destination $addonPath
    Copy-Item -LiteralPath (Join-Path $projectPath 'addons/brep/bin') -Destination $addonPath -Recurse
}
if ($Preset -eq 'Windows Validation') {
    Copy-Item -LiteralPath (Join-Path $projectPath 'tests') -Destination $stagePath -Recurse
}
New-Item -ItemType Directory -Path (Join-Path $stagePath 'tools') | Out-Null
foreach ($file in @('configure_export.gd')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $stagePath 'tools')
}
foreach ($file in @('project.godot', 'export_presets.cfg')) {
    Copy-Item -LiteralPath (Join-Path $projectPath $file) -Destination $stagePath
}
function Invoke-ViewerChecked {
    param([string]$Executable, [string[]]$Arguments, [string]$SuccessMarker = '')
    $log = & $Executable @Arguments 2>&1
    $code = $LASTEXITCODE
    $log | Write-Output
    $text = $log -join "`n"
    if ($code -ne 0 -or $text -match 'SCRIPT ERROR|ERROR:|Parse Error|Export failed|Cannot export|Failed to export' -or ($SuccessMarker -and $text -notmatch [regex]::Escape($SuccessMarker))) {
        throw "DigitalViewer command failed (exit $code): $($Arguments -join ' ')"
    }
}
$configureArguments = @('--headless', '--path', $stagePath, '--script', 'res://tools/configure_export.gd', '--', "--dv-profile=$Profile", "--dv-preset=$Preset", "--dv-source=$projectPath")
if ($Template) { $configureArguments += "--dv-template=$Template" }
Invoke-ViewerChecked $Editor $configureArguments "DIGITALVIEWER_PROFILE_CONFIGURED $Profile"
Invoke-ViewerChecked $Editor @('--headless', '--path', $stagePath, '--import')
$extension = if ($PackOnly) { 'pck' } elseif ($Preset -like 'Windows*') { 'exe' } elseif ($Preset -eq 'macOS') { 'zip' } else { 'x86_64' }
$outputPath = Join-Path $outputDirectory "DigitalViewer.$extension"
$mode = if ($PackOnly) { '--export-pack' } else { '--export-release' }
Invoke-ViewerChecked $Editor @('--headless', '--path', $stagePath, $mode, $Preset, $outputPath)
if (-not (Test-Path -LiteralPath $outputPath)) { throw 'Export did not create an artifact.' }
if ($configuration.features.brep) {
    # Godot remaps GDExtension binaries to the export directory root.
    Get-ChildItem -LiteralPath (Join-Path $projectPath 'addons/brep/bin') -File | Where-Object { $_.Extension -in @('.dll', '.json', '.md', '.txt') } | Copy-Item -Destination $outputDirectory
    $licensePath = Join-Path $outputDirectory 'BREP-LICENSES'
    New-Item -ItemType Directory -Force -Path $licensePath | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $projectPath 'addons/brep/licenses') -File | Copy-Item -Destination $licensePath
}
if ($Preset -like 'Windows*' -and -not $PackOnly) {
    $startupOutput = Join-Path $outputDirectory 'startup.log'
    $startupErrors = Join-Path $outputDirectory 'startup-errors.log'
    # Release templates can disable --path/--script. Probe the packaged runtime
    # beside its own PCK and explicitly await the Windows GUI executable.
    $probe = Start-Process -FilePath $outputPath -ArgumentList '--headless', '--', '--dv-check-template' -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $startupOutput -RedirectStandardError $startupErrors
    $probeText = (Get-Content -LiteralPath $startupOutput, $startupErrors -Raw) -join "`n"
    if ($probe.ExitCode -ne 0 -or $probeText -notmatch "DIGITALVIEWER_TEMPLATE_OK $Profile" -or $probeText -match 'SCRIPT ERROR|ERROR:|Parse Error') { throw "Exported template does not match ${Profile}: $probeText" }
    $startup = Start-Process -FilePath $outputPath -ArgumentList '--headless', '--quit-after', '8' -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $startupOutput -RedirectStandardError $startupErrors
    $startupText = (Get-Content -LiteralPath $startupOutput, $startupErrors -Raw) -join "`n"
    if ($startup.ExitCode -ne 0 -or $startupText -match 'SCRIPT ERROR|ERROR:|Parse Error') { throw "Exported application startup failed: $startupText" }
}
Copy-Item -LiteralPath (Join-Path $repoPath 'LICENSE.txt') -Destination (Join-Path $outputDirectory 'GODOT-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repoPath 'COPYRIGHT.txt') -Destination (Join-Path $outputDirectory 'THIRD-PARTY-LICENSES.txt')
if ($configuration.features.osg) {
    Copy-Item -LiteralPath (Join-Path $repoPath 'thirdparty/osgdb/src/osgDB/deprecated-dotosg/osg/LICENSE.txt') -Destination (Join-Path $outputDirectory 'OSG-LICENSE.txt')
}
Copy-Item -LiteralPath (Join-Path $projectPath 'docs/desktop-v1.md') -Destination (Join-Path $outputDirectory 'IMPLEMENTATION-STATUS.md')
Copy-Item -LiteralPath (Join-Path $projectPath 'docs/windows-package.md') -Destination (Join-Path $outputDirectory 'README.md')
$runtimeFiles = @('project.godot', 'export_presets.cfg', 'scenes/main.tscn', $configuration.theme.Replace('res://', ''), $configuration.icon.Replace('res://', ''), "profiles/$Profile.json", 'scripts/app_state.gd', 'scripts/main_ui.gd')
$runtimeFiles += Get-ChildItem -LiteralPath (Join-Path $projectPath 'scripts/desktop'), (Join-Path $projectPath 'scripts/scripting') -Filter '*.gd' -File | ForEach-Object { [IO.Path]::GetRelativePath($projectPath, $_.FullName).Replace('\', '/') }
$sourceHashes = [ordered]@{}
foreach ($relative in ($runtimeFiles | Sort-Object)) {
    $sourceHashes[$relative] = (Get-FileHash -LiteralPath (Join-Path $projectPath $relative) -Algorithm SHA256).Hash
}
$manifest = [ordered]@{
    product = $configuration.product_name; version = '1.0.0'; status = 'development preview'; preset = $Preset
    profile = $Profile; features = $configuration.features; theme = $configuration.theme; icon = $configuration.icon
    editor = $version; artifact = [IO.Path]::GetFileName($outputPath)
    template_sha256 = if ($Template -and -not $PackOnly) { (Get-FileHash -LiteralPath $Template -Algorithm SHA256).Hash } else { $null }
    staged_project_sha256 = (Get-FileHash -LiteralPath (Join-Path $stagePath 'project.godot') -Algorithm SHA256).Hash
    sha256 = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    pck_sha256 = if (Test-Path -LiteralPath (Join-Path $outputDirectory 'DigitalViewer.pck')) { (Get-FileHash -LiteralPath (Join-Path $outputDirectory 'DigitalViewer.pck') -Algorithm SHA256).Hash } else { $null }
    built_at_utc = [DateTime]::UtcNow.ToString('o')
    source_commit = (& git -C $repoPath rev-parse HEAD | Out-String).Trim()
    source_dirty = [bool](& git -C $repoPath status --porcelain | Out-String).Trim()
    runtime_sources_sha256 = $sourceHashes
    brep_runtime = if ($configuration.features.brep) { Get-Content -LiteralPath (Join-Path $projectPath 'addons/brep/bin/runtime-manifest.json') -Raw | ConvertFrom-Json } else { $null }
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputDirectory 'build-manifest.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $projectPath "profiles/$Profile.json") -Destination (Join-Path $outputDirectory 'build-profile.json')
if ($Preset -eq 'Windows Desktop' -and -not $PackOnly) {
    $packageNames = @('DigitalViewer.exe', 'DigitalViewer.pck', 'GODOT-LICENSE.txt', 'THIRD-PARTY-LICENSES.txt', 'IMPLEMENTATION-STATUS.md', 'README.md', 'build-manifest.json', 'build-profile.json')
    if ($configuration.features.osg) { $packageNames += 'OSG-LICENSE.txt' }
    if ($configuration.features.brep) {
        $packageNames += 'BREP-LICENSES'
        $packageNames += Get-ChildItem -LiteralPath (Join-Path $projectPath 'addons/brep/bin') -File | Where-Object { $_.Extension -in @('.dll', '.json', '.md', '.txt') } | Select-Object -ExpandProperty Name
    }
    $packageFiles = $packageNames | ForEach-Object { Join-Path $outputDirectory $_ }
    Compress-Archive -LiteralPath $packageFiles -DestinationPath (Join-Path $projectPath "build/DigitalViewer-$Profile-1.0.0-windows-x86_64-preview.zip") -Force
}
Write-Output "Created $outputPath"

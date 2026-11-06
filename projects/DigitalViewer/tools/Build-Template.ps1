param(
    [ValidateSet('ALL', 'CAD')][string]$Profile = 'ALL',
    [ValidateSet('template_release', 'template_debug')][string]$Target = 'template_release',
    [ValidateRange(1, 256)][int]$Jobs = [Math]::Min(8, [Math]::Max(1, [Environment]::ProcessorCount - 2)),
    [string]$SCons = 'scons',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Profile-Common.ps1')
$configuration = Get-DigitalViewerProfile $Profile
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repoPath = [IO.Path]::GetFullPath((Join-Path $projectPath '../..'))
$buildPath = Join-Path $projectPath 'build/templates'
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $projectPath 'build/.gdignore') | Out-Null
$nativeProfile = Join-Path $buildPath "$Profile.py"
$moduleFlags = [ordered]@{
    osgdb = $configuration.features.osg
    '3dtiles' = $configuration.features.'3dtiles'
    tiled_mesh_3d = ($configuration.features.osg -or $configuration.features.'3dtiles')
}
$lines = @('# Generated from profiles/' + $Profile + '.json. Do not edit.')
foreach ($module in $moduleFlags.Keys) {
    $lines += "module_${module}_enabled = $(if ($moduleFlags[$module]) { 'True' } else { 'False' })"
}
# glTF (including Draco), SVG, ZIP and native scripting remain available in CAD.
$lines += @('module_gltf_enabled = True', 'module_draco_enabled = True', 'module_svg_enabled = True', 'module_zip_enabled = True', 'module_gdscript_enabled = True')
Set-Content -LiteralPath $nativeProfile -Value $lines -Encoding utf8
$arguments = @('platform=windows', "target=$Target", 'arch=x86_64', "profile=$nativeProfile", "extra_suffix=$($configuration.template_suffix)", 'angle=no', 'accesskit=no', 'd3d12=no', "-j$Jobs")
if ($DryRun) { $arguments += '--dry-run' }
Push-Location $repoPath
try {
    & $SCons @arguments
    if ($LASTEXITCODE -ne 0) { throw "Template build failed (exit $LASTEXITCODE)." }
} finally { Pop-Location }
if (-not $DryRun) {
    Write-Output "Created $(Join-Path $repoPath "bin/godot.windows.$Target.x86_64.$($configuration.template_suffix).exe")"
}

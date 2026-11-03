param(
    [string]$Engine = '',
    [switch]$Graphics,
    [switch]$Capture,
    [switch]$Pack,
    [string]$Dataset = '',
    [switch]$DatasetNavigate,
    [switch]$DatasetExpectEmpty,
    [ValidateRange(5, 120)][int]$DatasetSeconds = 30
)
$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repoPath = [IO.Path]::GetFullPath((Join-Path $projectPath '../..'))
if (-not $Engine) { $Engine = Join-Path $repoPath 'bin/godot.windows.editor.dev.x86_64.console.exe' }
if (-not (Test-Path -LiteralPath $Engine)) { throw "Engine not found: $Engine" }
$frameLimit = if ($Dataset) { [string][Math]::Max(3000, ($DatasetSeconds + 30) * 240) } else { '3000' }
$arguments = @('--path', $projectPath, '--resolution', '1366x768', '--quit-after', $frameLimit)
if ($Pack) { $arguments = @('--resolution', '1366x768', '--quit-after', $frameLimit) }
if (-not $Graphics) { $arguments += '--headless' }
$arguments += @('--', '--dv-smoke-test')
if ($Capture) { $arguments += '--dv-capture' }
$previousTestDirectory = $env:DV_TEST_DIRECTORY
$previousDataset = $env:DV_TEST_DATASET
$previousDatasetSeconds = $env:DV_DATASET_SECONDS
$previousDatasetNavigate = $env:DV_DATASET_NAVIGATE
$previousDatasetExpectEmpty = $env:DV_DATASET_EXPECT_EMPTY
try {
    $env:DV_TEST_DIRECTORY = Join-Path $projectPath 'build/validation/scratch'
    $env:DV_TEST_DATASET = if ($Dataset) { (Resolve-Path -LiteralPath $Dataset).Path } else { '' }
    $env:DV_DATASET_SECONDS = [string]$DatasetSeconds
    $env:DV_DATASET_NAVIGATE = if ($DatasetNavigate) { '1' } else { '' }
    $env:DV_DATASET_EXPECT_EMPTY = if ($DatasetExpectEmpty) { '1' } else { '' }
    $output = & $Engine @arguments 2>&1
    $code = $LASTEXITCODE
} finally {
    $env:DV_TEST_DIRECTORY = $previousTestDirectory
    $env:DV_TEST_DATASET = $previousDataset
    $env:DV_DATASET_SECONDS = $previousDatasetSeconds
    $env:DV_DATASET_NAVIGATE = $previousDatasetNavigate
    $env:DV_DATASET_EXPECT_EMPTY = $previousDatasetExpectEmpty
}
$text = $output -join [Environment]::NewLine
$output | Write-Output
if ($code -ne 0 -or $text -notmatch 'DIGITALVIEWER_DESKTOP_OK checks=\d+' -or $text -match 'SCRIPT ERROR|DIGITALVIEWER_DESKTOP_FAILED|DESKTOP_CHECK:') {
    throw "DigitalViewer desktop verification failed (exit $code)."
}

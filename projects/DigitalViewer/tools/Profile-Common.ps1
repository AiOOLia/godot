function Get-DigitalViewerProfile {
    param([Parameter(Mandatory)][ValidateSet('ALL', 'CAD')][string]$Profile)
    $profilePath = Join-Path $PSScriptRoot "../profiles/$Profile.json"
    $configuration = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
    if ($configuration.id -cne $Profile) { throw "Invalid profile id in $profilePath" }
    foreach ($feature in @('osg', '3dtiles', 'gis', 'brep')) {
        if ($configuration.features.$feature -isnot [bool]) { throw "Profile requires a boolean feature: $feature" }
    }
    if ($configuration.template_suffix -notmatch '^dv_[a-z]+$') { throw 'Invalid template suffix.' }
    return $configuration
}

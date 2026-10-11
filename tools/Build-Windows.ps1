param([switch]$Install)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repo 'Source\build\windows-fx'

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}

Invoke-Checked 'cmake' @('-S', (Join-Path $repo 'Source'), '-B', $build,
    '-A', 'x64', '-DSMARTSAMPLER_BUILD_TESTS=ON')
Invoke-Checked 'cmake' @('--build', $build, '--config', 'Release', '--parallel', '2',
    '--target', 'SmartSampler_VST3', 'SmartSampler_Standalone',
    'SmartSamplerFX_VST3', 'SmartSamplerFX_Standalone',
    'SmartSamplerTests', 'SmartSamplerFXTests')
Invoke-Checked 'ctest' @('--test-dir', $build, '-C', 'Release', '--output-on-failure')

$fx = Join-Path $build 'SmartSamplerFX_artefacts\Release\VST3\SmartSampler FX.vst3'
Write-Host "FX VST3 bundle: $fx"
if ($Install) {
    $destination = Join-Path $env:CommonProgramFiles 'VST3'
    if (Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match 'Ableton|Studio One|Studio Pro|Fender' }) {
        throw 'Close Ableton and Fender Studio Pro before installing.'
    }
    if (Test-Path (Join-Path $destination 'SmartSampler FX.vst3')) {
        throw 'An FX bundle already exists. Back it up or remove it manually before installing.'
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath $fx -Destination $destination -Recurse
    Write-Host 'Installed SmartSampler FX. Enable VST3 scanning and rescan in your DAW.'
}

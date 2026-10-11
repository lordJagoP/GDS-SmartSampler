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
    $mutex = New-Object System.Threading.Mutex($false, 'Local\GDSSmartSamplerFXInstall')
    $held = $false
    $stage = Join-Path $destination ('_SmartSamplerFX_stage_' + [guid]::NewGuid().ToString() + '.vst3')
    try {
        $held = $mutex.WaitOne(0)
        if (-not $held) { throw 'Another SmartSampler FX installation is running.' }
        $final = Join-Path $destination 'SmartSampler FX.vst3'
        if (Test-Path -LiteralPath $final) {
            throw 'An FX bundle already exists. Back it up or remove it manually before installing.'
        }
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Copy-Item -LiteralPath $fx -Destination $stage -Recurse
        Move-Item -LiteralPath $stage -Destination $final
        Write-Host 'Installed SmartSampler FX. Enable VST3 scanning and rescan in your DAW.'
    } finally {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
        if ($held) { $mutex.ReleaseMutex() }
        $mutex.Dispose()
    }
}

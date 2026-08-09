<#
.SYNOPSIS
    Ensures the pinned Vulkan SDK is installed. Timefall needs the SDK because it ships
    Slang (slangc.exe + slang.dll) and the Vulkan C++ headers (vulkan.hpp).

.DESCRIPTION
    Non-interactive: no prompts, no PAUSE.
    Exit codes: 0 = ready, 1 = failed, 2 = an install is needed but the shell is not elevated
    (the LunarG installer writes to C:\VulkanSDK and HKLM). Setup-Vulkan.bat re-runs elevated on 2.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\Setup-Vulkan.ps1
#>
[CmdletBinding()]
param(
    [string]$PinnedVersion = '1.4.350.0',
    [switch]$AllowNewer,   # accept an installed SDK newer than the pin instead of reinstalling
    [switch]$Force         # reinstall even when the pinned version is already present
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'  # Invoke-WebRequest is ~20x slower with the progress bar

$InstallRoot   = "C:\VulkanSDK\$PinnedVersion"
$InstallerName = "vulkansdk-windows-X64-$PinnedVersion.exe"
$DownloadUrl   = "https://sdk.lunarg.com/sdk/download/$PinnedVersion/windows/$InstallerName"
$InstallerPath = Join-Path $env:TEMP $InstallerName

# What the engine actually consumes. Missing any one of these makes the install unusable.
$RequiredFiles = @(
    'Include\vulkan\vulkan.hpp',       # decision 18: C++ bindings, not the C API
    'Include\vulkan\vulkan_raii.hpp',
    'Include\slang\slang.h',
    'Include\slang\slang-com-ptr.h',
    'Bin\slangc.exe',                  # build-time shader cache pre-warm
    'Bin\slang.dll',                   # forwarding shim ...
    'Bin\slang-compiler.dll',          # ... forwards to this
    'Bin\slang-glsl-module.dll',       # required for -target glsl
    'Lib\slang.lib'
)

function Write-Step { param([string]$m) Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok   { param([string]$m) Write-Host "    OK   $m" -ForegroundColor Green }
function Write-Bad  { param([string]$m) Write-Host "    FAIL $m" -ForegroundColor Red }

function Test-Elevated {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Returns the installed version parsed out of a SDK root path, or $null.
function Get-SdkVersion {
    param([string]$Root)
    if ([string]::IsNullOrWhiteSpace($Root)) { return $null }
    if ($Root -match '(\d+\.\d+\.\d+\.\d+)') { return [version]$Matches[1] }
    return $null
}

# Returns the list of missing required files under $Root (empty array = complete).
function Get-MissingFiles {
    param([string]$Root)
    $missing = @()
    foreach ($rel in $RequiredFiles) {
        if (-not (Test-Path (Join-Path $Root $rel))) { $missing += $rel }
    }
    return $missing
}

# The validation layer is registered as a value name under this key, pointing at its manifest.
function Test-ValidationLayer {
    foreach ($hive in 'HKLM:\SOFTWARE\Khronos\Vulkan\ExplicitLayers',
                      'HKCU:\SOFTWARE\Khronos\Vulkan\ExplicitLayers') {
        $key = Get-Item $hive -ErrorAction SilentlyContinue
        if (-not $key) { continue }
        foreach ($name in $key.GetValueNames()) {
            if ($name -like '*VkLayer_khronos_validation.json' -and (Test-Path $name)) { return $true }
        }
    }
    return $false
}

# ---------------------------------------------------------------- current state

Write-Step "Checking for Vulkan SDK $PinnedVersion"

$pinned     = [version]$PinnedVersion
$currentSdk = $env:VULKAN_SDK
$current    = Get-SdkVersion $currentSdk

$needsInstall = $true
if ($Force) {
    Write-Host "    -Force specified; reinstalling."
} elseif (-not $current) {
    Write-Host "    VULKAN_SDK is not set."
} elseif ($current -eq $pinned -and (Get-MissingFiles $currentSdk).Count -eq 0) {
    Write-Ok "$currentSdk is the pinned version and complete."
    $InstallRoot  = $currentSdk
    $needsInstall = $false
} elseif ($current -gt $pinned -and $AllowNewer -and (Get-MissingFiles $currentSdk).Count -eq 0) {
    Write-Ok "$currentSdk is newer than the pin and -AllowNewer was passed."
    $InstallRoot  = $currentSdk
    $needsInstall = $false
} elseif ($current -ne $pinned) {
    Write-Host "    Installed $current does not match the pin $pinned."
} else {
    Write-Host "    Installed $current is incomplete: $((Get-MissingFiles $currentSdk) -join ', ')"
}

# ---------------------------------------------------------------- install

if ($needsInstall) {
    if (-not (Test-Elevated)) {
        Write-Bad "Installing the Vulkan SDK requires an elevated shell."
        Write-Host "    Re-run scripts\Setup-Vulkan.bat (it elevates for you), or from an Administrator PowerShell:"
        Write-Host "      powershell -ExecutionPolicy Bypass -File scripts\Setup-Vulkan.ps1"
        exit 2   # Setup-Vulkan.bat treats 2 as "relaunch me elevated"
    }

    if ((Test-Path $InstallerPath) -and ((Get-Item $InstallerPath).Length -gt 100MB)) {
        Write-Step "Reusing cached installer $InstallerPath"
    } else {
        Write-Step "Downloading $DownloadUrl (~310 MB)"
        Invoke-WebRequest -Uri $DownloadUrl -OutFile $InstallerPath -UseBasicParsing
    }

    # Qt Installer Framework silent install. Bare `install` takes the default component set,
    # which already includes Slang; no component ids need naming.
    Write-Step "Installing to $InstallRoot (silent, a few minutes)"
    $installArgs = @(
        '--root', $InstallRoot,
        '--accept-licenses',
        '--default-answer',
        '--confirm-command',
        'install'
    )
    $proc = Start-Process -FilePath $InstallerPath -ArgumentList $installArgs -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) {
        Write-Bad "Installer exited with code $($proc.ExitCode). See $InstallRoot\InstallationLog.txt"
        exit 1
    }

    # The installer sets VULKAN_SDK machine-wide; make it usable in *this* process too.
    $env:VULKAN_SDK = $InstallRoot
}

# ---------------------------------------------------------------- verify

Write-Step "Verifying $InstallRoot"
$failed = $false

foreach ($rel in $RequiredFiles) {
    $full = Join-Path $InstallRoot $rel
    if (Test-Path $full) { Write-Ok $rel } else { Write-Bad "$rel  (missing)"; $failed = $true }
}

# slangc prints its version banner to stderr, which $ErrorActionPreference='Stop' would trap.
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
# Collect everything first: `Select-Object -First` would stop the pipeline before $LASTEXITCODE is set.
$slangOut = & (Join-Path $InstallRoot 'Bin\slangc.exe') -v 2>&1 |
    ForEach-Object { if ($_ -is [Management.Automation.ErrorRecord]) { $_.Exception.Message } else { $_ } }
$slangExit = $LASTEXITCODE
$ErrorActionPreference = $prevEap
$slangVersion = ($slangOut | Select-Object -First 1 | Out-String).Trim()

if ($slangExit -eq 0 -and $slangVersion -match '^\d') {
    Write-Ok "slangc reports Slang $slangVersion"
} else {
    Write-Bad "slangc.exe -v failed (exit $slangExit): $slangVersion"
    $failed = $true
}

if (Test-ValidationLayer) {
    Write-Ok 'VK_LAYER_KHRONOS_validation is registered.'
} else {
    Write-Bad 'VK_LAYER_KHRONOS_validation is not registered (needed once Vulkan bring-up starts).'
    $failed = $true
}

if ($failed) {
    Write-Host ''
    Write-Host 'Vulkan SDK setup INCOMPLETE.' -ForegroundColor Red
    exit 1
}

Write-Host ''
Write-Host "Vulkan SDK $PinnedVersion ready at $InstallRoot" -ForegroundColor Green
Write-Host 'NOTE: VULKAN_SDK is a machine environment variable. Close and reopen your shell' -ForegroundColor Yellow
Write-Host '      (and restart Visual Studio) before running premake, or $(VULKAN_SDK) will' -ForegroundColor Yellow
Write-Host '      not resolve during the build.' -ForegroundColor Yellow
exit 0

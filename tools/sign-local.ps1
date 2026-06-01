[CmdletBinding()]
param(
    [string]$Subject = "CN=Orbit Guard Local Dev Code Signing",
    [string[]]$Paths = @(),
    [switch]$IncludeSmokeTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Get-OrCreateCodeSigningCertificate {
    param([string]$CertSubject)

    $certificate = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq $CertSubject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1

    if (-not $certificate) {
        $certificate = New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject $CertSubject `
            -CertStoreLocation Cert:\CurrentUser\My `
            -KeyUsage DigitalSignature `
            -HashAlgorithm SHA256 `
            -NotAfter (Get-Date).AddYears(3)
    }

    foreach ($storeName in @("Root", "TrustedPublisher")) {
        $storePath = "Cert:\CurrentUser\$storeName"
        $trustedCertificate = Get-ChildItem $storePath |
            Where-Object { $_.Thumbprint -eq $certificate.Thumbprint } |
            Select-Object -First 1

        if (-not $trustedCertificate) {
            $cerPath = Join-Path $env:TEMP ("orbit-guard-{0}-{1}.cer" -f $storeName, $certificate.Thumbprint)
            Export-Certificate -Cert $certificate -FilePath $cerPath | Out-Null
            Import-Certificate -FilePath $cerPath -CertStoreLocation $storePath | Out-Null
            Remove-Item -LiteralPath $cerPath -Force
        }
    }

    return $certificate
}

function Resolve-TargetFiles {
    param([string[]]$RequestedPaths, [switch]$WithSmokeTests)

    $targets = New-Object System.Collections.Generic.List[string]

    if ($RequestedPaths.Count -gt 0) {
        foreach ($path in $RequestedPaths) {
            $resolvedPath = if ([System.IO.Path]::IsPathRooted($path)) {
                Resolve-Path $path -ErrorAction Stop
            } else {
                Resolve-Path (Join-Path $projectRoot $path) -ErrorAction Stop
            }
            $targets.Add($resolvedPath.Path)
        }
        return $targets | Select-Object -Unique
    }

    foreach ($name in @("OrbitGuard.exe", "Project orbit guard.exe", "OrbitGuard_verify.exe", "raylib.dll")) {
        $path = Join-Path $projectRoot $name
        if (Test-Path -LiteralPath $path) {
            $targets.Add((Resolve-Path $path).Path)
        }
    }

    if ($WithSmokeTests) {
        Get-ChildItem -Path $projectRoot -Filter "*_smoke*.exe" -File -ErrorAction SilentlyContinue |
            ForEach-Object { $targets.Add($_.FullName) }
        Get-ChildItem -Path (Join-Path $projectRoot "tests") -Filter "*_smoke*.exe" -File -ErrorAction SilentlyContinue |
            ForEach-Object { $targets.Add($_.FullName) }
    }

    return $targets | Select-Object -Unique
}

$cert = Get-OrCreateCodeSigningCertificate -CertSubject $Subject
$targets = Resolve-TargetFiles -RequestedPaths $Paths -WithSmokeTests:$IncludeSmokeTests

if ($targets.Count -eq 0) {
    throw "No target .exe/.dll files were found to sign."
}

foreach ($target in $targets) {
    Unblock-File -LiteralPath $target -ErrorAction SilentlyContinue
    $signature = Set-AuthenticodeSignature -FilePath $target -Certificate $cert -HashAlgorithm SHA256
    if ($signature.Status -ne "Valid") {
        throw "Signing failed for $target with status $($signature.Status): $($signature.StatusMessage)"
    }

    Write-Host "Signed: $target"
}

Write-Host "Certificate thumbprint: $($cert.Thumbprint)"

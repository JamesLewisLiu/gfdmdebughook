[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $BinaryPath,
    [string] $OutputDirectory = 'dist'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sourceRoot = Split-Path -Parent $PSScriptRoot
$binary = (Resolve-Path -LiteralPath $BinaryPath).Path
if ((Split-Path -Leaf $binary) -cne 'gfdmdebug.dll') {
    throw 'Package only the production gfdmdebug.dll, never the test DLL.'
}

# Reject wrong-platform or non-DLL files before packaging. Does not execute code.
$stream = [IO.File]::OpenRead($binary)
$reader = [IO.BinaryReader]::new($stream)
try {
    if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'Not a PE image' }
    $stream.Position = 0x3c
    $peOffset = $reader.ReadInt32()
    if ($peOffset -lt 0 -or $peOffset -gt $stream.Length - 26) { throw 'Invalid PE header' }
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x14c) {
        throw 'Expected a Windows x86 production DLL'
    }
    $stream.Position = $peOffset + 22
    if (($reader.ReadUInt16() -band 0x2000) -eq 0 -or $reader.ReadUInt16() -ne 0x10b) {
        throw 'Expected a PE32 DLL'
    }
} finally {
    $reader.Dispose()
    $stream.Dispose()
}

$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutputDirectory))
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$packageName = 'GFDM-DebugHook-x86-development'
$packageRoot = Join-Path $outputRoot $packageName
$archivePath = Join-Path $outputRoot "$packageName.zip"
if ((Test-Path -LiteralPath $packageRoot) -or (Test-Path -LiteralPath $archivePath)) {
    throw 'Package destination already exists; use a fresh OutputDirectory.'
}
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'licenses') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'docs') -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $packageRoot 'gfdmdebug.dll')
foreach ($name in @('gfdmdebug.ini', 'README.md', 'LICENSE', 'DEPENDENCIES.json')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination $packageRoot
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/native-debug.md') -Destination (Join-Path $packageRoot 'docs/native-debug.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/xg23-debug.md') -Destination (Join-Path $packageRoot 'docs/xg23-debug.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'vendor/imgui/LICENSE.txt') -Destination (Join-Path $packageRoot 'licenses/imgui-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'vendor/minhook/LICENSE.txt') -Destination (Join-Path $packageRoot 'licenses/minhook-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'licenses/DebugHook-original-MIT.txt') -Destination (Join-Path $packageRoot 'licenses/DebugHook-original-MIT.txt')
$manifest = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = $_.FullName.Substring($packageRoot.Length + 1).Replace('\', '/')
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes = $_.Length
    }
})
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $packageRoot 'MANIFEST.json') -Encoding utf8
Compress-Archive -LiteralPath $packageRoot -DestinationPath $archivePath
$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
"$archiveHash  $packageName.zip" | Set-Content -LiteralPath (Join-Path $outputRoot 'SHA256SUMS.txt') -Encoding ascii
Write-Output "Package: $archivePath"
Write-Output "SHA256: $archiveHash"

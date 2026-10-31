<#
.SYNOPSIS
Installs and packages a tested Windows x64 Release build with versioned assets.
.PARAMETER build_directory
CMake binary directory, relative to the project root or absolute.
.PARAMETER output_directory
Release asset directory, relative to the project root or absolute.
.OUTPUTS
Portable ZIP, version-stamped DLL/loader and SHA256SUMS.txt in the output directory.
#>
param(
    [string]$build_directory = 'build',
    [string]$output_directory = 'release_assets'
)

$ErrorActionPreference = 'Stop'
$project_root = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $project_root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'VERSION must contain a semantic version such as 1.0.0.' }
$build_path = if ([IO.Path]::IsPathRooted($build_directory)) { $build_directory } else { Join-Path $project_root $build_directory }
$output_path = if ([IO.Path]::IsPathRooted($output_directory)) { $output_directory } else { Join-Path $project_root $output_directory }
$stage_path = Join-Path $build_path ('package-' + $version + '-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output_path -Force | Out-Null
cmake --install $build_path --config Release --prefix $stage_path
if ($LASTEXITCODE -ne 0) { throw 'CMake install failed.' }

foreach ($name in @('azs_overlay.dll', 'load_overlay.exe')) {
    $file_path = Join-Path $stage_path $name
    $file_version = [Diagnostics.FileVersionInfo]::GetVersionInfo($file_path).ProductVersion
    if ($file_version -ne $version) { throw ('Unexpected binary version for ' + $name + ': ' + $file_version) }
    Copy-Item -LiteralPath $file_path -Destination (Join-Path $output_path $name) -Force
}
$archive_name = 'AS-Trainer-' + $version + '-windows-x64.zip'
Compress-Archive -Path (Join-Path $stage_path '*') -DestinationPath (Join-Path $output_path $archive_name) -CompressionLevel Optimal -Force
$checksum_lines = foreach ($name in @('azs_overlay.dll', 'load_overlay.exe', $archive_name)) {
    $digest = (Get-FileHash -LiteralPath (Join-Path $output_path $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    $digest + '  ' + $name
}
[IO.File]::WriteAllLines((Join-Path $output_path 'SHA256SUMS.txt'), $checksum_lines, [Text.UTF8Encoding]::new($false))
Write-Output ('Packaged AS Trainer ' + $version + ' in ' + $output_path)

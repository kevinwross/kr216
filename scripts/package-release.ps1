# Stages a self-contained kr216 toolchain from the sibling checkouts and zips
# it. The staged directory matches the layout examples/build.cmd expects.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path (Split-Path -Parent $root) 'krcompiler'
$out = Join-Path $root 'dist\kr216-toolchain'
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force -Path (Join-Path $out 'bin'), (Join-Path $out 'runtime'), (Join-Path $out 'examples') | Out-Null

$preset = 'windows-msvc-debug'
$hostBin = Join-Path $compiler "build\$preset\host\bin\Debug"
foreach ($tool in 'krc.exe', 'krld.exe') {
    Copy-Item (Join-Path $hostBin $tool) (Join-Path $out "bin\$tool")
}
Copy-Item (Join-Path $root "build\$preset\tools\kr216-emu\Debug\kr216-emu.exe") (Join-Path $out 'bin\kr216-emu.exe')
Copy-Item (Join-Path $root "build\$preset\tools\kr216-dis\Debug\kr216-dis.exe") (Join-Path $out 'bin\kr216-dis.exe')
Copy-Item (Join-Path $root "build\$preset\tools\kr216-bench\Debug\kr216-bench.exe") (Join-Path $out 'bin\kr216-bench.exe')

$runtime = Join-Path $compiler 'targets\kr216\runtime'
Copy-Item (Join-Path $runtime 'crt0.s') (Join-Path $out 'runtime\crt0.s')
Copy-Item (Join-Path $runtime 'syscalls.s') (Join-Path $out 'runtime\syscalls.s')
Copy-Item (Join-Path $runtime 'eh.s') (Join-Path $out 'runtime\eh.s')
Copy-Item (Join-Path $runtime 'libc.c') (Join-Path $out 'runtime\libc.c')

$stage = Join-Path $compiler "build\$preset\stage\sysroot"
Copy-Item $stage (Join-Path $out 'sysroot') -Recurse
Copy-Item (Join-Path $root 'examples\*.cpp') (Join-Path $out 'examples')
Copy-Item (Join-Path $root 'examples\build.cmd') (Join-Path $out 'examples\build.cmd')
Copy-Item (Join-Path $root 'README.md') (Join-Path $out 'README.md')

$compilerRevision = (git -C $compiler rev-parse --short HEAD)
$kr216Revision = (git -C $root rev-parse --short HEAD)
@"
kr216 toolchain
version 0.1.1
krcompiler $compilerRevision
kr216 $kr216Revision
"@ | Set-Content -Encoding ascii (Join-Path $out 'VERSION.txt')

$zip = Join-Path $root 'dist\kr216-toolchain.zip'
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $out -DestinationPath $zip
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$hash  $([IO.Path]::GetFileName($zip))" |
    Set-Content -Encoding ascii (Join-Path $root 'dist\SHA256SUMS-windows.txt')
Write-Host "staged $out"
Write-Host "archive $zip"
Write-Host "sha256 $hash"


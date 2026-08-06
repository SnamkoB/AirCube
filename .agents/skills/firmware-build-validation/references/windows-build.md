# Windows ESP-IDF Build Commands

Run all blocks from `firmware/` in PowerShell. Read the instructions between blocks; do not combine the blocks or publish early.

## Clean build

```powershell
$ErrorActionPreference = 'Stop'
$eim = "$env:LOCALAPPDATA\Microsoft\WinGet\Links\eim.exe"
$output = "..\output\firmware"
New-Item -ItemType Directory -Force -Path $output | Out-Null

& $eim run "idf.py fullclean" v6.0.2
if ($LASTEXITCODE -ne 0) { throw "Firmware clean failed" }
& $eim run "idf.py set-target esp32h2" v6.0.2
if ($LASTEXITCODE -ne 0) { throw "Target configuration failed" }

$buildOutput = & $eim run "idf.py build" v6.0.2 2>&1
$buildExit = $LASTEXITCODE
$buildOutput | Tee-Object -FilePath "$output\build.log"
if ($buildExit -ne 0) { throw "Firmware build failed" }
$buildFindings = $buildOutput | Select-String -Pattern '(?i)warning:|error:|FAILED|overflow|deprecat(ed|ion) warning'
if ($buildFindings) {
    $buildFindings
    throw "Repair build diagnostics, then repeat the clean build"
}
```

**Stop gate 1:** Read the complete `build.log`, not only regex matches. Repair every actionable warning/error and repeat the clean build. Continue only after explicitly concluding that the build succeeded with zero unexplained diagnostics.

## Merge

```powershell
$ErrorActionPreference = 'Stop'
$eim = "$env:LOCALAPPDATA\Microsoft\WinGet\Links\eim.exe"
$output = "..\output\firmware"
$source = "build\merged-binary.bin"
if (Test-Path -LiteralPath $source) { Remove-Item -LiteralPath $source -Force }
$mergeOutput = & $eim run "idf.py merge-bin" v6.0.2 2>&1
$mergeExit = $LASTEXITCODE
$mergeOutput | Tee-Object -FilePath "$output\merge.log"
if ($mergeExit -ne 0) { throw "Firmware merge failed" }
$mergeFindings = $mergeOutput | Select-String -Pattern '(?i)warning:|error:|FAILED|overflow|deprecat(ed|ion) warning'
if ($mergeFindings) {
    $mergeFindings
    throw "Repair merge diagnostics before publishing"
}
if (-not (Test-Path -LiteralPath $source)) { throw "Merged binary was not generated" }
if ((Get-Item -LiteralPath $source).Length -le 0) { throw "Merged binary is empty" }
```

**Stop gate 2:** Read the complete `merge.log` and verify that it says the image is ready for offset `0x0`. Continue only after explicitly concluding that the merge succeeded with zero unexplained diagnostics.

## Publish

Validate a temporary copy before replacing the versioned destination. `Move-Item -Force` performs the final replacement but is not claimed to be atomic on Windows PowerShell 5.1.

```powershell
$ErrorActionPreference = 'Stop'
$output = "..\output\firmware"
$source = "build\merged-binary.bin"
$version = (Get-Content -Raw "version.txt").Trim()
if ($version -notmatch '^\d+\.\d+\.\d+([-.][0-9A-Za-z.-]+)?$') {
    throw "Invalid firmware version: $version"
}
$destination = "$output\AirCube_firmware_v$version.bin"
$temporary = "$destination.tmp"
Copy-Item -Force $source $temporary
if ((Get-Item $source).Length -ne (Get-Item $temporary).Length) {
    throw "Published firmware size mismatch"
}
if ((Get-FileHash $source).Hash -ne (Get-FileHash $temporary).Hash) {
    throw "Published firmware hash mismatch"
}
Move-Item -Force $temporary $destination
if (-not (Test-Path -LiteralPath $destination)) { throw "Published firmware is missing" }
if ((Get-Item -LiteralPath $destination).Length -le 0) { throw "Published firmware is empty" }
if ((Get-Item $source).Length -ne (Get-Item $destination).Length) {
    throw "Final firmware size mismatch"
}
if ((Get-FileHash $source).Hash -ne (Get-FileHash $destination).Hash) {
    throw "Final firmware hash mismatch"
}
```

After publication, report the destination path, source and destination SHA-256 hash, size, app partition headroom, diagnostics reviewed, and hardware validation status.

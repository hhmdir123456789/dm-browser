$ErrorActionPreference = "Continue"
$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host ""
Write-Host "===== STEP 1: create test_iter3.html =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  .img1 { display: block; }
  .img2 { display: block; }
  td { padding: 6px; border: 1px solid #ccc; }
</style>
</head>
<body>
  <img class="img1" src="test.png" alt="img">
  <img class="img2" src="test.png" width="100" height="50" alt="img2">
  <table>
    <tr><td>r1c1</td><td>r1c2</td><td>r1c3</td></tr>
    <tr><td>r2c1</td><td>r2c2</td><td>r2c3</td></tr>
  </table>
</body>
</html>
'@
$html | Out-File -Encoding ASCII "$root\tests\pages\test_iter3.html"
Write-Host "  test_iter3.html created"

Write-Host ""
Write-Host "===== STEP 2: create test.png (8x8 red) =====" -ForegroundColor Cyan
$b64 = "iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAYAAADED76LAAAAFklEQVQoU2P8z8Dwn4EIwDiqkL4hxQAAe/4D/0Q6B6sAAAAASUVORK5CYII="
[IO.File]::WriteAllBytes("$root\tests\pages\test.png", [Convert]::FromBase64String($b64))
Write-Host "  test.png created"

Write-Host ""
Write-Host "===== STEP 3: build =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED" -ForegroundColor Red; exit 1 }
Write-Host "BUILD OK" -ForegroundColor Green

Write-Host ""
Write-Host "===== STEP 4: dump =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" "$root\tests\pages\test_iter3.html" 1156 753 --dump

$snap = "$root\tests\pages\test_iter3.html.snapshot.json"
if (-not (Test-Path $snap)) { Write-Host "SNAPSHOT MISSING" -ForegroundColor Red; exit 1 }

$json = Get-Content $snap -Encoding UTF8 -Raw | ConvertFrom-Json

Write-Host ""
Write-Host "===== STEP 5: nodes =====" -ForegroundColor Cyan
$json.nodes | Where-Object { $_.tag -in @('img','table','tr','td') } |
  Select-Object @{n='path';e={$_.path}}, tag,
    @{n='x';e={$_.layout.x}}, @{n='y';e={$_.layout.y}},
    @{n='w';e={$_.layout.w}}, @{n='h';e={$_.layout.h}},
    @{n='iw';e={$_.style.intrinsicW}}, @{n='ih';e={$_.style.intrinsicH}} |
  Format-Table -AutoSize

Write-Host ""
Write-Host "===== STEP 6: DoD =====" -ForegroundColor Cyan
$all = $true
function Check($name, $ok) {
    if ($ok) { Write-Host "  PASS  $name" -ForegroundColor Green; return $true }
    else { Write-Host "  FAIL  $name" -ForegroundColor Red; return $false }
}

$imgs = @($json.nodes | Where-Object { $_.tag -eq 'img' })
$img1 = $imgs | Where-Object { $_.className -eq 'img1' }
$img2 = $imgs | Where-Object { $_.className -eq 'img2' }
$trs = @($json.nodes | Where-Object { $_.tag -eq 'tr' })
$tds = @($json.nodes | Where-Object { $_.tag -eq 'td' })

if (-not (Check "img1 intrinsic size" ($img1 -and [double]$img1.style.intrinsicW -gt 0))) { $all = $false }
if (-not (Check "img2 forced 100x50" ($img2 -and [double]$img2.layout.w -eq 100 -and [double]$img2.layout.h -eq 50))) { $all = $false }
if (-not (Check "table has 2 tr / 6 td" ($trs.Count -ge 2 -and $tds.Count -ge 6))) { $all = $false }

Write-Host ""
if ($all) { Write-Host "ITER3 PASS" -ForegroundColor Green }
else { Write-Host "ITER3 FAIL" -ForegroundColor Yellow }
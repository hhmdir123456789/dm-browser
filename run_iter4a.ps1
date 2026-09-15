$ErrorActionPreference = "Continue"
$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host ""
Write-Host "===== STEP 1: create test_iter4a.html =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  .box  { width: 100px; height: 30px; background-color: #cccccc; margin: 4px; }
  .wide { width: 100px; height: 30px; background-color: #cccccc; margin: 4px; }

  @media (min-width: 1200px) {
    .box { background-color: #ff0000; }
  }
  @media (max-width: 900px) {
    .box { background-color: #00ff00; }
  }
  @media print {
    .box { background-color: #000000; }
  }
  @media (min-width: 1000px) and (max-width: 1300px) {
    .wide { background-color: #0000ff; }
  }
</style>
</head>
<body>
  <div class="box">box</div>
  <div class="wide">wide</div>
</body>
</html>
'@
$html | Out-File -Encoding ASCII "$root\tests\pages\test_iter4a.html"
Write-Host "  created"

Write-Host ""
Write-Host "===== STEP 2: build =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "===== STEP 3: dump at vw=1156 =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" "$root\tests\pages\test_iter4a.html" 1156 753 --dump | Out-Null
Copy-Item "$root\tests\pages\test_iter4a.html.snapshot.json" "$root\tests\pages\test_iter4a_1156.json" -Force

Write-Host ""
Write-Host "===== STEP 4: dump at vw=800 =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" "$root\tests\pages\test_iter4a.html" 800 600 --dump | Out-Null
Copy-Item "$root\tests\pages\test_iter4a.html.snapshot.json" "$root\tests\pages\test_iter4a_800.json" -Force

Write-Host ""
Write-Host "===== STEP 5: checks =====" -ForegroundColor Cyan

function Check($name, $ok) {
    if ($ok) { Write-Host "  PASS  $name" -ForegroundColor Green; return $true }
    else { Write-Host "  FAIL  $name" -ForegroundColor Red; return $false }
}

$all = $true

$j = Get-Content "$root\tests\pages\test_iter4a_1156.json" -Encoding UTF8 -Raw | ConvertFrom-Json
$box  = $j.nodes | Where-Object { $_.className -eq 'box' }  | Select-Object -First 1
$wide = $j.nodes | Where-Object { $_.className -eq 'wide' } | Select-Object -First 1

if (-not (Check "vw=1156: .box bg default #ccc" ($box.style.backgroundColor -match '204,\s*204,\s*204')))  { $all = $false }
if (-not (Check "vw=1156: .wide bg #0000ff"    ($wide.style.backgroundColor -match '0,\s*0,\s*255')))       { $all = $false }

$j8 = Get-Content "$root\tests\pages\test_iter4a_800.json" -Encoding UTF8 -Raw | ConvertFrom-Json
$box8  = $j8.nodes | Where-Object { $_.className -eq 'box' }  | Select-Object -First 1
$wide8 = $j8.nodes | Where-Object { $_.className -eq 'wide' } | Select-Object -First 1

if (-not (Check "vw=800: .box bg #00ff00"          ($box8.style.backgroundColor  -match '0,\s*255,\s*0')))  { $all = $false }
if (-not (Check "vw=800: .wide bg default #ccc"    ($wide8.style.backgroundColor -match '204,\s*204,\s*204'))) { $all = $false }

Write-Host ""
if ($all) { Write-Host "ITER4A PASS" -ForegroundColor Green }
else { Write-Host "ITER4A FAIL" -ForegroundColor Yellow }
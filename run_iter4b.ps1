$ErrorActionPreference = "Continue"
$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host ""
Write-Host "===== STEP 1: create test_iter4b.html =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  .row { display: block; margin: 8px; height: 40px; }
  .red   { background-color: #ff0000; }
  .green { background-color: #00ff00; }
  .gray  { background-color: #808080; }

  .f-gray    { filter: grayscale(100%); }
  .f-gray50  { filter: grayscale(50%); }
  .f-bright  { filter: brightness(0.5); }
  .f-bright2 { filter: brightness(1.5); }
  .f-contrast{ filter: contrast(0.2); }
  .f-invert  { filter: invert(100%); }
  .f-multi   { filter: grayscale(100%) brightness(1.5); }
</style>
</head>
<body>
  <div class="row red">原色 red</div>
  <div class="row green">原色 green</div>
  <div class="row gray">原色 gray</div>
  <div class="row gray f-gray">grayscale 100%</div>
  <div class="row gray f-bright">brightness 0.5</div>
  <div class="row gray f-bright2">brightness 1.5</div>
  <div class="row gray f-contrast">contrast 0.2</div>
  <div class="row gray f-invert">invert 100%</div>
</body>
</html>
'@
$html | Out-File -Encoding ASCII "$root\tests\pages\test_iter4b.html"
Write-Host "  created"

Write-Host ""
Write-Host "===== STEP 2: build =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "===== STEP 3: dump =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" "$root\tests\pages\test_iter4b.html" 800 600 --dump

$snap = "$root\tests\pages\test_iter4b.html.snapshot.json"
if (-not (Test-Path $snap)) { Write-Host "SNAPSHOT MISSING" -ForegroundColor Red; exit 1 }

$json = Get-Content $snap -Encoding UTF8 -Raw | ConvertFrom-Json

Write-Host ""
Write-Host "===== STEP 4: filter captured =====" -ForegroundColor Cyan

$json.nodes | Where-Object { $_.className -match 'f-' } |
  Select-Object @{n='cls';e={$_.className}},
                @{n='filter';e={$_.style.filter}} |
  Format-Table -AutoSize

Write-Host ""
Write-Host "===== STEP 5: checks =====" -ForegroundColor Cyan

function Check($name, $ok) {
    if ($ok) { Write-Host "  PASS  $name" -ForegroundColor Green; return $true }
    else { Write-Host "  FAIL  $name" -ForegroundColor Red; return $false }
}

$all = $true

$f1 = $json.nodes | Where-Object { $_.className -match 'f-gray' -and $_.className -notmatch 'gray50' } | Select-Object -First 1
$f2 = $json.nodes | Where-Object { $_.className -match 'f-bright\b' } | Select-Object -First 1
$f3 = $json.nodes | Where-Object { $_.className -match 'f-invert' } | Select-Object -First 1

if (-not (Check "grayscale captured" ($f1 -and $f1.style.filter -match 'grayscale'))) { $all = $false }
if (-not (Check "brightness captured" ($f2 -and $f2.style.filter -match 'brightness'))) { $all = $false }
if (-not (Check "invert captured" ($f3 -and $f3.style.filter -match 'invert'))) { $all = $false }

Write-Host ""
if ($all) { Write-Host "ITER4B PASS" -ForegroundColor Green }
else { Write-Host "ITER4B FAIL" -ForegroundColor Yellow }
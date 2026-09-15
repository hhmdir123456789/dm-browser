# run_iter2.ps1 - ASCII only
$ErrorActionPreference = "Continue"

$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host ""
Write-Host "===== STEP 1: create test_iter2.html =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  ul.disc    { list-style: disc;    padding-left: 20px; margin: 4px 0; }
  ul.circle  { list-style: circle;  padding-left: 20px; margin: 4px 0; }
  ul.square  { list-style: square;  padding-left: 20px; margin: 4px 0; }
  ul.decimal { list-style: decimal; padding-left: 20px; margin: 4px 0; }
  .lg  { width: 200px; height: 60px; background: linear-gradient(to right, #ff0000, #0000ff); }
  .lgv { width: 200px; height: 60px; background: linear-gradient(to bottom, #00ff00, #000000); }
  .rg  { width: 120px; height: 120px; background: radial-gradient(#ffffff, #000000); }
  .ff  { font-family: "NoSuchFont_XXX", "Microsoft YaHei", sans-serif; font-size: 20px; }
  .lh-num    { line-height: 2; }
  .lh-normal { line-height: normal; }
  .lh-px     { line-height: 32px; }
</style>
</head>
<body>
  <ul class="disc"><li>disc one</li><li>disc two</li></ul>
  <ul class="circle"><li>circle one</li></ul>
  <ul class="square"><li>square one</li></ul>
  <ul class="decimal"><li>decimal one</li><li>decimal two</li></ul>
  <div class="lg">linear to right</div>
  <div class="lgv">linear to bottom</div>
  <div class="rg">radial</div>
  <div class="ff">font fallback test</div>
  <div class="lh-num">line-height 2</div>
  <div class="lh-normal">line-height normal</div>
  <div class="lh-px">line-height 32px</div>
</body>
</html>
'@

$htmlPath = "$root\tests\pages\test_iter2.html"
$html | Out-File -Encoding ASCII $htmlPath
Write-Host "  created: $htmlPath"

Write-Host ""
Write-Host "===== STEP 2: build =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}
Write-Host "BUILD OK" -ForegroundColor Green

Write-Host ""
Write-Host "===== STEP 3: dump test_iter2 =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" $htmlPath 1156 753 --dump

$snap = "$htmlPath.snapshot.json"
if (-not (Test-Path $snap)) {
    Write-Host "SNAPSHOT MISSING" -ForegroundColor Red
    exit 1
}

$json = Get-Content $snap -Encoding UTF8 -Raw | ConvertFrom-Json

Write-Host ""
Write-Host "===== STEP 4: list nodes =====" -ForegroundColor Cyan

$rows = @()
foreach ($n in $json.nodes) {
    if ($n.path -match '(ul|div)\.\d+') {
        $rows += [pscustomobject]@{
            path    = $n.path
            tag     = $n.tag
            display = $n.style.display
            lst     = $n.style.listStyleType
            bgImg   = if ($n.style.backgroundImage.Length -gt 40) {
                          $n.style.backgroundImage.Substring(0, 40) + "..."
                      } else { $n.style.backgroundImage }
            x       = $n.layout.x
            y       = $n.layout.y
            w       = $n.layout.w
            h       = $n.layout.h
        }
    }
}
$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "===== STEP 5: DoD checks =====" -ForegroundColor Cyan

function Check {
    param($name, $ok)
    if ($ok) {
        Write-Host ("  PASS  " + $name) -ForegroundColor Green
        return $true
    } else {
        Write-Host ("  FAIL  " + $name) -ForegroundColor Red
        return $false
    }
}

$all = $true

# 5.1 list-style 传递
$discLi    = $json.nodes | Where-Object { $_.path -match 'ul\.0>li\.0$' }
$circleLi  = $json.nodes | Where-Object { $_.path -match 'ul\.1>li\.0$' }
$squareLi  = $json.nodes | Where-Object { $_.path -match 'ul\.2>li\.0$' }
$decimalLi = $json.nodes | Where-Object { $_.path -match 'ul\.3>li\.0$' }

$ok51 = $discLi -and $discLi.style.listStyleType -eq 'disc' -and
        $circleLi -and $circleLi.style.listStyleType -eq 'circle' -and
        $squareLi -and $squareLi.style.listStyleType -eq 'square' -and
        $decimalLi -and $decimalLi.style.listStyleType -eq 'decimal'
if (-not (Check "list-style disc/circle/square/decimal propagated" $ok51)) { $all = $false }

# 5.2 linear-gradient 保留原串
$lg = $json.nodes | Where-Object { $_.className -eq 'lg' }
$ok52 = $lg -and ($lg.style.backgroundImage -match 'linear-gradient')
if (-not (Check "linear-gradient captured" $ok52)) { $all = $false }

# 5.3 radial-gradient 保留原串
$rg = $json.nodes | Where-Object { $_.className -eq 'rg' }
$ok53 = $rg -and ($rg.style.backgroundImage -match 'radial-gradient')
if (-not (Check "radial-gradient captured" $ok53)) { $all = $false }

# 5.4 font-family 回退（快照里没有 fontFamilyList 输出，只检查 ff 节点存在）
$ff = $json.nodes | Where-Object { $_.className -eq 'ff' }
$ok54 = $ff -and ($ff.layout.w -gt 0)
if (-not (Check "font fallback node laid out" $ok54)) { $all = $false }

# 5.5 line-height 三种类型（通过 h 推算）
$lnNum    = $json.nodes | Where-Object { $_.className -eq 'lh-num' }
$lnNormal = $json.nodes | Where-Object { $_.className -eq 'lh-normal' }
$lnPx     = $json.nodes | Where-Object { $_.className -eq 'lh-px' }

# lh-num: font-size 14, line-height 2 -> 28
# lh-normal: font-size 14, normal -> 21
# lh-px: 32
$ok55a = $lnNum    -and ([double]$lnNum.layout.h    -ge 27) -and ([double]$lnNum.layout.h    -le 29)
$ok55b = $lnNormal -and ([double]$lnNormal.layout.h -ge 20) -and ([double]$lnNormal.layout.h -le 22)
$ok55c = $lnPx     -and ([double]$lnPx.layout.h     -ge 31) -and ([double]$lnPx.layout.h     -le 33)
if (-not (Check "line-height 2 -> h=28" $ok55a)) { $all = $false }
if (-not (Check "line-height normal -> h=21" $ok55b)) { $all = $false }
if (-not (Check "line-height 32px -> h=32" $ok55c)) { $all = $false }

Write-Host ""
Write-Host "===== STEP 6: iter1 regression =====" -ForegroundColor Cyan
$iter1 = & "$root\build\Release\dm_render_test.exe" `
    "$root\tests\pages\test_iter1.html" 1156 753 --dump
$iter1Snap = "$root\tests\pages\test_iter1.html.snapshot.json"
if (Test-Path $iter1Snap) {
    $j1 = Get-Content $iter1Snap -Encoding UTF8 -Raw | ConvertFrom-Json
    $d0 = $j1.nodes | Where-Object { $_.path -match 'div\.0$' }
    $d4 = $j1.nodes | Where-Object { $_.path -match 'div\.4$' }
    $d5 = $j1.nodes | Where-Object { $_.path -match 'div\.5$' }
    $r1 = $d0 -and ($d0.style.backgroundColor -match '255,\s*224,\s*224')
    $r2 = $d4 -and ([double]$d4.style.opacity -gt 0.4) -and ([double]$d4.style.opacity -lt 0.6)
    $r3 = $d5 -and ($d5.style.fontStyle -eq 'italic')
    if (-not (Check "iter1: first-child bg" $r1)) { $all = $false }
    if (-not (Check "iter1: opacity 0.5" $r2)) { $all = $false }
    if (-not (Check "iter1: italic" $r3)) { $all = $false }
} else {
    Write-Host "  (iter1 snapshot missing, skip)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "===== SUMMARY =====" -ForegroundColor Cyan
if ($all) {
    Write-Host "ITER2 PASS" -ForegroundColor Green
} else {
    Write-Host "ITER2 FAIL" -ForegroundColor Yellow
}
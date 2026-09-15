# run_iter1.ps1
$ErrorActionPreference = "Stop"

$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host "`n===== 1. 生成 test_iter1.html =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  .item { padding: 8px; margin: 4px; background: #f0f0f0; }
  .item:first-child { background: #ffe0e0; }
  .item:not(.keep) { color: #c00; }
  .rel { position: relative; left: 20px; top: 10px; background: #e0f0ff; padding: 6px; }
  .op { opacity: 0.5; background: #333; color: #fff; padding: 10px; }
  .it { font-style: italic; }
</style>
</head>
<body>
  <div class="item keep">item 1 (first + keep)</div>
  <div class="item">item 2 (not keep)</div>
  <div class="item">item 3 (not keep)</div>
  <div class="rel">relative offset</div>
  <div class="op">opacity 0.5</div>
  <div class="it">italic text</div>
</body>
</html>
'@

$html | Out-File -Encoding UTF8 "$root\tests\pages\test_iter1.html"
Write-Host "  已生成 tests\pages\test_iter1.html"

Write-Host "`n===== 2. 编译 =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n编译失败，停止。" -ForegroundColor Red
    exit 1
}
Write-Host "  编译成功" -ForegroundColor Green

Write-Host "`n===== 3. 旧回归 test_render.html =====" -ForegroundColor Cyan

$old = & .\scripts\compare_engines.ps1 `
  -Url "http://localhost:8080/test_render.html" `
  -HtmlPath "tests\pages\test_render.html" `
  -OutDir "build\Release" `
  -Vw 1156 -Vh 753 2>&1

$old | Out-String | Write-Host

$oldText = $old | Out-String
$oldOk = ($oldText -match "结构相似度:\s*100%") -and
         ($oldText -match "样式相似度:\s*100%") -and
         ($oldText -match "布局相似度:\s*100%")

if ($oldOk) {
    Write-Host "  旧回归：通过" -ForegroundColor Green
} else {
    Write-Host "  旧回归：未通过" -ForegroundColor Red
}

Write-Host "`n===== 4. 迭代 1 新用例 =====" -ForegroundColor Cyan

.\build\Release\dm_render_test.exe `
  "$root\tests\pages\test_iter1.html" 1156 753 --dump

$snap = "$root\tests\pages\test_iter1.html.snapshot.json"
if (-not (Test-Path $snap)) {
    Write-Host "  快照未生成" -ForegroundColor Red
    exit 1
}
Write-Host "  快照：$snap"

$json = Get-Content $snap -Encoding UTF8 -Raw | ConvertFrom-Json

Write-Host "`n--- 关键节点 ---" -ForegroundColor Cyan
$json.nodes | Where-Object {
    $_.path -match "div\.[0-9]"
} | Select-Object `
    path, tag,
    @{n='bg';e={$_.style.backgroundColor}},
    @{n='color';e={$_.style.color}},
    @{n='opacity';e={$_.style.opacity}},
    @{n='x';e={$_.layout.x}},
    @{n='y';e={$_.layout.y}},
    @{n='w';e={$_.layout.w}},
    @{n='h';e={$_.layout.h}} |
  Format-Table -AutoSize

Write-Host "`n===== 5. 迭代 1 DoD 检查 =====" -ForegroundColor Cyan

$divs = $json.nodes | Where-Object { $_.path -match "div\.[0-9]" }
$checks = @()

# 5.1 first-child 只命中第 1 个
$first = $divs | Where-Object { $_.path -match "div\.0$" }
$second = $divs | Where-Object { $_.path -match "div\.1$" }
$third = $divs | Where-Object { $_.path -match "div\.2$" }
$firstBgOk = $first -and ($first.style.backgroundColor -like "*255,224,224*")
$secondBgOk = $second -and ($second.style.backgroundColor -notlike "*255,224,224*")
$checks += [pscustomobject]@{
    DoD = ":first-child 只命中第 1 个"
    Result = if ($firstBgOk -and $secondBgOk) { "PASS" } else { "FAIL" }
}

# 5.2 :not(.keep) 命中 2、3，不命中 1
$firstColorOk = $first -and ($first.style.color -like "*51,51,51*")
$secondColorOk = $second -and ($second.style.color -like "*204,0,0*")
$thirdColorOk = $third -and ($third.style.color -like "*204,0,0*")
$checks += [pscustomobject]@{
    DoD = ":not(.keep) 命中 2、3，不命中 1"
    Result = if ($firstColorOk -and $secondColorOk -and $thirdColorOk) { "PASS" } else { "FAIL" }
}

# 5.3 relative 偏移
$rel = $divs | Where-Object { $_.path -match "div\.3$" }
$relOk = $rel -and ([double]$rel.layout.x -ge 20) -and ([double]$rel.layout.y -ge 10)
$checks += [pscustomobject]@{
    DoD = "position:relative 偏移生效"
    Result = if ($relOk) { "PASS" } else { "FAIL" }
}

# 5.4 opacity
$op = $divs | Where-Object { $_.path -match "div\.4$" }
$opVal = if ($op) { [double]$op.style.opacity } else { 1 }
$opOk = $op -and ($opVal -gt 0.4 -and $opVal -lt 0.6)
$checks += [pscustomobject]@{
    DoD = "opacity 0.5 生效"
    Result = if ($opOk) { "PASS" } else { "FAIL" }
}

# 5.5 italic
$it = $divs | Where-Object { $_.path -match "div\.5$" }
$itOk = $it -and ($it.style.fontStyle -eq "italic")
$checks += [pscustomobject]@{
    DoD = "font-style:italic 生效"
    Result = if ($itOk) { "PASS" } else { "FAIL" }
}

$checks | Format-Table -AutoSize

$failCount = ($checks | Where-Object { $_.Result -eq "FAIL" }).Count

Write-Host "`n===== 汇总 =====" -ForegroundColor Cyan
Write-Host "  旧回归：$(if ($oldOk) { 'PASS' } else { 'FAIL' })"
Write-Host "  新用例：$(if ($failCount -eq 0) { 'PASS' } else { "FAIL ($failCount 项)" })"

if ($oldOk -and $failCount -eq 0) {
    Write-Host "`n迭代 1 通过，可以进入迭代 2。" -ForegroundColor Green
} else {
    Write-Host "`n迭代 1 未通过，请把上面的输出贴回来。" -ForegroundColor Yellow
}
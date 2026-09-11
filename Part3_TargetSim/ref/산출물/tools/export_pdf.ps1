# pptx -> PDF + 슬라이드 PNG (PowerPoint COM). 이 PC 의 PowerPoint 2010 으로 확인함.
#
#   powershell -ExecutionPolicy Bypass -File export_pdf.ps1 [-Png]
#
# PDF 는 pptx 옆에 같은 이름으로, PNG 는 산출물\render\ 에 "슬라이드N.PNG" 로 나온다.
# 이미 PowerPoint 가 열려 있으면(다른 발표자료 편집 중) 프로그램을 종료하지 않는다.
param([switch]$Png)

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = (Resolve-Path (Join-Path $here "..")).Path
$pptx = Join-Path $out "Chapter3_target_simulation.pptx"
$pdf = Join-Path $out "Chapter3_target_simulation.pdf"
$render = Join-Path $out "render"

$app = New-Object -ComObject PowerPoint.Application
$n = $app.Presentations.Count
$p = $app.Presentations.Open($pptx, $true, $false, $false)
$p.SaveAs($pdf, 32)
"PDF: $pdf"
if ($Png) {
    if (Test-Path $render) { Remove-Item (Join-Path $render "*.PNG") -Force -ErrorAction SilentlyContinue }
    $p.Export($render, "PNG", 1600, 900)
    "PNG: $render"
}
$p.Close()
if ($n -eq 0) { $app.Quit() }

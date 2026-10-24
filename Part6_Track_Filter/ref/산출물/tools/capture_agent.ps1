# Build / run / capture helper for the Part 6 deck (Windows PowerShell 5+).
# Watches request.txt next to this script. One command per request:
#   build                  -> MSBuild Release|x64 of src\TargetSim\TargetSim.sln (log: build_log.txt)
#   run                    -> start build\x64\Release\TargetSimUI.exe and wait for its first run to finish
#   deck                   -> run (if needed) + every program screen the deck uses -> ..\figures\ui_<name>.png + ui_<name>.json
#   capture <name>         -> main window PNG ..\figures\ui_<name>.png + control rectangles ui_<name>.json (other windows: ui_<name>_<n>.png)
#   kill                   -> close TargetSimUI
#   size <w> <h>           -> resize the main window (pixels)
#   scenario <0|1>         -> scenario combo (0 ship, 1 air), runs again
#   given | tuned          -> parameter buttons (task values / tuned values), runs again
#   param <r> <az> <el> <q> <tau> + apply -> type the filter parameters, then press "apply / run again"
#   view <0..4>            -> view combo (3D + 2D / 3D / 2D / map / tracking performance)
#   scan <n>               -> move the scan slider to scan n
#   wait                   -> wait until the current run has processed every scan
#   quit                   -> stop this helper
# UI commands drive the app's own controls with window messages (control IDs read from TargetSimUI\Resource.h).
# The .json next to each capture holds control rectangles in image pixels; build_deck.py crops the plots with them.
$ErrorActionPreference = 'Continue'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Resolve-Path (Join-Path $Here '..\..\..\src\TargetSim')
$Fig = Join-Path $Here '..\figures'
New-Item -ItemType Directory -Force -Path $Fig | Out-Null
$Req = Join-Path $Here 'request.txt'
$Log = Join-Path $Here 'agent_log.txt'

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public class W32 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int s);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendText(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr GetText(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static List<IntPtr> Windows(uint pid) {
    var list = new List<IntPtr>();
    EnumWindows((h, p) => { uint q; GetWindowThreadProcessId(h, out q); if (q == pid && IsWindowVisible(h)) list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }
}
"@
[W32]::SetProcessDPIAware() | Out-Null

# Control IDs from the UI's Resource.h (IDC_* only)
$Ids = @{}
foreach ($l in Get-Content (Join-Path $Root 'TargetSimUI\Resource.h')) {
  if ($l -match '^#define\s+(IDC_\w+)\s+(\d+)') { $Ids[$Matches[1]] = [int]$Matches[2] }
}
# Summary text: "(<done>)" and "<processed> N / N" (Korean words as code points so this file stays ASCII)
$Done = '(' + [char]0xB05D + ')'
$Processed = [string][char]0xCC98 + [char]0xB9AC + ' (\d+) / (\d+)'

function Write-Log($m) { Add-Content -Path $Log -Value ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) -Encoding UTF8 }

function Do-Build {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
  Write-Log "msbuild: $msbuild"
  $out = & $msbuild (Join-Path $Root 'TargetSim.sln') /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo 2>&1
  $out | Out-File -FilePath (Join-Path $Here 'build_log.txt') -Encoding UTF8
  Add-Content -Path (Join-Path $Here 'build_log.txt') -Value ("exit=" + $LASTEXITCODE) -Encoding UTF8
  Write-Log "build exit $LASTEXITCODE"
}

function Find-Win($ctrlId) {
  $p = Get-Process TargetSimUI -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) { return [IntPtr]::Zero }
  foreach ($h in [W32]::Windows([uint32]$p.Id)) { if ([W32]::GetDlgItem($h, $ctrlId) -ne [IntPtr]::Zero) { return $h } }
  return [IntPtr]::Zero
}
function Main-Win { return Find-Win $Ids['IDC_VIEW_MODE'] }

# Window image origin on screen: the DWM frame (no invisible resize border), else the window rectangle
function Get-Frame($h) {
  $r = New-Object W32+RECT
  [W32]::GetWindowRect($h, [ref]$r) | Out-Null
  $f = New-Object W32+RECT
  if ([W32]::DwmGetWindowAttribute($h, 9, [ref]$f, 16) -eq 0 -and ($f.R - $f.L) -gt 0) { return @($r, $f) }
  return @($r, $r)
}

function Save-Window($h, $file) {
  $rf = Get-Frame $h; $r = $rf[0]; $f = $rf[1]
  $w = $r.R - $r.L; $hh = $r.B - $r.T
  $bmp = New-Object System.Drawing.Bitmap $w, $hh
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  [W32]::PrintWindow($h, $hdc, 2) | Out-Null
  $g.ReleaseHdc($hdc); $g.Dispose()
  $cx = $f.L - $r.L; $cy = $f.T - $r.T; $cw = $f.R - $f.L; $ch = $f.B - $f.T
  if ($cw -gt 0 -and $ch -gt 0 -and ($cx + $cw) -le $w -and ($cy + $ch) -le $hh -and ($cx -ne 0 -or $cy -ne 0 -or $cw -ne $w -or $ch -ne $hh)) {
    $crop = $bmp.Clone((New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch), $bmp.PixelFormat); $bmp.Dispose(); $bmp = $crop
  }
  $bmp.Save((Join-Path $Fig $file), [System.Drawing.Imaging.ImageFormat]::Png)
  $size = @($bmp.Width, $bmp.Height); $bmp.Dispose()
  return @($f, $size)
}

function Do-Capture($name) {
  $main = Main-Win
  if ($main -eq [IntPtr]::Zero) { Write-Log "capture: no main window"; return }
  $res = Save-Window $main "ui_$name.png"; $f = $res[0]; $size = $res[1]
  # visible controls -> [x, y, w, h] in image pixels (same names as Resource.h; the deck crops the plots with them)
  $items = @()
  foreach ($k in ($Ids.Keys | Sort-Object)) {
    $c = [W32]::GetDlgItem($main, $Ids[$k])
    if ($c -eq [IntPtr]::Zero -or -not [W32]::IsWindowVisible($c)) { continue }
    $cr = New-Object W32+RECT
    [W32]::GetWindowRect($c, [ref]$cr) | Out-Null
    $items += ('  "{0}": [{1}, {2}, {3}, {4}]' -f $k, ($cr.L - $f.L), ($cr.T - $f.T), ($cr.R - $cr.L), ($cr.B - $cr.T))
  }
  $json = "{`n `"source`": `"capture`", `"w`": $($size[0]), `"h`": $($size[1]),`n `"rects`": {`n" + ($items -join ",`n") + "`n }`n}`n"
  [System.IO.File]::WriteAllText((Join-Path $Fig "ui_$name.json"), $json, (New-Object System.Text.UTF8Encoding $false))
  Write-Log "captured ui_$name.png ($($size[0]) x $($size[1])), $($items.Count) controls"
  # other visible windows of the program (e.g. the score dialog)
  $p = Get-Process TargetSimUI -ErrorAction SilentlyContinue | Select-Object -First 1
  $i = 1
  foreach ($h in [W32]::Windows([uint32]$p.Id)) {
    if ($h -eq $main) { continue }
    $r = New-Object W32+RECT; [W32]::GetWindowRect($h, [ref]$r) | Out-Null
    if (($r.R - $r.L) -lt 50 -or ($r.B - $r.T) -lt 50) { continue }
    Save-Window $h "ui_${name}_$i.png" | Out-Null; Write-Log "captured ui_${name}_$i.png"; $i++
  }
}

function Set-Combo($dlg, $id, $n) {
  $c = [W32]::GetDlgItem($dlg, $id)
  [W32]::SendMessage($c, 0x014E, [IntPtr]$n, [IntPtr]::Zero) | Out-Null                       # CB_SETCURSEL
  $wp = [IntPtr](($id -band 0xFFFF) -bor (1 -shl 16))                                          # CBN_SELCHANGE
  [W32]::SendMessage($dlg, 0x0111, $wp, $c) | Out-Null                                         # WM_COMMAND
}
function Click($dlg, $id) {
  # posted: a wrong parameter opens a message box, which must not block this helper
  [W32]::PostMessage($dlg, 0x0111, [IntPtr]$id, [W32]::GetDlgItem($dlg, $id)) | Out-Null       # WM_COMMAND BN_CLICKED
}
function Get-Summary($dlg) {
  $c = [W32]::GetDlgItem($dlg, $Ids['IDC_TRK_SUMMARY'])
  $n = ([W32]::SendMessage($c, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)).ToInt32()             # WM_GETTEXTLENGTH
  $sb = New-Object System.Text.StringBuilder ($n + 1)
  [W32]::GetText($c, 0x000D, [IntPtr]($n + 1), $sb) | Out-Null                               # WM_GETTEXT
  return $sb.ToString()
}
function Wait-Run($timeoutSec = 60) {
  Start-Sleep -Milliseconds 600                                                                # let the new run replace the old summary
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt $timeoutSec) {
    $main = Main-Win
    if ($main -ne [IntPtr]::Zero) {
      $s = Get-Summary $main
      if ($s.Contains($Done) -and $s -match $Processed -and [int]$Matches[1] -gt 0 -and $Matches[1] -eq $Matches[2]) {
        Start-Sleep -Milliseconds 300; Write-Log "run finished ($($Matches[1]) scans)"; return $true
      }
    }
    Start-Sleep -Milliseconds 200
  }
  Write-Log "wait: timeout"; return $false
}
function Start-App {
  if (-not (Get-Process TargetSimUI -ErrorAction SilentlyContinue)) {
    Start-Process -FilePath (Join-Path $Root 'build\x64\Release\TargetSimUI.exe') -WorkingDirectory (Join-Path $Root 'build\x64\Release')
    $t0 = Get-Date
    while ((Main-Win) -eq [IntPtr]::Zero -and ((Get-Date) - $t0).TotalSeconds -lt 20) { Start-Sleep -Milliseconds 200 }
    Write-Log "started"
    Wait-Run | Out-Null
  }
}

function Do-Ui($verb, $arg) {
  $main = Main-Win
  if ($main -eq [IntPtr]::Zero) { Write-Log "ui ${verb}: no main window"; return }
  switch ($verb) {
    'size'     { $wh = $arg -split '\s+'; [W32]::SetWindowPos($main, [IntPtr]::Zero, 40, 40, [int]$wh[0], [int]$wh[1], 0x0004) | Out-Null }
    'scenario' { Set-Combo $main $Ids['IDC_SCN_COMBO'] ([int]$arg) }
    'view'     { Set-Combo $main $Ids['IDC_VIEW_MODE'] ([int]$arg) }
    'given'    { Click $main $Ids['IDC_PRM_GIVEN'] }
    'tuned'    { Click $main $Ids['IDC_PRM_TUNED'] }
    'apply'    { Click $main $Ids['IDC_TRK_RESEND'] }
    'param'    {
      $v = $arg -split '\s+'
      $edits = 'IDC_PRM_SIGR', 'IDC_PRM_SIGAZ', 'IDC_PRM_SIGEL', 'IDC_PRM_Q', 'IDC_PRM_TAU'
      for ($k = 0; $k -lt [Math]::Min($v.Count, 5); $k++) {
        [W32]::SendText([W32]::GetDlgItem($main, $Ids[$edits[$k]]), 0x000C, [IntPtr]::Zero, $v[$k]) | Out-Null   # WM_SETTEXT
      }
    }
    'scan'     {
      $sl = [W32]::GetDlgItem($main, $Ids['IDC_SCAN_SLIDER'])
      [W32]::SendMessage($sl, 0x0405, [IntPtr]1, [IntPtr]([int]$arg)) | Out-Null               # TBM_SETPOS
      $wp = [IntPtr](4 -bor ([int]$arg -shl 16))                                                # TB_THUMBPOSITION
      [W32]::SendMessage($main, 0x0114, $wp, $sl) | Out-Null                                    # WM_HSCROLL
    }
  }
  Write-Log "ui $verb $arg"
}

# Every program screen the deck uses (names = build_deck.py ui_shot names; same set as the Linux previews pv_*)
function Do-Deck {
  Start-App
  $plan = @(
    @{ s = 0; p = 'given'; shots = @(@(0, 'ship_given_all'), @(4, 'ship_given_perf')) },
    @{ s = 0; p = 'tuned'; shots = @(@(0, 'ship_tuned_all'), @(4, 'ship_tuned_perf')) },
    @{ s = 1; p = 'given'; shots = @(@(0, 'air_given_all'), @(4, 'air_given_perf'), @(3, 'air_given_map')) },
    @{ s = 1; p = 'tuned'; shots = @(@(0, 'air_tuned_all'), @(4, 'air_tuned_perf')) })
  foreach ($step in $plan) {
    Do-Ui 'scenario' $step.s
    Start-Sleep -Milliseconds 300
    Do-Ui $step.p ''
    if (-not (Wait-Run)) { Write-Log "deck: stopped (run did not finish)"; return }
    foreach ($shot in $step.shots) {
      Do-Ui 'view' $shot[0]
      Start-Sleep -Milliseconds 500
      Do-Capture $shot[1]
    }
  }
  Do-Ui 'view' 0
  Write-Log "deck: done"
}

Write-Log "agent started, root $Root"
while ($true) {
  if (Test-Path $Req) {
    $cmd = (Get-Content $Req -Raw -Encoding UTF8).Trim()
    Remove-Item $Req -Force
    Write-Log "request: $cmd"
    $parts = $cmd -split '\s+', 2
    $arg = if ($parts.Count -gt 1) { $parts[1] } else { '' }
    switch ($parts[0]) {
      'build'   { Do-Build }
      'run'     { Start-App }
      'deck'    { Do-Deck }
      'capture' { Do-Capture $arg }
      'wait'    { Wait-Run | Out-Null }
      'kill'    { Get-Process TargetSimUI -ErrorAction SilentlyContinue | Stop-Process -Force; Write-Log "killed" }
      'quit'    { Write-Log "bye"; exit }
      { $_ -in 'size', 'scenario', 'view', 'given', 'tuned', 'apply', 'param', 'scan' } { Do-Ui $parts[0] $arg }
      default   { Write-Log "unknown: $cmd" }
    }
    Write-Log "done: $cmd"
  }
  Start-Sleep -Milliseconds 300
}

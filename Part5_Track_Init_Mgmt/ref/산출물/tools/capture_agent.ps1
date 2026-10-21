# Build / run / capture helper for the Part 5 deck (Windows PowerShell 5+).
# Watches request.txt next to this script. One command per request:
#   build            -> MSBuild Release|x64 of src\TargetSim\TargetSim.sln (log: build_log.txt)
#   run              -> start build\x64\Release\TargetSimUI.exe
#   capture <name>   -> PNG of every visible top-level window of TargetSimUI into ..\figures\ui_<name>[_n].png
#   kill             -> close TargetSimUI
#   size <w> <h>     -> resize the main window (pixels)
#   view <0|1|2>     -> main window view combo (2D + 3D / 2D / 3D)
#   scan <n>         -> move the scan slider to scan n
#   score            -> press the score-check button (opens the score dialog)
#   preset <n>       -> score dialog preset combo
#   closescore       -> close the score dialog
# UI commands drive the app's own controls with window messages (control IDs from TargetSimUI\Resource.h).
#   quit             -> stop this helper
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

function Do-Capture($name) {
  $p = Get-Process TargetSimUI -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) { Write-Log "capture: no process"; return }
  $i = 0
  foreach ($h in [W32]::Windows([uint32]$p.Id)) {
    $r = New-Object W32+RECT
    [W32]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    if ($w -lt 50 -or $hh -lt 50) { continue }
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [W32]::PrintWindow($h, $hdc, 2) | Out-Null
    $g.ReleaseHdc($hdc); $g.Dispose()
    # trim the invisible resize border DWM adds around the window
    $f = New-Object W32+RECT
    if ([W32]::DwmGetWindowAttribute($h, 9, [ref]$f, 16) -eq 0) {
      $cx = $f.L - $r.L; $cy = $f.T - $r.T; $cw = $f.R - $f.L; $ch = $f.B - $f.T
      if ($cw -gt 0 -and $ch -gt 0 -and ($cx + $cw) -le $w -and ($cy + $ch) -le $hh) {
        $crop = $bmp.Clone((New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch), $bmp.PixelFormat); $bmp.Dispose(); $bmp = $crop
      }
    }
    $file = if ($i -eq 0) { "ui_$name.png" } else { "ui_${name}_$i.png" }
    $bmp.Save((Join-Path $Fig $file), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Write-Log "captured $file ($w x $hh)"
    $i++
  }
}

function Find-Win($ctrlId) {
  $p = Get-Process TargetSimUI -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) { return [IntPtr]::Zero }
  foreach ($h in [W32]::Windows([uint32]$p.Id)) { if ([W32]::GetDlgItem($h, $ctrlId) -ne [IntPtr]::Zero) { return $h } }
  return [IntPtr]::Zero
}
function Set-Combo($dlg, $id, $n) {
  $c = [W32]::GetDlgItem($dlg, $id)
  [W32]::SendMessage($c, 0x014E, [IntPtr]$n, [IntPtr]::Zero) | Out-Null                       # CB_SETCURSEL
  $wp = [IntPtr](($id -band 0xFFFF) -bor (1 -shl 16))                                          # CBN_SELCHANGE
  [W32]::SendMessage($dlg, 0x0111, $wp, $c) | Out-Null                                         # WM_COMMAND
}
function Do-Ui($verb, $arg) {
  $main = Find-Win 1064
  switch ($verb) {
    'size' { $wh = $arg -split '\s+'; [W32]::SetWindowPos($main, [IntPtr]::Zero, 40, 40, [int]$wh[0], [int]$wh[1], 0x0004) | Out-Null }
    'view' { Set-Combo $main 1064 ([int]$arg) }
    'scan' {
      $sl = [W32]::GetDlgItem($main, 1062)
      [W32]::SendMessage($sl, 0x0405, [IntPtr]1, [IntPtr]([int]$arg)) | Out-Null               # TBM_SETPOS
      $wp = [IntPtr](4 -bor ([int]$arg -shl 16))                                                # TB_THUMBPOSITION
      [W32]::SendMessage($main, 0x0114, $wp, $sl) | Out-Null                                    # WM_HSCROLL
    }
    'wheel' {
      # wheel <x> <y> <delta>: x, y relative to the captured window image (DWM frame), delta in wheel units (120 = one notch)
      $a = $arg -split '\s+'; $f = New-Object W32+RECT
      [W32]::DwmGetWindowAttribute($main, 9, [ref]$f, 16) | Out-Null
      $sx = $f.L + [int]$a[0]; $sy = $f.T + [int]$a[1]
      $wp = [IntPtr]((([int]$a[2]) -band 0xFFFF) -shl 16)
      $lp = [IntPtr]((($sy -band 0xFFFF) -shl 16) -bor ($sx -band 0xFFFF))
      [W32]::SendMessage($main, 0x020A, $wp, $lp) | Out-Null
    }
    'score' { [W32]::PostMessage($main, 0x0111, [IntPtr]1111, [W32]::GetDlgItem($main, 1111)) | Out-Null }   # WM_COMMAND BN_CLICKED (posted: the dialog is modal)
    'preset' { $d = Find-Win 1122; if ($d -ne [IntPtr]::Zero) { Set-Combo $d 1122 ([int]$arg) } }
    'closescore' { $d = Find-Win 1122; if ($d -ne [IntPtr]::Zero) { [W32]::PostMessage($d, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null } }  # WM_CLOSE
  }
  Write-Log "ui $verb $arg (main $main)"
}

Write-Log "agent started, root $Root"
while ($true) {
  if (Test-Path $Req) {
    $cmd = (Get-Content $Req -Raw -Encoding UTF8).Trim()
    Remove-Item $Req -Force
    Write-Log "request: $cmd"
    $parts = $cmd -split '\s+', 2
    switch ($parts[0]) {
      'build'   { Do-Build }
      'run'     { Start-Process -FilePath (Join-Path $Root 'build\x64\Release\TargetSimUI.exe') -WorkingDirectory (Join-Path $Root 'build\x64\Release'); Write-Log "started" }
      'capture' { Do-Capture $parts[1] }
      'kill'    { Get-Process TargetSimUI -ErrorAction SilentlyContinue | Stop-Process -Force; Write-Log "killed" }
      'quit'    { Write-Log "bye"; exit }
      { $_ -in 'size','view','scan','wheel','score','preset','closescore' } { Do-Ui $parts[0] $parts[1] }
      default   { Write-Log "unknown: $cmd" }
    }
    Write-Log "done: $cmd"
  }
  Start-Sleep -Milliseconds 300
}

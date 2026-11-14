param(
  [Parameter(Mandatory)] [string]$Game,
  [int]$Runs = 3
)
$ErrorActionPreference = 'Stop'

$sleepCalls = @{
  '89CF90A333E6E61B0D26AD67C3FA9F906D38BB3D32006A269F74B6AA6D5741F0' = 0x75B0D
  'BFC18DD1E46F58AEF450CD7E81C9899B9A97694C6F81A38863916C2FE1C2677F' = 0x71FD6
}
$exe = Join-Path $Game 'quakespasm.exe'
$offset = $sleepCalls[(Get-FileHash $exe -Algorithm SHA256).Hash]
if ($null -eq $offset) {
  throw "$exe is not QuakeSpasm 0.97.0 for Windows"
}
$bytes = [IO.File]::ReadAllBytes($exe)
[Array]::Copy([byte[]](0x0F, 0x1F, 0x44, 0x00, 0x00), 0, $bytes, $offset, 5)
$bench = Join-Path $Game 'quakespasm-bench.exe'
[IO.File]::WriteAllBytes($bench, $bytes)

if (-not ('HiddenLauncher' -as [type])) {
  Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class HiddenLauncher {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  struct StartupInfo {
    public int cb;
    public string reserved, desktop, title;
    public int x, y, width, height, xchars, ychars, fill, flags;
    public short show, reserved2;
    public IntPtr reserved3, input, output, error;
  }

  [StructLayout(LayoutKind.Sequential)]
  struct ProcessInfo {
    public IntPtr process, thread;
    public int pid, tid;
  }

  [StructLayout(LayoutKind.Sequential)]
  struct PowerThrottling {
    public uint version, control, state;
  }

  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern IntPtr CreateDesktopW(string name, IntPtr device, IntPtr mode, int flags, uint access, IntPtr security);
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern bool CreateProcessW(string app, string command, IntPtr processAttributes, IntPtr threadAttributes,
                                    bool inherit, int flags, IntPtr environment, string directory,
                                    ref StartupInfo startup, out ProcessInfo info);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool SetProcessInformation(IntPtr process, int type, ref PowerThrottling info, int size);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool SetProcessAffinityMask(IntPtr process, IntPtr mask);
  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool GetLogicalProcessorInformationEx(int relation, IntPtr buffer, ref int length);
  [DllImport("kernel32.dll")]
  static extern uint ResumeThread(IntPtr thread);
  [DllImport("kernel32.dll")]
  static extern bool CloseHandle(IntPtr handle);

  static IntPtr PerformanceCores() {
    int length = 0;
    GetLogicalProcessorInformationEx(0, IntPtr.Zero, ref length);
    IntPtr buffer = Marshal.AllocHGlobal(length);
    try {
      if (!GetLogicalProcessorInformationEx(0, buffer, ref length)) {
        throw new Win32Exception();
      }
      byte best = 0;
      long mask = 0;
      for (int offset = 0; offset < length; offset += Marshal.ReadInt32(buffer, offset + 4)) {
        byte efficiency = Marshal.ReadByte(buffer, offset + 9);
        if (efficiency > best) {
          best = efficiency;
          mask = 0;
        }
        if (efficiency == best) {
          mask |= Marshal.ReadIntPtr(buffer, offset + 32).ToInt64();
        }
      }
      return new IntPtr(mask);
    } finally {
      Marshal.FreeHGlobal(buffer);
    }
  }

  public static int Start(string exe, string arguments, string directory) {
    if (CreateDesktopW("softgpu-bench", IntPtr.Zero, IntPtr.Zero, 0, 0x10000000, IntPtr.Zero) == IntPtr.Zero) {
      throw new Win32Exception();
    }
    StartupInfo startup = new StartupInfo();
    startup.cb = Marshal.SizeOf(startup);
    startup.desktop = "softgpu-bench";
    ProcessInfo info;
    if (!CreateProcessW(exe, "\"" + exe + "\" " + arguments, IntPtr.Zero, IntPtr.Zero, false, 4, IntPtr.Zero,
                        directory, ref startup, out info)) {
      throw new Win32Exception();
    }
    PowerThrottling throttling = new PowerThrottling();
    throttling.version = 1;
    throttling.control = 1;
    if (!SetProcessInformation(info.process, 4, ref throttling, Marshal.SizeOf(throttling)) ||
        !SetProcessAffinityMask(info.process, PerformanceCores())) {
      throw new Win32Exception();
    }
    ResumeThread(info.thread);
    CloseHandle(info.thread);
    CloseHandle(info.process);
    return info.pid;
  }
}
'@
}

$configs = 'autoexec.cfg', 'config.cfg'
$saved = @{}
foreach ($name in $configs) {
  $path = Join-Path $Game "id1\$name"
  if (Test-Path $path) {
    $saved[$name] = [IO.File]::ReadAllBytes($path)
  }
}
$log = Join-Path $Game 'qconsole.log'
$arguments = '-condebug -window -width 640 -height 480 -nosound -nomouse -nojoy'
$process = $null
try {
  Set-Content (Join-Path $Game 'id1\autoexec.cfg') 'timedemo demo1'
  Remove-Item (Join-Path $Game 'id1\config.cfg') -ErrorAction SilentlyContinue
  foreach ($run in 1..$Runs) {
    Remove-Item $log -ErrorAction SilentlyContinue
    $process = Get-Process -Id ([HiddenLauncher]::Start($bench, $arguments, $Game))
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $result = $null
    while (-not $result -and -not $process.HasExited -and $clock.Elapsed.TotalSeconds -lt 300) {
      Start-Sleep -Milliseconds 200
      if (Test-Path $log) {
        $result = Select-String -Path $log -Pattern '\d+ frames\s+[\d.]+ seconds\s+[\d.]+ fps' |
          Select-Object -First 1
      }
    }
    Stop-Process $process -Force -ErrorAction SilentlyContinue
    $process.WaitForExit()
    if (-not $result) {
      throw 'The timedemo did not finish'
    }
    if (-not (Select-String -Path $log -Pattern 'GL_RENDERER: softgpu' -Quiet)) {
      throw 'QuakeSpasm did not use softgpu'
    }
    $result.Matches[0].Value
  }
} finally {
  if ($process -and -not $process.HasExited) {
    Stop-Process $process -Force
    $process.WaitForExit()
  }
  foreach ($name in $configs) {
    $path = Join-Path $Game "id1\$name"
    if ($saved.ContainsKey($name)) {
      [IO.File]::WriteAllBytes($path, $saved[$name])
    } else {
      Remove-Item $path -ErrorAction SilentlyContinue
    }
  }
}

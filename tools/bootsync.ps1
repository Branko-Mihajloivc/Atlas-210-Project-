param([string]$Port = 'COM17', [int]$Baud = 115200)
$src = @'
using System; using System.Runtime.InteropServices; using System.Text; using Microsoft.Win32.SafeHandles;
public static class BootSync {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern SafeFileHandle CreateFile(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadFile(SafeFileHandle h, byte[] b, int n, out int r, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteFile(SafeFileHandle h, byte[] b, int n, out int w, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetCommTimeouts(SafeFileHandle h, ref COMMTIMEOUTS t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool EscapeCommFunction(SafeFileHandle h, uint f);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool PurgeComm(SafeFileHandle h, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct COMMTIMEOUTS { public uint RI, RTM, RTC, WTM, WTC; }
  public static string Run(string port) {
    var h = CreateFile("\\\\.\\" + port, 0xC0000000, 0, IntPtr.Zero, 3, 0, IntPtr.Zero);
    if (h.IsInvalid) return "CreateFile failed, err " + Marshal.GetLastWin32Error();
    var t = new COMMTIMEOUTS { RI = 0xFFFFFFFF, RTM = 0, RTC = 50, WTC = 500 };
    SetCommTimeouts(h, ref t);
    // reset: DTR+RTS low then high
    EscapeCommFunction(h, 6); EscapeCommFunction(h, 4); System.Threading.Thread.Sleep(250);
    EscapeCommFunction(h, 5); EscapeCommFunction(h, 3); System.Threading.Thread.Sleep(50);
    PurgeComm(h, 0x0F);
    var sb = new StringBuilder(); var cmd = new byte[] { 0x30, 0x20 }; var buf = new byte[64];
    for (int i = 0; i < 10; i++) {
      int w, r; WriteFile(h, cmd, 2, out w, IntPtr.Zero);
      System.Threading.Thread.Sleep(50);
      ReadFile(h, buf, buf.Length, out r, IntPtr.Zero);
      sb.Append("try " + i + ": ");
      for (int k = 0; k < r; k++) sb.Append(buf[k].ToString("X2") + " ");
      sb.Append("\n");
    }
    h.Close(); return sb.ToString();
  }
}
'@
Add-Type -TypeDefinition $src
mode $Port BAUD=$Baud PARITY=n DATA=8 STOP=1 dtr=off rts=off | Out-Null
"---- bootloader sync $Port @ $Baud ----"
[BootSync]::Run($Port)

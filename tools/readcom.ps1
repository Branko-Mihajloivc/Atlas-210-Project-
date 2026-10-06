param([string]$Port = 'COM17', [int]$Baud = 9600, [int]$Seconds = 8)
$src = @'
using System; using System.Runtime.InteropServices; using System.Text; using Microsoft.Win32.SafeHandles;
public static class RawCom {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern SafeFileHandle CreateFile(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadFile(SafeFileHandle h, byte[] b, int n, out int r, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetCommTimeouts(SafeFileHandle h, ref COMMTIMEOUTS t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool EscapeCommFunction(SafeFileHandle h, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct COMMTIMEOUTS { public uint RI, RTM, RTC, WTM, WTC; }
  public static string Read(string port, int seconds) {
    var h = CreateFile("\\\\.\\" + port, 0xC0000000, 0, IntPtr.Zero, 3, 0, IntPtr.Zero);
    if (h.IsInvalid) return "CreateFile failed, err " + Marshal.GetLastWin32Error();
    var t = new COMMTIMEOUTS { RI = 0xFFFFFFFF, RTM = 0, RTC = 200 };
    bool to = SetCommTimeouts(h, ref t);
    EscapeCommFunction(h, 6); System.Threading.Thread.Sleep(150); EscapeCommFunction(h, 5); // pulse DTR -> reset Nano
    var hex = new StringBuilder(); var sb = new StringBuilder(); var buf = new byte[1024]; var end = DateTime.Now.AddSeconds(seconds);
    while (DateTime.Now < end) {
      int r;
      if (!ReadFile(h, buf, buf.Length, out r, IntPtr.Zero)) { sb.Append("[ReadFile err " + Marshal.GetLastWin32Error() + "]"); break; }
      if (r > 0) { sb.Append(Encoding.ASCII.GetString(buf, 0, r)); for (int i = 0; i < r; i++) hex.Append(buf[i].ToString("X2") + " "); }
    }
    h.Close(); return "bytes: " + (hex.Length / 3) + "\nHEX: " + hex + "\nTEXT:\n" + sb;
  }
}
'@
Add-Type -TypeDefinition $src
mode $Port BAUD=$Baud PARITY=n DATA=8 STOP=1 dtr=on | Out-Null
$out = [RawCom]::Read($Port, $Seconds)
"---- $Port @ $Baud ----"
$out

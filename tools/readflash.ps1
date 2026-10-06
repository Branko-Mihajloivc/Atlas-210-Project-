param([string]$Port = 'COM17', [int]$Baud = 57600, [string]$Out = "$PSScriptRoot\flash.bin")
$src = @'
using System; using System.IO; using System.Runtime.InteropServices; using Microsoft.Win32.SafeHandles;
public static class StkRead {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern SafeFileHandle CreateFile(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadFile(SafeFileHandle h, byte[] b, int n, out int r, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteFile(SafeFileHandle h, byte[] b, int n, out int w, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetCommTimeouts(SafeFileHandle h, ref COMMTIMEOUTS t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool EscapeCommFunction(SafeFileHandle h, uint f);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool PurgeComm(SafeFileHandle h, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct COMMTIMEOUTS { public uint RI, RTM, RTC, WTM, WTC; }
  static SafeFileHandle h;
  static void Send(params byte[] b) { int w; WriteFile(h, b, b.Length, out w, IntPtr.Zero); }
  static byte[] Recv(int n) {
    var res = new byte[n]; int got = 0; var end = DateTime.Now.AddMilliseconds(1000);
    while (got < n && DateTime.Now < end) { var tmp = new byte[n - got]; int r; ReadFile(h, tmp, tmp.Length, out r, IntPtr.Zero); Array.Copy(tmp, 0, res, got, r); got += r; }
    if (got < n) throw new Exception("timeout: got " + got + " of " + n);
    return res;
  }
  public static string Run(string port, string outPath) {
    h = CreateFile("\\\\.\\" + port, 0xC0000000, 0, IntPtr.Zero, 3, 0, IntPtr.Zero);
    if (h.IsInvalid) return "CreateFile failed, err " + Marshal.GetLastWin32Error();
    var t = new COMMTIMEOUTS { RI = 0xFFFFFFFF, RTM = 0, RTC = 50, WTC = 500 };
    SetCommTimeouts(h, ref t);
    EscapeCommFunction(h, 6); EscapeCommFunction(h, 4); System.Threading.Thread.Sleep(250);
    EscapeCommFunction(h, 5); EscapeCommFunction(h, 3); System.Threading.Thread.Sleep(50);
    PurgeComm(h, 0x0F);
    bool sync = false;
    for (int i = 0; i < 10 && !sync; i++) {
      Send(0x30, 0x20); System.Threading.Thread.Sleep(50);
      var b = new byte[16]; int r; ReadFile(h, b, 16, out r, IntPtr.Zero);
      if (r >= 2 && b[r-2] == 0x14 && b[r-1] == 0x10) sync = true;
    }
    if (!sync) { h.Close(); return "no sync"; }
    PurgeComm(h, 0x0F);
    var flash = new byte[32768];
    for (int addr = 0; addr < 32768; addr += 128) {
      int word = addr / 2;
      Send(0x55, (byte)(word & 0xFF), (byte)(word >> 8), 0x20);
      var a = Recv(2); if (a[0] != 0x14 || a[1] != 0x10) throw new Exception("bad addr ack at " + addr);
      Send(0x74, 0x00, 0x80, 0x46, 0x20);
      var d = Recv(130); if (d[0] != 0x14 || d[129] != 0x10) throw new Exception("bad page at " + addr);
      Array.Copy(d, 1, flash, addr, 128);
    }
    Send(0x51, 0x20); try { Recv(2); } catch {} // leave programming mode -> sketch starts
    h.Close();
    File.WriteAllBytes(outPath, flash);
    int last = 32767; while (last >= 0 && flash[last] == 0xFF) last--;
    return "OK, 32768 bytes read; last non-FF byte at 0x" + last.ToString("X4");
  }
}
'@
Add-Type -TypeDefinition $src
mode $Port BAUD=$Baud PARITY=n DATA=8 STOP=1 dtr=off rts=off | Out-Null
[StkRead]::Run($Port, $Out)

# Upload an Intel HEX (or raw .bin) to an Arduino Nano through its STK500v1 bootloader,
# opening the COM port with plain Win32 calls. Works with CH340 drivers where avrdude fails
# with "cannot set com-state".
#   Old bootloader: -Baud 57600   New (Optiboot) bootloader: -Baud 115200
param(
    [Parameter(Mandatory = $true)][string]$File,
    [string]$Port = 'COM17',
    [int]$Baud = 57600
)

$src = @'
using System; using System.IO; using System.Runtime.InteropServices; using Microsoft.Win32.SafeHandles;
public static class NanoUpload {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] static extern SafeFileHandle CreateFile(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadFile(SafeFileHandle h, byte[] b, int n, out int r, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteFile(SafeFileHandle h, byte[] b, int n, out int w, IntPtr o);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetCommTimeouts(SafeFileHandle h, ref COMMTIMEOUTS t);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool EscapeCommFunction(SafeFileHandle h, uint f);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool PurgeComm(SafeFileHandle h, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct COMMTIMEOUTS { public uint RI, RTM, RTC, WTM, WTC; }
  static SafeFileHandle h;
  const int PAGE = 128;

  static void Send(params byte[] b) { int w; WriteFile(h, b, b.Length, out w, IntPtr.Zero); }
  static byte[] Recv(int n) {
    var res = new byte[n]; int got = 0; var end = DateTime.Now.AddMilliseconds(1000);
    while (got < n && DateTime.Now < end) {
      var tmp = new byte[n - got]; int r;
      ReadFile(h, tmp, tmp.Length, out r, IntPtr.Zero);
      Array.Copy(tmp, 0, res, got, r); got += r;
    }
    if (got < n) throw new Exception("timeout: got " + got + " of " + n + " bytes");
    return res;
  }
  static void Ok(byte[] a, string what) { if (a[0] != 0x14 || a[a.Length - 1] != 0x10) throw new Exception("bad reply to " + what); }
  static void LoadAddress(int byteAddr) { int w = byteAddr / 2; Send(0x55, (byte)(w & 0xFF), (byte)(w >> 8), 0x20); Ok(Recv(2), "load address"); }

  public static byte[] ParseHex(string path, out int length) {
    var img = new byte[32768]; for (int i = 0; i < img.Length; i++) img[i] = 0xFF;
    int baseAddr = 0; length = 0;
    foreach (var raw in File.ReadAllLines(path)) {
      var line = raw.Trim(); if (line.Length == 0) continue;
      if (line[0] != ':') throw new Exception("not an Intel HEX line: " + line);
      int count = Convert.ToInt32(line.Substring(1, 2), 16);
      int addr = Convert.ToInt32(line.Substring(3, 4), 16);
      int type = Convert.ToInt32(line.Substring(7, 2), 16);
      int sum = 0; for (int i = 1; i < line.Length; i += 2) sum += Convert.ToInt32(line.Substring(i, 2), 16);
      if ((sum & 0xFF) != 0) throw new Exception("HEX checksum error: " + line);
      if (type == 0) {
        for (int i = 0; i < count; i++) {
          int a = baseAddr + addr + i;
          img[a] = Convert.ToByte(line.Substring(9 + i * 2, 2), 16);
          if (a + 1 > length) length = a + 1;
        }
      } else if (type == 2) baseAddr = Convert.ToInt32(line.Substring(9, 4), 16) << 4;
      else if (type == 4) baseAddr = Convert.ToInt32(line.Substring(9, 4), 16) << 16;
      else if (type == 1) break;
    }
    return img;
  }

  public static string Run(string port, byte[] img, int length, Action<string> log) {
    if (length > 30720) throw new Exception("image is " + length + " bytes, would overwrite the bootloader (max 30720)");
    h = CreateFile("\\\\.\\" + port, 0xC0000000, 0, IntPtr.Zero, 3, 0, IntPtr.Zero);
    if (h.IsInvalid) return "CreateFile failed, Win32 error " + Marshal.GetLastWin32Error();
    try {
      var t = new COMMTIMEOUTS { RI = 0xFFFFFFFF, RTM = 0, RTC = 50, WTC = 1000 };
      SetCommTimeouts(h, ref t);
      // reset the Nano: DTR/RTS low -> high
      EscapeCommFunction(h, 6); EscapeCommFunction(h, 4); System.Threading.Thread.Sleep(250);
      EscapeCommFunction(h, 5); EscapeCommFunction(h, 3); System.Threading.Thread.Sleep(50);
      PurgeComm(h, 0x0F);

      bool sync = false;
      for (int i = 0; i < 10 && !sync; i++) {
        Send(0x30, 0x20); System.Threading.Thread.Sleep(50);
        var b = new byte[16]; int r; ReadFile(h, b, 16, out r, IntPtr.Zero);
        if (r >= 2 && b[r - 2] == 0x14 && b[r - 1] == 0x10) sync = true;
      }
      if (!sync) return "no sync with bootloader (wrong baud rate?)";
      PurgeComm(h, 0x0F);

      Send(0x75, 0x20); var sig = Recv(5); Ok(sig, "read signature");
      log(string.Format("signature {0:X2} {1:X2} {2:X2}", sig[1], sig[2], sig[3]));
      if (sig[1] != 0x1E || sig[2] != 0x95 || sig[3] != 0x0F) return "unexpected signature - not an ATmega328P, nothing written";

      Send(0x50, 0x20); Ok(Recv(2), "enter programming mode");

      int pages = (length + PAGE - 1) / PAGE;
      for (int p = 0; p < pages; p++) {
        int addr = p * PAGE;
        LoadAddress(addr);
        var cmd = new byte[4 + PAGE + 1];
        cmd[0] = 0x64; cmd[1] = 0x00; cmd[2] = PAGE; cmd[3] = 0x46;
        Array.Copy(img, addr, cmd, 4, PAGE);
        cmd[4 + PAGE] = 0x20;
        Send(cmd); Ok(Recv(2), "program page 0x" + addr.ToString("X4"));
      }
      log("written " + pages + " pages (" + length + " bytes)");

      for (int p = 0; p < pages; p++) {
        int addr = p * PAGE;
        LoadAddress(addr);
        Send(0x74, 0x00, PAGE, 0x46, 0x20);
        var d = Recv(PAGE + 2); Ok(d, "read page");
        for (int i = 0; i < PAGE; i++)
          if (d[1 + i] != img[addr + i])
            return "VERIFY FAILED at 0x" + (addr + i).ToString("X4");
      }
      log("verify OK");

      Send(0x51, 0x20); try { Recv(2); } catch { }   // leave programming mode, sketch starts
      return "DONE";
    } finally { h.Close(); }
  }
}
'@
Add-Type -TypeDefinition $src

$path = (Resolve-Path -LiteralPath $File).Path
$len = 0
if ($path -match '\.hex$') {
    $img = [NanoUpload]::ParseHex($path, [ref]$len)
} else {
    $bin = [IO.File]::ReadAllBytes($path)
    $len = [Math]::Min($bin.Length, 30720)                        # never touch the bootloader area of a full dump
    while ($len -gt 0 -and $bin[$len - 1] -eq 0xFF) { $len-- }   # skip the empty tail
    $img = New-Object byte[] 32768
    for ($i = 0; $i -lt 32768; $i++) { $img[$i] = 0xFF }
    [Array]::Copy($bin, $img, [Math]::Min($bin.Length, 32768))
}

mode $Port BAUD=$Baud PARITY=n DATA=8 STOP=1 dtr=off rts=off | Out-Null
"Uploading $([IO.Path]::GetFileName($path)) ($len bytes) to $Port @ $Baud"
[NanoUpload]::Run($Port, $img, $len, [Action[string]] { param($m) Write-Host $m })

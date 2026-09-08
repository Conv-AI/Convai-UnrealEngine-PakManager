$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class AvatarArchiveRenameProbe
{
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool SetFileInformationByHandle(IntPtr file, int kind, IntPtr data, uint length);
    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr handle);

    public static string Run(string parent, string variant, bool relative, bool fullDirectoryAccess, bool extended)
    {
        string directory = Path.Combine(parent, variant);
        Directory.CreateDirectory(directory);
        string source = Path.Combine(directory, "created.txt");
        string destination = Path.Combine(directory, "existing.txt");
        File.WriteAllText(source, "new-verified-bytes");
        File.WriteAllText(destination, "old-good-bytes");
        IntPtr file = new IntPtr(-1), root = new IntPtr(-1), buffer = IntPtr.Zero;
        bool success = false;
        int error = 0;
        try
        {
            root = CreateFileW(directory, fullDirectoryAccess ? 0x001F01FFu : 0x80u, 1, IntPtr.Zero, 3, 0x02200000, IntPtr.Zero);
            if (root == new IntPtr(-1)) return variant + ": directory open error=" + Marshal.GetLastWin32Error();
            file = CreateFileW(source, 0xC0010000, 1, IntPtr.Zero, 3, 0, IntPtr.Zero);
            if (file == new IntPtr(-1)) return variant + ": file open error=" + Marshal.GetLastWin32Error();
            string name = relative ? "existing.txt" : extended ? "\\\\?\\" + destination : destination;
            byte[] nameBytes = Encoding.Unicode.GetBytes(name);
            int rootOffset = IntPtr.Size == 8 ? 8 : 4;
            int lengthOffset = rootOffset + IntPtr.Size;
            int nameOffset = lengthOffset + 4;
            int size = nameOffset + nameBytes.Length + 2;
            buffer = Marshal.AllocHGlobal(size);
            for (int i=0; i<size; ++i) Marshal.WriteByte(buffer, i, 0);
            Marshal.WriteByte(buffer, 0, 1);
            Marshal.WriteIntPtr(buffer, rootOffset, relative ? root : IntPtr.Zero);
            Marshal.WriteInt32(buffer, lengthOffset, nameBytes.Length);
            Marshal.Copy(nameBytes, 0, IntPtr.Add(buffer, nameOffset), nameBytes.Length);
            success = SetFileInformationByHandle(file, 3, buffer, (uint)size);
            error = success ? 0 : Marshal.GetLastWin32Error();
        }
        finally
        {
            if (buffer != IntPtr.Zero) Marshal.FreeHGlobal(buffer);
            if (file != new IntPtr(-1)) CloseHandle(file);
            if (root != new IntPtr(-1)) CloseHandle(root);
        }
        string result = variant + ": success=" + success + " error=" + error + " destination=" + File.ReadAllText(destination);
        if (File.Exists(source)) File.Delete(source);
        File.Delete(destination);
        Directory.Delete(directory, false);
        return result;
    }
}
'@
$fixtureParent = Join-Path $PSScriptRoot ('rename-' + [Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($fixtureParent) | Out-Null
try {
    [AvatarArchiveRenameProbe]::Run($fixtureParent, 'relative_attributes', $true, $false, $false)
    [AvatarArchiveRenameProbe]::Run($fixtureParent, 'relative_full_access', $true, $true, $false)
    [AvatarArchiveRenameProbe]::Run($fixtureParent, 'absolute_dos', $false, $false, $false)
    [AvatarArchiveRenameProbe]::Run($fixtureParent, 'absolute_extended', $false, $false, $true)
}
finally {
    if ([System.IO.Directory]::GetFileSystemEntries($fixtureParent).Length -eq 0) {
        [System.IO.Directory]::Delete($fixtureParent, $false)
    }
}

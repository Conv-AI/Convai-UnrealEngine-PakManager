param([string] $FixtureDirectory)

$ErrorActionPreference = 'Stop'
$development = [IO.Path]::GetFullPath('E:/UEProjects/UE5.8/Dev_CPM_58/Saved/ConvaiAvatarStudio/Development')
if (-not $FixtureDirectory) {
    $FixtureDirectory = Join-Path $development ('GuardedRenameProbe-' + [Guid]::NewGuid().ToString('N'))
}
$fixture = [IO.Path]::GetFullPath($FixtureDirectory).TrimEnd('\', '/')
$leaf = [IO.Path]::GetFileName($fixture)
$fixtureId = [Guid]::Empty
if (-not [string]::Equals([IO.Path]::GetDirectoryName($fixture), $development, [StringComparison]::OrdinalIgnoreCase) -or
    -not $leaf.StartsWith('GuardedRenameProbe-', [StringComparison]::Ordinal) -or
    -not [Guid]::TryParseExact($leaf.Substring('GuardedRenameProbe-'.Length), 'N', [ref] $fixtureId)) {
    throw 'Use a new immediate Development/GuardedRenameProbe-<32-digit GUID> directory.'
}
if ([IO.Directory]::Exists($fixture) -or [IO.File]::Exists($fixture)) { throw 'The fixture must not already exist.' }
$ancestor = $development
while ($ancestor) {
    $attributes = [IO.File]::GetAttributes($ancestor)
    if (($attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse ancestor refused: $ancestor" }
    $ancestor = [IO.Path]::GetDirectoryName($ancestor)
}
[IO.Directory]::CreateDirectory($fixture) | Out-Null
[IO.File]::WriteAllText((Join-Path $fixture 'owned-fixture.txt'), $fixture)

# Windows SDK 10.0.26100.0 um/winbase.h:9112 defines the rename buffer layout.
# https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info
# https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information
# The NT simple-name/null-root case explicitly means rename within the existing parent.
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class AvatarStudioGuardedRenameProbe
{
    const uint List = 1, AddFile = 2, Traverse = 0x20, ReadAttributes = 0x80, Delete = 0x10000;
    const uint Read = 0x80000000, Write = 0x40000000, ShareRead = 1, ShareAll = 7;
    const uint OpenExisting = 3, CreateNew = 1, DirectoryFlags = 0x02200000;
    static readonly IntPtr Invalid = new IntPtr(-1);

    [StructLayout(LayoutKind.Sequential)]
    struct RenameLayout { public uint Flags; public IntPtr Root; public uint Length; public ushort Name; }
    [StructLayout(LayoutKind.Sequential)]
    struct IoStatus { public IntPtr Status; public UIntPtr Information; }
    [StructLayout(LayoutKind.Sequential)]
    struct FileInfo
    {
        public uint Attributes;
        public System.Runtime.InteropServices.ComTypes.FILETIME Creation, Access, Write;
        public uint Volume, SizeHigh, SizeLow, Links, IndexHigh, IndexLow;
    }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr CreateFileW(string p, uint access, uint share, IntPtr security, uint mode, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetFileInformationByHandle(IntPtr h, int infoClass, IntPtr buffer, uint size);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool MoveFileExW(string from, string to, uint flags);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool GetFileInformationByHandle(IntPtr h, out FileInfo info);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern uint GetFinalPathNameByHandleW(IntPtr h, StringBuilder path, uint length, uint flags);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool WriteFile(IntPtr h, byte[] bytes, uint count, out uint written, IntPtr overlapped);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool ReadFile(IntPtr h, byte[] bytes, uint count, out uint read, IntPtr overlapped);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetFilePointerEx(IntPtr h, long offset, out long actual, uint origin);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool FlushFileBuffers(IntPtr h);
    [DllImport("ntdll.dll")] static extern int NtSetInformationFile(IntPtr h, out IoStatus status, IntPtr buffer, uint size, int infoClass);
    [DllImport("ntdll.dll")] static extern uint RtlNtStatusToDosError(int status);

    public sealed class Result
    {
        public string Case, Directory, Stage, Exception, NtStatus, FileIdentityBefore, FileIdentityAfter, FinalFilePath;
        public uint GuardAccess, GuardShare, NtMappedError;
        public int GuardOpenError, DeleteOpenError, DirectoryRenameError, DirectoryRestoreError, FileOpenError, RenameError;
        public bool GuardPresent, DeleteOpenAllowed, DirectoryRenameAllowed, DirectoryRestoreSucceeded;
        public bool RenameSucceeded, SameFileObject, SameFileBytes, SameGuardObjectAndPath, DestinationExists, TemporaryExists;
    }

    static string Native(string p) { return "\\\\?\\" + Path.GetFullPath(p); }
    static string Identity(IntPtr h)
    {
        FileInfo i;
        if (!GetFileInformationByHandle(h, out i)) { throw new IOException("GetFileInformationByHandle: " + Marshal.GetLastWin32Error()); }
        return i.Volume.ToString("X8") + ":" + i.IndexHigh.ToString("X8") + i.IndexLow.ToString("X8");
    }
    static string FinalPath(IntPtr h)
    {
        StringBuilder b = new StringBuilder(32768);
        uint n = GetFinalPathNameByHandleW(h, b, (uint)b.Capacity, 0);
        if (n == 0 || n >= b.Capacity) { throw new IOException("GetFinalPathNameByHandleW: " + Marshal.GetLastWin32Error()); }
        return b.ToString();
    }
    static void Rename(IntPtr file, IntPtr parent, string name, bool native, Result r)
    {
        byte[] bytes = Encoding.Unicode.GetBytes(name);
        int rootOffset = (int)Marshal.OffsetOf(typeof(RenameLayout), "Root");
        int lengthOffset = (int)Marshal.OffsetOf(typeof(RenameLayout), "Length");
        int nameOffset = (int)Marshal.OffsetOf(typeof(RenameLayout), "Name");
        int size = Marshal.SizeOf(typeof(RenameLayout)) + bytes.Length;
        IntPtr buffer = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.Copy(new byte[size], 0, buffer, size);
            Marshal.WriteIntPtr(buffer, rootOffset, parent);
            Marshal.WriteInt32(buffer, lengthOffset, bytes.Length);
            Marshal.Copy(bytes, 0, IntPtr.Add(buffer, nameOffset), bytes.Length);
            if (native)
            {
                IoStatus io;
                int status = NtSetInformationFile(file, out io, buffer, (uint)size, 10);
                r.NtStatus = unchecked((uint)status).ToString("X8");
                r.NtMappedError = RtlNtStatusToDosError(status);
                r.RenameSucceeded = status >= 0;
                r.RenameError = r.RenameSucceeded ? 0 : (int)r.NtMappedError;
            }
            else
            {
                r.RenameSucceeded = SetFileInformationByHandle(file, 3, buffer, (uint)size);
                r.RenameError = r.RenameSucceeded ? 0 : Marshal.GetLastWin32Error();
            }
        }
        finally { Marshal.FreeHGlobal(buffer); }
    }
    static Result RunCase(string root, string name, uint guardAccess, bool native, bool relativeParent, bool simpleSameDirectory)
    {
        Result r = new Result { Case = name, Directory = Path.Combine(root, name), GuardAccess = guardAccess, GuardShare = ShareRead };
        IntPtr guard = Invalid, file = Invalid;
        Directory.CreateDirectory(r.Directory);
        string temporary = Path.Combine(r.Directory, "pending.tmp");
        string destination = Path.Combine(r.Directory, "pending.json");
        string guardIdentity = null;
        try
        {
            r.Stage = "open guard";
            if (guardAccess != 0)
            {
                guard = CreateFileW(Native(r.Directory), guardAccess, ShareRead, IntPtr.Zero, OpenExisting, DirectoryFlags, IntPtr.Zero);
                r.GuardOpenError = guard == Invalid ? Marshal.GetLastWin32Error() : 0;
                if (guard == Invalid) { return r; }
                r.GuardPresent = true;
                guardIdentity = Identity(guard);
            }

            // Test the empty directory: a held child file would mask an ineffective guard.
            r.Stage = "probe directory DELETE access and rename";
            IntPtr probe = CreateFileW(Native(r.Directory), Delete, ShareAll, IntPtr.Zero, OpenExisting, DirectoryFlags, IntPtr.Zero);
            r.DeleteOpenAllowed = probe != Invalid;
            r.DeleteOpenError = r.DeleteOpenAllowed ? 0 : Marshal.GetLastWin32Error();
            if (probe != Invalid) { CloseHandle(probe); }
            string moved = r.Directory + ".renamed";
            r.DirectoryRenameAllowed = MoveFileExW(Native(r.Directory), Native(moved), 0);
            r.DirectoryRenameError = r.DirectoryRenameAllowed ? 0 : Marshal.GetLastWin32Error();
            if (r.DirectoryRenameAllowed)
            {
                r.DirectoryRestoreSucceeded = MoveFileExW(Native(moved), Native(r.Directory), 0);
                r.DirectoryRestoreError = r.DirectoryRestoreSucceeded ? 0 : Marshal.GetLastWin32Error();
                if (!r.DirectoryRestoreSucceeded) { return r; }
            }

            r.Stage = "create and flush owned file under guard";
            file = CreateFileW(Native(temporary), Read | Write | Delete, 0, IntPtr.Zero, CreateNew, 0x00200080, IntPtr.Zero);
            r.FileOpenError = file == Invalid ? Marshal.GetLastWin32Error() : 0;
            if (file == Invalid) { return r; }
            byte[] payload = Encoding.UTF8.GetBytes("owned-guarded-rename:" + name + "\n");
            uint count;
            if (!WriteFile(file, payload, (uint)payload.Length, out count, IntPtr.Zero) || count != payload.Length || !FlushFileBuffers(file))
            { throw new IOException("Write/flush: " + Marshal.GetLastWin32Error()); }
            r.FileIdentityBefore = Identity(file);
            r.Stage = "rename retained file handle";
            Rename(file, relativeParent ? guard : IntPtr.Zero,
                (relativeParent || simpleSameDirectory) ? "pending.json" : Native(destination), native, r);
            r.FileIdentityAfter = Identity(file);
            r.SameFileObject = r.FileIdentityBefore == r.FileIdentityAfter;
            r.FinalFilePath = FinalPath(file);
            long offset;
            byte[] actual = new byte[payload.Length + 1];
            if (!SetFilePointerEx(file, 0, out offset, 0) || !ReadFile(file, actual, (uint)actual.Length, out count, IntPtr.Zero))
            { throw new IOException("Read retained object: " + Marshal.GetLastWin32Error()); }
            r.SameFileBytes = count == payload.Length && Encoding.UTF8.GetString(actual, 0, (int)count) == Encoding.UTF8.GetString(payload);
            r.SameGuardObjectAndPath = guard == Invalid || (Identity(guard) == guardIdentity && String.Equals(FinalPath(guard), Native(r.Directory), StringComparison.OrdinalIgnoreCase));
            r.DestinationExists = File.Exists(destination);
            r.TemporaryExists = File.Exists(temporary);
            r.Stage = "complete";
        }
        catch (Exception e) { r.Exception = e.GetType().Name + ": " + e.Message; }
        finally
        {
            if (file != Invalid) { CloseHandle(file); }
            if (guard != Invalid) { CloseHandle(guard); }
        }
        return r;
    }
    public static Result[] Run(string root)
    {
        return new Result[] {
            RunCase(root, "ControlWin32Absolute", 0, false, false, false),
            RunCase(root, "HeldListingGuardAbsoluteRename", List | ReadAttributes, false, false, false),
            RunCase(root, "OwnedParentWin32RelativeRename", List | AddFile | ReadAttributes, false, true, false),
            RunCase(root, "OwnedParentNtRelativeRename", List | AddFile | ReadAttributes, true, true, false),
            RunCase(root, "HeldListingGuardNtSameDirectoryRename", List | ReadAttributes, true, false, true),
            RunCase(root, "OwnedTraverseParentNtRelativeRename", Traverse | ReadAttributes, true, true, false)
        };
    }
}
'@

$previousDirectory = [Environment]::CurrentDirectory
try {
    [Environment]::CurrentDirectory = $fixture
    $results = [AvatarStudioGuardedRenameProbe]::Run($fixture)
}
finally { [Environment]::CurrentDirectory = $previousDirectory }
$report = [ordered]@{ fixture = $fixture; utc = [DateTime]::UtcNow.ToString('o'); pointerBytes = [IntPtr]::Size; results = $results }
$json = $report | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText((Join-Path $fixture 'results.json'), $json)
$json

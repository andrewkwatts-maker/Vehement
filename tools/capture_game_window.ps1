Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    public class WinAPI {
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    }
"@

# Find the Nova3D Engine Demo window
$process = Get-Process | Where-Object {$_.MainWindowTitle -like "*Nova3D Engine Demo*"} | Select-Object -First 1

if ($process) {
    Write-Host "Found window: $($process.MainWindowTitle)"

    # Bring window to front
    [WinAPI]::ShowWindow($process.MainWindowHandle, 9)  # SW_RESTORE
    [WinAPI]::SetForegroundWindow($process.MainWindowHandle)

    Start-Sleep -Milliseconds 500

    # Take screenshot
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap($screen.Width, $screen.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($screen.Location, [System.Drawing.Point]::Empty, $screen.Size)

    $timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $path = "H:\Github\Old3DEngine\screenshots\hero_polish\screenshot_$timestamp.png"

    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output $path

    $graphics.Dispose()
    $bitmap.Dispose()
} else {
    Write-Host "Nova3D Engine Demo window not found"
}

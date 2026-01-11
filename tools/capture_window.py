#!/usr/bin/env python3
"""
Capture screenshot of Nova Demo window
"""

import subprocess
import time
from pathlib import Path

try:
    from PIL import ImageGrab
    import win32gui
    import win32ui
    import win32con
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("PIL or win32gui not available - attempting PowerShell method")

PROJECT_ROOT = Path("H:/Github/Old3DEngine")
SCREENSHOTS_DIR = PROJECT_ROOT / "screenshots/hero_polish"

def find_window(title_substring):
    """Find window by partial title match"""
    windows = []

    def callback(hwnd, _):
        if win32gui.IsWindowVisible(hwnd):
            title = win32gui.GetWindowText(hwnd)
            if title_substring.lower() in title.lower():
                windows.append((hwnd, title))

    win32gui.EnumWindows(callback, None)
    return windows

def capture_window_screenshot(hwnd, output_path):
    """Capture screenshot of specific window"""
    # Get window dimensions
    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    width = right - left
    height = bottom - top

    # Capture window
    hwndDC = win32gui.GetWindowDC(hwnd)
    mfcDC = win32ui.CreateDCFromHandle(hwndDC)
    saveDC = mfcDC.CreateCompatibleDC()

    saveBitMap = win32ui.CreateBitmap()
    saveBitMap.CreateCompatibleBitmap(mfcDC, width, height)
    saveDC.SelectObject(saveBitMap)

    result = windll.user32.PrintWindow(hwnd, saveDC.GetSafeHdc(), 3)

    bmpinfo = saveBitMap.GetInfo()
    bmpstr = saveBitMap.GetBitmapBits(True)

    from PIL import Image
    img = Image.frombuffer(
        'RGB',
        (bmpinfo['bmWidth'], bmpinfo['bmHeight']),
        bmpstr, 'raw', 'BGRX', 0, 1
    )

    # Save
    img.save(output_path)

    # Cleanup
    win32gui.DeleteObject(saveBitMap.GetHandle())
    saveDC.DeleteDC()
    mfcDC.DeleteDC()
    win32gui.ReleaseDC(hwnd, hwndDC)

    return True

def capture_via_powershell():
    """Fallback: Use PowerShell to capture full screen"""
    output_path = SCREENSHOTS_DIR / f"screenshot_{int(time.time())}.png"

    ps_script = f"""
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap($screen.Width, $screen.Height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($screen.Location, [System.Drawing.Point]::Empty, $screen.Size)
$bitmap.Save('{str(output_path).replace('\\', '\\\\')}')
$graphics.Dispose()
$bitmap.Dispose()
"""

    subprocess.run(['powershell', '-Command', ps_script], check=True)
    return output_path

def main():
    print("=" * 70)
    print(" Screenshot Capture - Nova Demo")
    print("=" * 70)

    # Create screenshots directory
    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)

    output_path = SCREENSHOTS_DIR / f"hero_screenshot_{int(time.time())}.png"

    if PIL_AVAILABLE:
        print("\nSearching for Nova Demo window...")
        windows = find_window("nova")

        if not windows:
            print("No Nova window found - trying full screen capture instead")
            output_path = capture_via_powershell()
            print(f"\nOK: Full screen captured: {output_path}")
            return

        print(f"\nFound {len(windows)} window(s):")
        for i, (hwnd, title) in enumerate(windows, 1):
            print(f"  {i}. {title}")

        # Use first window
        hwnd, title = windows[0]
        print(f"\nCapturing: {title}")

        try:
            if capture_window_screenshot(hwnd, output_path):
                print(f"OK: Screenshot saved: {output_path}")
            else:
                print("Failed to capture - trying full screen")
                output_path = capture_via_powershell()
                print(f"OK: Full screen captured: {output_path}")
        except Exception as e:
            print(f"Error: {e}")
            print("Trying full screen capture...")
            output_path = capture_via_powershell()
            print(f"OK: Full screen captured: {output_path}")
    else:
        # Fallback to PowerShell
        print("\nUsing PowerShell for full screen capture...")
        output_path = capture_via_powershell()
        print(f"\nOK: Screenshot captured: {output_path}")

if __name__ == "__main__":
    main()

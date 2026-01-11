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

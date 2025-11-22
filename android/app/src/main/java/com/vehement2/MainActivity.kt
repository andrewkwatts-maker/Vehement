package com.vehement2

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.View
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

/**
 * Main Activity for Nova3D Engine Android Application
 *
 * Handles:
 * - Activity lifecycle management
 * - Permission requests
 * - Fullscreen/immersive mode
 * - Integration with GameSurfaceView
 */
class MainActivity : AppCompatActivity() {

    private lateinit var gameSurfaceView: GameSurfaceView
    private var locationPermissionGranted = false

    companion object {
        private const val LOCATION_PERMISSION_REQUEST_CODE = 1001
        private const val TAG = "MainActivity"
    }

    // -------------------------------------------------------------------------
    // Activity Lifecycle
    // -------------------------------------------------------------------------

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Enable fullscreen immersive mode
        setupFullscreen()

        // Keep screen on during gameplay
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Create and set the game surface view
        gameSurfaceView = GameSurfaceView(this)
        setContentView(gameSurfaceView)

        // Initialize native library
        NativeLib.getInstance().initialize(this)
    }

    override fun onStart() {
        super.onStart()
    }

    override fun onResume() {
        super.onResume()
        gameSurfaceView.onResume()
        NativeLib.getInstance().resume()
    }

    override fun onPause() {
        super.onPause()
        gameSurfaceView.onPause()
        NativeLib.getInstance().pause()
    }

    override fun onStop() {
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
        NativeLib.getInstance().destroy()
    }

    override fun onLowMemory() {
        super.onLowMemory()
        NativeLib.getInstance().onLowMemory()
    }

    // -------------------------------------------------------------------------
    // Fullscreen Configuration
    // -------------------------------------------------------------------------

    private fun setupFullscreen() {
        // Use WindowCompat for edge-to-edge display
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // Hide system bars
        val windowInsetsController = WindowInsetsControllerCompat(window, window.decorView)
        windowInsetsController.apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        // Additional flags for older Android versions
        @Suppress("DEPRECATION")
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            or View.SYSTEM_UI_FLAG_FULLSCREEN
            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        )
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            setupFullscreen()
        }
    }

    // -------------------------------------------------------------------------
    // Permission Handling
    // -------------------------------------------------------------------------

    fun requestLocationPermission() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(
                this,
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
                ),
                LOCATION_PERMISSION_REQUEST_CODE
            )
        } else {
            locationPermissionGranted = true
            NativeLib.getInstance().onLocationPermissionResult(true)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        when (requestCode) {
            LOCATION_PERMISSION_REQUEST_CODE -> {
                locationPermissionGranted = grantResults.isNotEmpty() &&
                        grantResults[0] == PackageManager.PERMISSION_GRANTED

                NativeLib.getInstance().onLocationPermissionResult(locationPermissionGranted)
            }
        }
    }

    fun hasLocationPermission(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) ==
                PackageManager.PERMISSION_GRANTED
    }

    // -------------------------------------------------------------------------
    // Back Button Handling
    // -------------------------------------------------------------------------

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        // Could add custom back button handling here
        // For example, show exit confirmation dialog
        @Suppress("DEPRECATION")
        super.onBackPressed()
    }
}

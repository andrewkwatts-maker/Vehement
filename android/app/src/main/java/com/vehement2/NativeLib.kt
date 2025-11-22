package com.vehement2

import android.content.Context
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.Surface
import android.widget.Toast
import androidx.core.content.ContextCompat
import java.lang.ref.WeakReference

/**
 * Native Library Bridge for Nova3D Engine
 *
 * Provides:
 * - JNI method declarations
 * - Location services integration
 * - System services (vibration, toast)
 * - Thread-safe singleton pattern
 */
class NativeLib private constructor() {

    private var contextRef: WeakReference<Context>? = null
    private var locationManager: LocationManager? = null
    private var locationListener: LocationListener? = null
    private var isLocationUpdatesActive = false

    companion object {
        private const val TAG = "NativeLib"

        // Singleton instance
        @Volatile
        private var instance: NativeLib? = null

        fun getInstance(): NativeLib {
            return instance ?: synchronized(this) {
                instance ?: NativeLib().also { instance = it }
            }
        }

        init {
            System.loadLibrary("nova3d")
        }
    }

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    fun initialize(context: Context) {
        contextRef = WeakReference(context.applicationContext)

        // Initialize native library with asset manager
        val assetManager = context.assets
        init(assetManager)
    }

    // -------------------------------------------------------------------------
    // Native Methods
    // -------------------------------------------------------------------------

    /**
     * Initialize native engine with asset manager
     */
    private external fun init(assetManager: Any)

    /**
     * Notify native code that surface was created
     */
    external fun surfaceCreated(surface: Surface)

    /**
     * Notify native code that surface was destroyed
     */
    external fun surfaceDestroyed()

    /**
     * Notify native code of surface size change
     */
    external fun resize(width: Int, height: Int)

    /**
     * Render a single frame
     */
    external fun step()

    /**
     * Handle touch event
     */
    external fun touch(action: Int, x: Float, y: Float, pointerId: Int)

    /**
     * Handle touch event with pressure
     */
    external fun touchWithPressure(action: Int, x: Float, y: Float, pointerId: Int, pressure: Float)

    /**
     * Notify native code of pause
     */
    external fun pause()

    /**
     * Notify native code of resume
     */
    external fun resume()

    /**
     * Notify native code of destroy
     */
    external fun destroy()

    /**
     * Set GPS location
     */
    external fun setLocation(latitude: Double, longitude: Double)

    /**
     * Set GPS location with full data
     */
    external fun setLocationFull(
        latitude: Double,
        longitude: Double,
        altitude: Double,
        accuracy: Float,
        timestamp: Long
    )

    /**
     * Notify native code of location permission result
     */
    external fun onLocationPermissionResult(granted: Boolean)

    /**
     * Notify native code of low memory
     */
    external fun onLowMemory()

    // -------------------------------------------------------------------------
    // Location Services (Called from native code)
    // -------------------------------------------------------------------------

    /**
     * Request location permission - called from native code
     */
    @Suppress("unused") // Called from JNI
    fun requestLocationPermission() {
        val context = contextRef?.get() ?: return

        if (context is MainActivity) {
            context.runOnUiThread {
                context.requestLocationPermission()
            }
        }
    }

    /**
     * Start receiving location updates - called from native code
     */
    @Suppress("unused") // Called from JNI
    fun startLocationUpdates() {
        val context = contextRef?.get() ?: return

        if (isLocationUpdatesActive) return

        try {
            locationManager = context.getSystemService(Context.LOCATION_SERVICE) as LocationManager

            locationListener = object : LocationListener {
                override fun onLocationChanged(location: Location) {
                    setLocationFull(
                        location.latitude,
                        location.longitude,
                        location.altitude,
                        location.accuracy,
                        location.time
                    )
                }

                override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
                override fun onProviderEnabled(provider: String) {}
                override fun onProviderDisabled(provider: String) {}
            }

            // Check permission
            if (context is MainActivity && context.hasLocationPermission()) {
                locationManager?.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER,
                    1000L, // Update interval in ms
                    1f,    // Minimum distance in meters
                    locationListener!!,
                    Looper.getMainLooper()
                )
                isLocationUpdatesActive = true
            }
        } catch (e: SecurityException) {
            android.util.Log.e(TAG, "Location permission denied", e)
        }
    }

    /**
     * Stop receiving location updates - called from native code
     */
    @Suppress("unused") // Called from JNI
    fun stopLocationUpdates() {
        if (!isLocationUpdatesActive) return

        locationListener?.let {
            locationManager?.removeUpdates(it)
        }
        locationListener = null
        isLocationUpdatesActive = false
    }

    // -------------------------------------------------------------------------
    // System Services (Called from native code)
    // -------------------------------------------------------------------------

    /**
     * Show a toast message - called from native code
     */
    @Suppress("unused") // Called from JNI
    fun showToast(message: String) {
        val context = contextRef?.get() ?: return

        if (context is MainActivity) {
            context.runOnUiThread {
                Toast.makeText(context, message, Toast.LENGTH_SHORT).show()
            }
        }
    }

    /**
     * Trigger device vibration - called from native code
     */
    @Suppress("unused") // Called from JNI
    fun vibrate(durationMs: Long) {
        val context = contextRef?.get() ?: return

        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vibratorManager = context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
            vibratorManager.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            context.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createOneShot(durationMs, VibrationEffect.DEFAULT_AMPLITUDE))
        } else {
            @Suppress("DEPRECATION")
            vibrator.vibrate(durationMs)
        }
    }
}

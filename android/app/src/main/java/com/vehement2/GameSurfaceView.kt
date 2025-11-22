package com.vehement2

import android.content.Context
import android.graphics.PixelFormat
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import javax.microedition.khronos.egl.EGL10
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.egl.EGLContext
import javax.microedition.khronos.egl.EGLDisplay
import javax.microedition.khronos.opengles.GL10

/**
 * Custom GLSurfaceView for Nova3D Engine rendering
 *
 * Handles:
 * - OpenGL ES context creation and management
 * - Touch input forwarding to native code
 * - Surface lifecycle management
 */
class GameSurfaceView : GLSurfaceView {

    private val renderer: GameRenderer

    companion object {
        private const val TAG = "GameSurfaceView"

        // EGL configuration
        private const val EGL_CONTEXT_CLIENT_VERSION = 0x3098
        private const val EGL_OPENGL_ES3_BIT = 0x0040
    }

    constructor(context: Context) : super(context) {
        renderer = GameRenderer()
        init()
    }

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs) {
        renderer = GameRenderer()
        init()
    }

    private fun init() {
        // Set EGL context factory for ES 3.0
        setEGLContextFactory(ContextFactory())

        // Set EGL config chooser
        setEGLConfigChooser(ConfigChooser())

        // Use translucent surface (for potential overlay support)
        holder.setFormat(PixelFormat.TRANSLUCENT)

        // Set renderer
        setRenderer(renderer)

        // Render continuously
        renderMode = RENDERMODE_CONTINUOUSLY

        // Enable touch events
        isFocusable = true
        isFocusableInTouchMode = true
    }

    // -------------------------------------------------------------------------
    // Surface Lifecycle
    // -------------------------------------------------------------------------

    override fun surfaceCreated(holder: SurfaceHolder) {
        super.surfaceCreated(holder)
        Log.d(TAG, "surfaceCreated")
        NativeLib.getInstance().surfaceCreated(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
        NativeLib.getInstance().surfaceDestroyed()
        super.surfaceDestroyed(holder)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        super.surfaceChanged(holder, format, width, height)
        Log.d(TAG, "surfaceChanged: ${width}x${height}")
    }

    // -------------------------------------------------------------------------
    // Touch Input
    // -------------------------------------------------------------------------

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)

        when (action) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                val x = event.getX(pointerIndex)
                val y = event.getY(pointerIndex)
                val pressure = event.getPressure(pointerIndex)
                NativeLib.getInstance().touchWithPressure(action, x, y, pointerId, pressure)
            }

            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP -> {
                val x = event.getX(pointerIndex)
                val y = event.getY(pointerIndex)
                NativeLib.getInstance().touch(action, x, y, pointerId)
            }

            MotionEvent.ACTION_MOVE -> {
                // Process all pointers for move events
                for (i in 0 until event.pointerCount) {
                    val id = event.getPointerId(i)
                    val x = event.getX(i)
                    val y = event.getY(i)
                    val pressure = event.getPressure(i)
                    NativeLib.getInstance().touchWithPressure(action, x, y, id, pressure)
                }
            }

            MotionEvent.ACTION_CANCEL -> {
                NativeLib.getInstance().touch(action, 0f, 0f, 0)
            }
        }

        return true
    }

    // -------------------------------------------------------------------------
    // EGL Context Factory
    // -------------------------------------------------------------------------

    private inner class ContextFactory : EGLContextFactory {
        override fun createContext(egl: EGL10, display: EGLDisplay, config: EGLConfig): EGLContext {
            Log.d(TAG, "Creating OpenGL ES 3.x context")

            // Try ES 3.2 first, then 3.1, then 3.0
            val versions = arrayOf(
                intArrayOf(EGL_CONTEXT_CLIENT_VERSION, 3, EGL10.EGL_NONE), // ES 3.0
            )

            var context = EGL10.EGL_NO_CONTEXT
            for (attribs in versions) {
                context = egl.eglCreateContext(display, config, EGL10.EGL_NO_CONTEXT, attribs)
                if (context != EGL10.EGL_NO_CONTEXT) {
                    Log.d(TAG, "Created OpenGL ES 3.x context successfully")
                    break
                }
            }

            if (context == EGL10.EGL_NO_CONTEXT) {
                Log.e(TAG, "Failed to create OpenGL ES context")
            }

            return context
        }

        override fun destroyContext(egl: EGL10, display: EGLDisplay, context: EGLContext) {
            egl.eglDestroyContext(display, context)
        }
    }

    // -------------------------------------------------------------------------
    // EGL Config Chooser
    // -------------------------------------------------------------------------

    private inner class ConfigChooser : EGLConfigChooser {
        override fun chooseConfig(egl: EGL10, display: EGLDisplay): EGLConfig {
            // Configuration attributes
            val configAttribs = intArrayOf(
                EGL10.EGL_RED_SIZE, 8,
                EGL10.EGL_GREEN_SIZE, 8,
                EGL10.EGL_BLUE_SIZE, 8,
                EGL10.EGL_ALPHA_SIZE, 8,
                EGL10.EGL_DEPTH_SIZE, 24,
                EGL10.EGL_STENCIL_SIZE, 8,
                EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
                EGL10.EGL_NONE
            )

            val numConfigs = IntArray(1)
            egl.eglChooseConfig(display, configAttribs, null, 0, numConfigs)

            if (numConfigs[0] <= 0) {
                Log.e(TAG, "No EGL configs found, trying fallback")
                return chooseFallbackConfig(egl, display)
            }

            val configs = arrayOfNulls<EGLConfig>(numConfigs[0])
            egl.eglChooseConfig(display, configAttribs, configs, numConfigs[0], numConfigs)

            return configs[0] ?: chooseFallbackConfig(egl, display)
        }

        private fun chooseFallbackConfig(egl: EGL10, display: EGLDisplay): EGLConfig {
            // Fallback with minimal requirements
            val fallbackAttribs = intArrayOf(
                EGL10.EGL_RED_SIZE, 5,
                EGL10.EGL_GREEN_SIZE, 6,
                EGL10.EGL_BLUE_SIZE, 5,
                EGL10.EGL_DEPTH_SIZE, 16,
                EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                EGL10.EGL_NONE
            )

            val numConfigs = IntArray(1)
            egl.eglChooseConfig(display, fallbackAttribs, null, 0, numConfigs)

            val configs = arrayOfNulls<EGLConfig>(numConfigs[0])
            egl.eglChooseConfig(display, fallbackAttribs, configs, numConfigs[0], numConfigs)

            return configs[0] ?: throw RuntimeException("Unable to find suitable EGL config")
        }
    }

    // -------------------------------------------------------------------------
    // Renderer
    // -------------------------------------------------------------------------

    private inner class GameRenderer : Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            Log.d(TAG, "onSurfaceCreated")
            // Surface initialization is handled in surfaceCreated callback
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            Log.d(TAG, "onSurfaceChanged: ${width}x${height}")
            NativeLib.getInstance().resize(width, height)
        }

        override fun onDrawFrame(gl: GL10?) {
            // Render frame through native code
            NativeLib.getInstance().step()
        }
    }
}

# Nova3D Engine ProGuard Rules
# ============================================================================

# Keep JNI methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep NativeLib class and all its methods
-keep class com.vehement2.NativeLib {
    *;
}

# Keep MainActivity for potential reflection
-keep class com.vehement2.MainActivity {
    public void requestLocationPermission();
    public boolean hasLocationPermission();
}

# Keep GameSurfaceView
-keep class com.vehement2.GameSurfaceView {
    *;
}

# Keep all classes that might be called from JNI
-keep class com.vehement2.** {
    public <methods>;
}

# Keep annotations
-keepattributes *Annotation*

# Keep generic signatures
-keepattributes Signature

# Keep source file and line number information for debugging
-keepattributes SourceFile,LineNumberTable

# Remove logging in release
-assumenosideeffects class android.util.Log {
    public static *** d(...);
    public static *** v(...);
}

# Keep Kotlin metadata
-keep class kotlin.Metadata { *; }

# Keep Kotlin coroutines
-keepclassmembers class kotlinx.coroutines.** {
    volatile <fields>;
}

# Firebase (if used)
# -keep class com.google.firebase.** { *; }

# Google Play Services Location
-keep class com.google.android.gms.location.** { *; }

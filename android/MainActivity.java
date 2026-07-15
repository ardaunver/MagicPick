package com.ardaunver.magicpick;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

// raylib's AWINDOW_FLAG_FULLSCREEN (set natively in rcore_android.c) hides
// the status bar but doesn't cover modern gesture-navigation insets -- that
// needs an explicit immersive-mode request, which only exists as a Java
// View/WindowInsetsController API, hence this thin subclass instead of the
// stock android.app.NativeActivity.
//
// raylib's native side reads the screen size once, at EGL surface creation
// (which happens after onCreate/onResume but is not itself tied to any
// callback we can hook) and never refreshes it. Hiding the bars only in
// onWindowFocusChanged is too late -- that surface creation can beat it,
// leaving the game permanently sized for the pre-immersive (smaller) area.
// Hiding as early as possible in onCreate gives the surface creation the
// best chance of already seeing the final, full-screen dimensions;
// onWindowFocusChanged stays as a fallback for later focus changes.
public class MainActivity extends NativeActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }

    private void hideSystemBars() {
        View decorView = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            decorView.getWindowInsetsController().setSystemBarsBehavior(
                WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            decorView.getWindowInsetsController().hide(WindowInsets.Type.systemBars());
        } else {
            decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }
}

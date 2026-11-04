// Downhill Revived on Android: SDL's surface with an optional PS2-style touch overlay.
package app.downhillrevived;

import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

public class DhdActivity extends SDLActivity {
    private TouchOverlayView touchOverlay;
    private SettingsOverlayView settingsOverlay;
    private static native void nativeTouchState(int buttons,int lx,int ly,int rx,int ry,int active);
    private static native boolean nativeSaveOption(int option,int value);
    private static native boolean nativeSettingsLoaded();
    private static native String nativeReadOption(int option,boolean saved);
    @Override protected String[] getLibraries() { return new String[] { "SDL3", "main" }; }

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // SDL shows its own error dialog when the libraries failed; don't install a view then.
        if (mLayout == null || mBrokenLibraries) return;
        try {
            Rect bounds = getWindowManager().getMaximumWindowMetrics().getBounds();
            int w = Math.max(bounds.width(), bounds.height()), h = Math.min(bounds.width(), bounds.height());
            if (h > 0) nativeSetenv("DHD_MONITOR_ASPECT", Double.toString((double) w / h));
        } catch (Throwable e) { Log.w("DownhillRevived", "no screen shape: " + e); }
        touchOverlay = new TouchOverlayView(this, new TouchOverlayView.Listener() {
            @Override public void send(TouchPadModel.State s) {nativeTouchState(s.buttons,s.lx,s.ly,s.rx,s.ry,s.active);}
            @Override public void settings() {showTouchSettings();}
        });
        touchOverlay.setControlsVisible(getPreferences(MODE_PRIVATE).getBoolean("touch_visible",true));
        touchOverlay.setOpacity(getPreferences(MODE_PRIVATE).getInt("touch_alpha",145));
        mLayout.addView(touchOverlay, new RelativeLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.MATCH_PARENT));
    }
    @Override protected void onPause() {
        if(touchOverlay!=null)touchOverlay.clear();
        super.onPause();
    }
    private void showTouchSettings() {
        if(touchOverlay==null || isFinishing() || settingsOverlay!=null)return;
        touchOverlay.clear();
        settingsOverlay = new SettingsOverlayView(this,new SettingsOverlayView.Bridge() {
            @Override public boolean ready(){return nativeSettingsLoaded();}
            @Override public String read(int option,boolean saved){return nativeReadOption(option,saved);}
            @Override public boolean save(int option,int value){return nativeSaveOption(option,value);}
            @Override public boolean controlsVisible(){return touchOverlay.controlsVisible();}
            @Override public void controlsVisible(boolean show){
                touchOverlay.setControlsVisible(show);
                getPreferences(MODE_PRIVATE).edit().putBoolean("touch_visible",show).apply();
            }
            @Override public int opacity(){return getPreferences(MODE_PRIVATE).getInt("touch_alpha",145);}
            @Override public void opacity(int value){
                touchOverlay.setOpacity(value);
                getPreferences(MODE_PRIVATE).edit().putInt("touch_alpha",value).apply();
            }
            @Override public void close(){closeSettings();}
        });
        mLayout.addView(settingsOverlay,new RelativeLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.MATCH_PARENT));
    }
    private void closeSettings(){
        if(settingsOverlay==null)return;
        mLayout.removeView(settingsOverlay); settingsOverlay=null;
    }
    @Override public void onBackPressed(){
        if(settingsOverlay!=null){closeSettings();return;}
        super.onBackPressed();
    }
}

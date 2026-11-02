// Downhill Revived on Android: SDL's surface with an optional PS2-style touch overlay.
package app.downhillrevived;

import android.app.AlertDialog;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

public class DhdActivity extends SDLActivity {
    private TouchOverlayView touchOverlay;
    private static native void nativeTouchState(int buttons,int lx,int ly,int rx,int ry,int active);
    private static native boolean nativeSaveOption(int option,int value);
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
    private void saveOption(int option,int value) {
        boolean saved=nativeSaveOption(option,value);
        Toast.makeText(this,saved ? "Tersimpan. Tutup lalu buka lagi game untuk menerapkan." :
            "Belum bisa disimpan. Tunggu game selesai memulai, lalu coba lagi.",Toast.LENGTH_LONG).show();
    }
    private void showTouchSettings() {
        if(touchOverlay==null || isFinishing())return;
        final String[] items={"Grafik ringan: resolusi 1x", "Grafik lebih tajam: resolusi 2x",
            "Tampilkan FPS", "Sembunyikan FPS", "Transparansi kontrol",
            touchOverlay.controlsVisible() ? "Sembunyikan tombol" : "Tampilkan tombol", "Tutup"};
        new AlertDialog.Builder(this).setTitle("Pengaturan Downhill")
            .setItems(items,(dialog,which)->{
                if(which==0)saveOption(1,1);
                else if(which==1)saveOption(1,2);
                else if(which==2)saveOption(2,1);
                else if(which==3)saveOption(2,0);
                else if(which==4)showOpacity();
                else if(which==5){
                    boolean show=!touchOverlay.controlsVisible();touchOverlay.setControlsVisible(show);
                    getPreferences(MODE_PRIVATE).edit().putBoolean("touch_visible",show).apply();
                    Toast.makeText(this,"Ketuk ⚙ untuk membuka pengaturan lagi",Toast.LENGTH_SHORT).show();
                }
            }).show();
    }
    private void showOpacity() {
        final String[] labels={"Tipis", "Sedang", "Tebal"};
        final int[] values={90,145,210};
        new AlertDialog.Builder(this).setTitle("Transparansi tombol")
            .setItems(labels,(dialog,which)->{
                touchOverlay.setOpacity(values[which]);
                getPreferences(MODE_PRIVATE).edit().putInt("touch_alpha",values[which]).apply();
            }).show();
    }
}

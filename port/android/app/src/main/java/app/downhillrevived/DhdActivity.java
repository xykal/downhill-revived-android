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
        boolean live=option==2 || option==3 || option==5;
        Toast.makeText(this,saved ? (live ? "Tersimpan. Berlaku saat permainan berlanjut." :
            "Tersimpan. Tutup lalu buka lagi game untuk menerapkan.") :
            "Belum bisa disimpan. Tunggu game selesai memulai, lalu coba lagi.",Toast.LENGTH_LONG).show();
    }
    private void showTouchSettings() {
        if(touchOverlay==null || isFinishing())return;
        final String[] items={"Layar dan grafik", "Kontrol sentuh", "Performa dan FPS", "Suara", "Film pembuka"};
        new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert).setTitle("PENGATURAN DOWNHILL")
            .setItems(items,(dialog,which)->{
                if(which==0)showDisplay();
                else if(which==1)showControls();
                else if(which==2)showPerformance();
                else if(which==3)showAudio();
                else showMovies();
            }).setNegativeButton("Tutup",null).show();
    }
    private void choose(String title,String[] labels,java.util.function.IntConsumer action) {
        new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert).setTitle(title).setItems(labels,(dialog,i)->action.accept(i))
            .setNegativeButton("Kembali",(dialog,which)->showTouchSettings()).show();
    }
    private void showDisplay() {
        choose("LAYAR DAN GRAFIK",new String[]{
            "Bentuk layar: penuh / lebar / 4:3", "Widescreen: ON / OFF",
            "Resolusi internal: 1x / 2x / layar", "Filter: halus / tajam", "V-Sync: ON / OFF"
        },i->{
            if(i==0)choose("BENTUK LAYAR",new String[]{"Penuh (stretch, gambar melebar)",
                "Lebar (perlu widescreen ON)","Asli 4:3 (garis hitam)"},v->saveOption(3,v));
            else if(i==1)choose("WIDESCREEN",new String[]{"ON", "OFF"},v->saveOption(4,v==0?1:0));
            else if(i==2)choose("RESOLUSI INTERNAL",new String[]{"1x — ringan", "2x — lebih tajam, lebih berat", "Sesuai layar — sangat berat"},v->saveOption(1,v+1));
            else if(i==3)choose("FILTER GAMBAR",new String[]{"Halus", "Tajam"},v->saveOption(5,v));
            else choose("V-SYNC",new String[]{"OFF", "ON"},v->saveOption(7,v));
        });
    }
    private void showControls() {
        choose("KONTROL SENTUH",new String[]{"Transparansi",touchOverlay.controlsVisible() ? "Sembunyikan tombol" : "Tampilkan tombol",
            "Panduan tombol Downhill"},i->{
            if(i==0)showOpacity();
            else if(i==1){boolean show=!touchOverlay.controlsVisible();touchOverlay.setControlsVisible(show);
                getPreferences(MODE_PRIVATE).edit().putBoolean("touch_visible",show).apply();
                Toast.makeText(this,"Ketuk ⚙ untuk membuka pengaturan lagi",Toast.LENGTH_SHORT).show();
            } else new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert).setTitle("TOMBOL DOWNHILL")
                .setMessage("X: mengayuh / sprint\n△: rem\n□ / ○: serang\nR2: lompat\nL1, L2, R1, R2: trik di udara\nL3: powerslide • R3: sprint")
                .setPositiveButton("OK",null).show();
        });
    }
    private void showPerformance() {
        choose("PERFORMA",new String[]{"Target frame rate: 60 / layar", "Penghitung FPS: ON / OFF",
            "Profil waktu tunggu: ON / OFF", "Profil renderer: ON / OFF", "Waktu tunggu CPU (eksperimental)", "Tentang profil dan lag"},i->{
            if(i==0)choose("TARGET FRAME RATE",new String[]{"60 — disarankan", "Ikuti layar"},v->saveOption(9,v));
            else if(i==1)choose("PENGHITUNG FPS",new String[]{"ON", "OFF"},v->saveOption(2,v==0?1:0));
            else if(i==2)choose("PROFIL WAKTU TUNGGU",new String[]{"ON — ada tambahan beban", "OFF"},v->saveOption(11,v==0?1:0));
            else if(i==3)choose("PROFIL RENDERER",new String[]{"ON — ada tambahan beban", "OFF"},v->saveOption(13,v==0?1:0));
            else if(i==4)choose("WAKTU TUNGGU CPU",new String[]{"Bawaan 3 ms", "Eksperimen 1 ms", "Eksperimen 0 ms"},v->saveOption(12,v));
            else new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert).setTitle("PERFORMA")
                .setMessage("60 adalah target, bukan jaminan FPS. 1x memakai lebih sedikit piksel daripada 2x. Profil membantu diagnosa, tetapi bisa mengurangi performa saat aktif. Profil renderer mencatat waktu kerja CPU, bukan waktu GPU.")
                .setPositiveButton("OK",null).show();
        });
    }
    private void showAudio() {
        choose("SUARA",new String[]{"Mode suara saat game lambat", "Buffer suara", "Profil audio: ON / OFF"},i->{
            if(i==0) choose("MODE SUARA",new String[]{"Stretch — kurangi jeda, bisa terdengar bergema",
                "Tanpa stretch — mungkin lebih putus-putus"},v->saveOption(8,v==0?1:0));
            else if(i==1)choose("BUFFER SUARA",new String[]{"70 ms — responsif", "120 ms — lebih banyak bantalan",
                "200 ms — paling banyak bantalan, suara lebih terlambat"},v->saveOption(10,v));
            else choose("PROFIL AUDIO",new String[]{"ON — untuk tes saja", "OFF"},v->saveOption(14,v==0?1:0));
        });
    }
    private void showMovies() {choose("FILM PEMBUKA",new String[]{"Lewati", "Putar bila tersedia"},v->saveOption(6,v==0?1:0));}
    private void showOpacity() {
        choose("TRANSPARANSI TOMBOL",new String[]{"Tipis", "Sedang", "Tebal"},which->{
            final int[] values={90,145,210};touchOverlay.setOpacity(values[which]);
            getPreferences(MODE_PRIVATE).edit().putInt("touch_alpha",values[which]).apply();
        });
    }

}

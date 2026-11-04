package app.downhillrevived;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/** A deliberately self-contained in-game settings panel, not an Android AlertDialog.
 * Displayed values are read from the native ini and startup snapshot; choices that require a
 * restart never masquerade as applied. This view does not change ISO, saves or system settings. */
final class SettingsOverlayView extends FrameLayout {
    interface Bridge {
        boolean ready();
        String read(int option,boolean saved);
        boolean save(int option,int value);
        boolean controlsVisible(); void controlsVisible(boolean show);
        int opacity(); void opacity(int value);
        void close(); void restart();
    }
    private static final int BG=0xff101b25, CARD=0xff192b37, EDGE=0xff324856,
            TEXT=0xffeff7f7, MUTED=0xffa9bbc2, ACCENT=0xff68e0c5, WARN=0xffffc878;
    private final Bridge bridge;
    private final LinearLayout tabs, rows;
    private final TextView message, restartButton, cancelRestart;
    private boolean confirmingRestart;
    private int category=0;
    private final String[] categories={"LAYAR", "PERFORMA", "AUDIO", "KONTROL", "DIAGNOSTIK"};
    private final float dp;

    private static final class Option {
        final int id; final String title,detail; final String[] codes,labels; final int[] values; final boolean live;
        Option(int id,String title,String detail,String[] codes,String[] labels,int[] values,boolean live){
            this.id=id;this.title=title;this.detail=detail;this.codes=codes;this.labels=labels;this.values=values;this.live=live;
        }
        int index(String code){
            if(code==null)return -1;
            if((id==11||id==13||id==14)&&code.isEmpty())code="0";
            if(id==2||id==4||id==6||id==7||id==8){
                if(code.equalsIgnoreCase("off") || code.equalsIgnoreCase("false") || code.equalsIgnoreCase("no"))code="0";
                if(code.equalsIgnoreCase("on") || code.equalsIgnoreCase("true") || code.equalsIgnoreCase("yes"))code="1";
            }
            for(int i=0;i<codes.length;i++)if(codes[i].equalsIgnoreCase(code))return i;
            return -1;
        }
        String label(String code){int i=index(code);return i>=0?labels[i]:code==null?"—":code;}
    }
    private static final Option[] DISPLAY={
        new Option(3,"Bentuk gambar","Penuh mengisi layar, tapi meregangkan proporsi.",
            new String[]{"stretch","wide","4:3"},new String[]{"PENUH", "LEBAR", "4:3"},new int[]{0,1,2},true),
        new Option(4,"Widescreen game","Mengubah sudut gambar; perlu mulai ulang.",
            new String[]{"1","0"},new String[]{"AKTIF","MATI"},new int[]{1,0},false),
        new Option(1,"Resolusi internal","2x lebih tajam namun lebih berat. Ini bukan anti-aliasing.",
            new String[]{"1","2","window"},new String[]{"1×","2×","LAYAR"},new int[]{1,2,3},false),
        new Option(5,"Filter gambar","Halus mengurangi blok; tajam mempertahankan piksel.",
            new String[]{"linear","point"},new String[]{"HALUS","TAJAM"},new int[]{0,1},true),
        new Option(7,"V-Sync","Dapat mengurangi tearing; juga dapat menambah jeda.",
            new String[]{"0","1"},new String[]{"MATI","AKTIF"},new int[]{0,1},false)
    };
    private static final Option[] PERFORMANCE={
        new Option(9,"Target frame rate","Target simulasi, bukan jaminan FPS yang tercapai.",
            new String[]{"60","monitor"},new String[]{"60 FPS","LAYAR"},new int[]{0,1},false),
        new Option(2,"Penghitung FPS","Angka di layar; hasil game dapat berbeda dari fields/s log.",
            new String[]{"0","1"},new String[]{"MATI","AKTIF"},new int[]{0,1},true),
        new Option(12,"Waktu spin CPU","Eksperimen: pengaruhnya pada HP ini belum diukur.",
            new String[]{"3000","1000","0"},new String[]{"3 MS","1 MS","0 MS"},new int[]{0,1,2},false),
        new Option(6,"Film pembuka","Hanya memengaruhi film awal, bukan performa balapan.",
            new String[]{"1","0"},new String[]{"LEWATI","PUTAR"},new int[]{1,0},false)
    };
    private static final Option[] AUDIO={
        new Option(8,"Mode saat game melambat","Stretch bisa bergema; tanpa stretch bisa terputus.",
            new String[]{"1","0"},new String[]{"STRETCH","TANPA"},new int[]{1,0},true),
        new Option(10,"Buffer suara","Lebih besar mungkin mengurangi putus, tetapi menambah jeda.",
            new String[]{"70","120","200"},new String[]{"70 MS","120 MS","200 MS"},new int[]{0,1,2},true)
    };
    private static final Option[] DIAGNOSTICS={
        new Option(11,"Profil waktu tunggu","Merekam spin dan park. Dapat memperlambat saat tes.",
            new String[]{"0","1"},new String[]{"MATI","AKTIF"},new int[]{0,1},false),
        new Option(13,"Profil renderer","Mencatat waktu kerja CPU renderer, bukan waktu GPU.",
            new String[]{"0","1"},new String[]{"MATI","AKTIF"},new int[]{0,1},false),
        new Option(14,"Profil audio","Mencatat antrean audio; gunakan hanya saat tes.",
            new String[]{"0","1"},new String[]{"MATI","AKTIF"},new int[]{0,1},false)
    };
    SettingsOverlayView(Context context,Bridge bridge){
        super(context);this.bridge=bridge;dp=getResources().getDisplayMetrics().density;
        setBackgroundColor(0xd90a1118);setClickable(true);setFocusable(true);setFocusableInTouchMode(true);
        LinearLayout panel=new LinearLayout(context);panel.setOrientation(LinearLayout.VERTICAL);
        panel.setBackground(shape(BG,EDGE,18));panel.setPadding(d(14),d(10),d(14),d(10));
        FrameLayout.LayoutParams panelParams=new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.MATCH_PARENT);
        panelParams.setMargins(d(24),d(12),d(24),d(12));addView(panel,panelParams);
        message=text("Perubahan langsung ditandai; lainnya perlu mulai ulang game.",11,MUTED,false);
        LinearLayout head=new LinearLayout(context);head.setGravity(Gravity.CENTER_VERTICAL);panel.addView(head);
        LinearLayout brand=new LinearLayout(context);brand.setOrientation(LinearLayout.VERTICAL);
        head.addView(brand,new LinearLayout.LayoutParams(0,LayoutParams.WRAP_CONTENT,1));
        brand.addView(text("DOWNHILL / REVIVED",12,ACCENT,true));
        brand.addView(text("PENGATURAN",21,TEXT,true));
        cancelRestart=button("BATAL",0xff2c404b,TEXT);
        cancelRestart.setOnClickListener(v->{confirmingRestart=false;message.setText("Mulai ulang dibatalkan.");message.setTextColor(MUTED);render();});
        head.addView(cancelRestart);
        restartButton=button("MULAI ULANG",WARN,0xff302012);
        LinearLayout.LayoutParams rp=new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT);
        rp.setMargins(d(6),0,d(6),0);head.addView(restartButton,rp);
        restartButton.setOnClickListener(v->{
            if(confirmingRestart){bridge.restart();return;}
            confirmingRestart=true;message.setText("Balapan yang belum disimpan bisa hilang. Tekan YA, ULANG untuk melanjutkan.");message.setTextColor(WARN);render();
        });
        TextView close=button("TUTUP  ✕",0xff2c404b,TEXT);close.setContentDescription("Tutup pengaturan");
        close.setOnClickListener(v->bridge.close());head.addView(close);
        HorizontalScrollView scrollTabs=new HorizontalScrollView(context);scrollTabs.setHorizontalScrollBarEnabled(false);
        LinearLayout.LayoutParams tabWrap=new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.WRAP_CONTENT);
        tabWrap.setMargins(0,d(10),0,d(8));panel.addView(scrollTabs,tabWrap);
        tabs=new LinearLayout(context);tabs.setOrientation(LinearLayout.HORIZONTAL);scrollTabs.addView(tabs);
        ScrollView scroll=new ScrollView(context);scroll.setFillViewport(false);scroll.setVerticalScrollBarEnabled(true);
        panel.addView(scroll,new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,0,1));
        rows=new LinearLayout(context);rows.setOrientation(LinearLayout.VERTICAL);scroll.addView(rows);
        message.setPadding(d(4),d(7),d(4),0);panel.addView(message);
        render();requestFocus();
    }
    private int d(float value){return Math.round(value*dp);}
    private GradientDrawable shape(int fill,int stroke,int radius){
        GradientDrawable g=new GradientDrawable();g.setColor(fill);g.setCornerRadius(d(radius));g.setStroke(d(1),stroke);return g;
    }
    private TextView text(String value,int size,int color,boolean bold){
        TextView t=new TextView(getContext());t.setText(value);t.setTextSize(size);t.setTextColor(color);
        t.setTypeface(Typeface.create("sans-serif",bold?Typeface.BOLD:Typeface.NORMAL));t.setIncludeFontPadding(false);return t;
    }
    private TextView button(String value,int fill,int color){
        TextView t=text(value,12,color,true);t.setGravity(Gravity.CENTER);t.setMinHeight(d(40));
        t.setPadding(d(14),d(7),d(14),d(7));t.setBackground(shape(fill,fill,10));t.setClickable(true);return t;
    }
    private boolean differs(Option option){
        String running=bridge.read(option.id,false),saved=bridge.read(option.id,true);
        if(saved==null)saved=running;
        int a=option.index(running),b=option.index(saved);
        return !(a>=0 && b>=0 ? a==b : running!=null && running.equalsIgnoreCase(saved));
    }
    private boolean restartNeeded(){
        if(!bridge.ready())return false;
        for(Option[] group:new Option[][]{DISPLAY,PERFORMANCE,AUDIO,DIAGNOSTICS})
            for(Option option:group)if(!option.live && differs(option))return true;
        return false;
    }
    private void render(){
        boolean pending=restartNeeded();
        if(!pending)confirmingRestart=false;
        restartButton.setVisibility(pending?View.VISIBLE:View.GONE);
        restartButton.setText(confirmingRestart?"YA, ULANG":"MULAI ULANG");
        cancelRestart.setVisibility(confirmingRestart?View.VISIBLE:View.GONE);
        tabs.removeAllViews();rows.removeAllViews();
        for(int i=0;i<categories.length;i++){
            final int selected=i;boolean active=i==category;
            TextView tab=button(categories[i],active?ACCENT:0xff253743,active?0xff10272a:MUTED);
            LinearLayout.LayoutParams tp=new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT);
            tp.setMargins(0,0,d(6),0);tabs.addView(tab,tp);tab.setOnClickListener(v->{category=selected;render();});
        }
        if(category!=3 && !bridge.ready()){
            TextView loading=text("Game sedang menyiapkan pengaturan. Tutup panel dan coba lagi setelah game terbuka.",15,WARN,false);
            loading.setPadding(d(14),d(14),d(14),d(14));rows.addView(loading);return;
        }
        Option[] list=category==0?DISPLAY:category==1?PERFORMANCE:category==2?AUDIO:category==4?DIAGNOSTICS:null;
        if(list!=null) for(Option option:list)addOption(option);
        else addControls();
    }
    private LinearLayout row(String title,String detail,String status,int statusColor){
        LinearLayout card=new LinearLayout(getContext());card.setOrientation(LinearLayout.HORIZONTAL);card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(d(13),d(10),d(10),d(10));card.setBackground(shape(CARD,EDGE,12));
        LinearLayout info=new LinearLayout(getContext());info.setOrientation(LinearLayout.VERTICAL);
        card.addView(info,new LinearLayout.LayoutParams(0,LayoutParams.WRAP_CONTENT,1));
        info.addView(text(title,15,TEXT,true));
        TextView hint=text(detail,11,MUTED,false);hint.setPadding(0,d(3),d(8),0);info.addView(hint);
        TextView state=text(status,10,statusColor,true);state.setPadding(0,d(5),d(8),0);info.addView(state);
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.WRAP_CONTENT);
        p.setMargins(0,0,0,d(7));rows.addView(card,p);return card;
    }
    private void addOption(Option option){
        String running=bridge.read(option.id,false),saved=bridge.read(option.id,true);
        if(saved==null)saved=running;
        int runIndex=option.index(running),saveIndex=option.index(saved);
        boolean same=runIndex>=0 && saveIndex>=0 ? runIndex==saveIndex : running!=null && running.equalsIgnoreCase(saved);
        String status=same?"TERBACA: "+option.label(running):
            (option.live?(option.id==8 || option.id==10 ? "MENUNGGU AUDIO · TERBACA: " : "MENUNGGU GAME · TERBACA: "):"BUTUH RESTART · TERBACA: ")+option.label(running);
        LinearLayout card=row(option.title,option.detail,status,same?ACCENT:WARN);
        TextView change=button("‹  "+option.label(saved)+"  ›",0xff2c4650,TEXT);
        change.setContentDescription(option.title+", pilihan "+option.label(saved)+", ketuk untuk mengubah");
        final int next=saveIndex<0?0:(saveIndex+1)%option.values.length;
        change.setOnClickListener(v->{
            if(!bridge.save(option.id,option.values[next])){
                message.setText("Gagal menyimpan: game belum siap atau pilihan dikunci lingkungan.");message.setTextColor(WARN);return;
            }
            confirmingRestart=false;
            message.setText(option.live?"Tersimpan. Perubahan sedang diterapkan.":"Tersimpan. Tekan MULAI ULANG jika siap menerapkan.");
            message.setTextColor(option.live?ACCENT:WARN);render();
            if(option.live)refreshLive(option,0);
        });card.addView(change);
    }
    private void refreshLive(Option option,int attempt){
        postDelayed(()->{
            if(!isAttachedToWindow())return;
            boolean waiting=differs(option);render();
            if(waiting && attempt<7)refreshLive(option,attempt+1);
        },500);
    }
    private void addControls(){
        boolean show=bridge.controlsVisible();
        LinearLayout visible=row("Tombol sentuh","Tombol ⚙ tetap tersedia saat kontrol disembunyikan.","LANGSUNG",ACCENT);
        TextView toggle=button(show?"TAMPIL":"SEMBUNYI",0xff2c4650,TEXT);
        toggle.setOnClickListener(v->{bridge.controlsVisible(!show);render();});visible.addView(toggle);
        int a=bridge.opacity();int next=a<115?145:a<180?210:90;
        LinearLayout opacity=row("Transparansi","Atur ketebalan visual kontrol, tanpa mengubah tombol.","LANGSUNG",ACCENT);
        TextView fade=button(a<115?"TIPIS":a<180?"SEDANG":"TEBAL",0xff2c4650,TEXT);
        fade.setOnClickListener(v->{bridge.opacity(next);render();});opacity.addView(fade);
        LinearLayout help=row("Panduan singkat","X mengayuh · △ rem · R2 lompat · □ / ○ serang.","KONTROL GAME",MUTED);
        help.setContentDescription("Panduan tombol");
    }
}

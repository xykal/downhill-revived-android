package app.downhillrevived;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Process;
import android.view.Gravity;
import android.widget.TextView;

/** Runs in a short-lived separate app process, so SDL and the native core really start fresh.
 * This activity is private and is opened only after the player's in-panel confirmation. */
public final class RestartActivity extends Activity {
    static final String OLD_PID="app.downhillrevived.RESTART_OLD_PID";
    @Override protected void onCreate(Bundle state){
        super.onCreate(state);
        TextView status=new TextView(this);status.setText("MEMULAI ULANG DOWNHILL…");
        status.setTextColor(Color.WHITE);status.setTextSize(18);status.setGravity(Gravity.CENTER);
        status.setBackgroundColor(0xff101b25);setContentView(status);
        int oldPid=getIntent().getIntExtra(OLD_PID,-1);
        getIntent().removeExtra(OLD_PID);
        if(oldPid<=0 || oldPid==Process.myPid()){finish();return;}
        Process.killProcess(oldPid);  // same app UID; preferences, saves, ISO URI remain untouched
        Intent launch=new Intent(this,DhdActivity.class);
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(launch);
        finish();
    }
}

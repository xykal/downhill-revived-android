package app.downhillrevived;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

/** Lightweight Android overlay above SDL's SurfaceView: multi-touch PS2-style controls. */
final class TouchOverlayView extends View {
    interface Listener {
        void send(TouchPadModel.State state);
        void settings();
    }
    private final TouchPadModel model = new TouchPadModel();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Listener listener;
    private boolean controlsVisible=true;
    private int alpha=145;
    TouchOverlayView(Context context, Listener listener) {
        super(context); this.listener=listener;
        setFocusable(false); setClickable(true); setWillNotDraw(false);
    }
    void setControlsVisible(boolean value) { controlsVisible=value; clear(); invalidate(); }
    boolean controlsVisible() { return controlsVisible; }
    void setOpacity(int value) {alpha=Math.max(55,Math.min(230,value));invalidate();}
    void clear() {model.clear();listener.send(model.state());invalidate();}
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) {
        super.onSizeChanged(w,h,oldw,oldh); model.resize(w,h); listener.send(model.state());
    }
    private void circle(Canvas c,float x,float y,float r,String text,int color) {
        circle(c,x,y,r,text,color,false);
    }
    private void circle(Canvas c,float x,float y,float r,String text,int color,boolean pressed) {
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(pressed ? Color.argb(Math.min(230,alpha),55,78,100) : Color.argb(alpha/2,12,20,30));
        c.drawCircle(x,y,r,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(2,model.u*.004f));
        int border = color==Color.WHITE ? 0xffdce6eb : color;
        paint.setColor((border & 0x00ffffff) | (Math.min(255,pressed?alpha+90:alpha)<<24));
        c.drawCircle(x,y,r,paint);
        if(text!=null) label(c,text,x,y,model.u*.060f,color);
    }
    private void label(Canvas c,String text,float x,float y,float size,int color) {
        paint.setStyle(Paint.Style.FILL);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(size);
        paint.setFakeBoldText(true);paint.setColor((color & 0x00ffffff) | (Math.min(255,alpha+45)<<24));
        Paint.FontMetrics fm=paint.getFontMetrics();c.drawText(text,x,y-(fm.ascent+fm.descent)/2,paint);
    }
    private void capsule(Canvas c,float cx,float cy,float w,float h,String text) {
        paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(alpha/2,15,22,28));
        c.drawRoundRect(cx-w/2,cy-h/2,cx+w/2,cy+h/2,h*.35f,h*.35f,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(2,model.u*.004f));paint.setColor(Color.argb(alpha,220,230,235));
        c.drawRoundRect(cx-w/2,cy-h/2,cx+w/2,cy+h/2,h*.35f,h*.35f,paint);
        label(c,text,cx,cy,model.u*.042f,Color.WHITE);
    }
    private void stick(Canvas c,int role,float x,float y) {
        circle(c,x,y,model.stickR,null,Color.WHITE);
        TouchPadModel.Contact p=model.contactForRole(role);
        float dx=p==null?0:p.x-x,dy=p==null?0:p.y-y;
        float d=(float)Math.hypot(dx,dy);if(d>model.stickR){dx*=model.stickR/d;dy*=model.stickR/d;}
        circle(c,x+dx,y+dy,model.stickR*.43f,null,Color.WHITE);
    }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        float u=model.u;
        if(controlsVisible) {
            capsule(c,.105f*model.w,.06f*model.h,.15f*model.w,.075f*model.h,"L2");
            capsule(c,.105f*model.w,.18f*model.h,.15f*model.w,.075f*model.h,"L1");
            capsule(c,.895f*model.w,.06f*model.h,.15f*model.w,.075f*model.h,"R2");
            capsule(c,.895f*model.w,.18f*model.h,.15f*model.w,.075f*model.h,"R1");
            capsule(c,.40f*model.w,.10f*model.h,.105f*model.w,.075f*model.h,"SELECT");
            capsule(c,.60f*model.w,.10f*model.h,.105f*model.w,.075f*model.h,"START");
            circle(c,model.dpadX,model.dpadY,u*.17f,null,Color.WHITE);
            label(c,"▲",model.dpadX,model.dpadY-u*.105f,u*.045f,Color.WHITE);
            label(c,"▼",model.dpadX,model.dpadY+u*.105f,u*.045f,Color.WHITE);
            label(c,"◀",model.dpadX-u*.105f,model.dpadY,u*.045f,Color.WHITE);
            label(c,"▶",model.dpadX+u*.105f,model.dpadY,u*.045f,Color.WHITE);
            int buttons=model.state().buttons;
            circle(c,model.faceX,model.faceY-model.faceOffset,model.buttonR,"△",0xff8ee7b3,(buttons&TouchPadModel.TRIANGLE)!=0);
            circle(c,model.faceX+model.faceOffset,model.faceY,model.buttonR,"○",0xffffa3a3,(buttons&TouchPadModel.CIRCLE)!=0);
            circle(c,model.faceX,model.faceY+model.faceOffset,model.buttonR,"×",0xffa2caff,(buttons&TouchPadModel.CROSS)!=0);
            circle(c,model.faceX-model.faceOffset,model.faceY,model.buttonR,"□",0xffffb6e3,(buttons&TouchPadModel.SQUARE)!=0);
            stick(c,TouchPadModel.LEFT_STICK,model.leftX,model.leftY);
            stick(c,TouchPadModel.RIGHT_STICK,model.rightX,model.rightY);
            capsule(c,model.leftX,.964f*model.h,.075f*model.w,.05f*model.h,"L3");
            capsule(c,model.rightX,.964f*model.h,.075f*model.w,.05f*model.h,"R3");
        }
        circle(c,model.gearX,model.gearY,model.gearR,"⚙",Color.WHITE);
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action=event.getActionMasked(),index=event.getActionIndex(),id=event.getPointerId(index);
        boolean gear=false;
        switch(action) {
            case MotionEvent.ACTION_DOWN:
                if(model.down(id,event.getX(index),event.getY(index),controlsVisible)==TouchPadModel.NONE)return false;
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                model.down(id,event.getX(index),event.getY(index),controlsVisible);break;
            case MotionEvent.ACTION_MOVE:
                for(int i=0;i<event.getPointerCount();i++) model.move(event.getPointerId(i),event.getX(i),event.getY(i));
                break;
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_UP:
                gear=model.up(id)==TouchPadModel.GEAR && model.gearHit(event.getX(index),event.getY(index));break;
            case MotionEvent.ACTION_CANCEL:
                model.clear();break;
            default: return true;
        }
        listener.send(model.state());invalidate();
        if(gear){clear();performClick();listener.settings();}
        return true;
    }
    @Override public boolean performClick(){super.performClick();return true;}
}

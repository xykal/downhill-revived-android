package app.downhillrevived;

import java.util.HashMap;
import java.util.Map;

/** Geometry and multitouch controller state, independent of Android UI and unit-testable. */
final class TouchPadModel {
    static final int SELECT=1, L3=2, R3=4, START=8, UP=0x10, RIGHT=0x20, DOWN=0x40, LEFT=0x80;
    static final int L2=0x100, R2=0x200, L1=0x400, R1=0x800;
    static final int TRIANGLE=0x1000, CIRCLE=0x2000, CROSS=0x4000, SQUARE=0x8000;
    static final int NONE=0, LEFT_STICK=1, RIGHT_STICK=2, BUTTON=3, GEAR=4;
    static final class Contact { int role; float x, y; Contact(int r,float px,float py) {role=r;x=px;y=py;} }
    static final class State {
        int buttons, lx=128, ly=128, rx=128, ry=128, active;
    }
    private final Map<Integer,Contact> touches=new HashMap<>();
    float w=1,h=1,u=1;
    float leftX,leftY,rightX,rightY,dpadX,dpadY,faceX,faceY,stickR,buttonR,faceOffset;
    float gearX,gearY,gearR;
    void resize(float width,float height) {
        w=Math.max(1,width); h=Math.max(1,height); u=Math.min(h,w/1.6f);
        leftX=.225f*w;leftY=.78f*h;rightX=.775f*w;rightY=.78f*h;
        dpadX=.095f*w;dpadY=.53f*h;faceX=.905f*w;faceY=.53f*h;
        stickR=.105f*u;buttonR=.075f*u;faceOffset=.145f*u;
        gearX=.5f*w;gearY=.10f*h;gearR=.055f*u;
        touches.clear();
    }
    private static float dist(float x,float y,float cx,float cy) { return (float)Math.hypot(x-cx,y-cy); }
    boolean gearHit(float x,float y) {return dist(x,y,gearX,gearY)<=gearR*1.25f;}
    private int buttonsAt(float x,float y) {
        int b=0;
        float dx=x-dpadX,dy=y-dpadY,range=.17f*u,dead=.045f*u;
        if(Math.abs(dx)<=range && Math.abs(dy)<=range) {
            if(dx < -dead)b|=LEFT;if(dx>dead)b|=RIGHT;
            if(dy < -dead)b|=UP;if(dy>dead)b|=DOWN;
        }
        if(dist(x,y,faceX,faceY-faceOffset)<=buttonR*1.25f)b|=TRIANGLE;
        if(dist(x,y,faceX+faceOffset,faceY)<=buttonR*1.25f)b|=CIRCLE;
        if(dist(x,y,faceX,faceY+faceOffset)<=buttonR*1.25f)b|=CROSS;
        if(dist(x,y,faceX-faceOffset,faceY)<=buttonR*1.25f)b|=SQUARE;
        if(x<.20f*w && y<.24f*h) b|=y<.12f*h ? L2 : L1;
        if(x>.80f*w && y<.24f*h) b|=y<.12f*h ? R2 : R1;
        if(y<.19f*h && x>.34f*w && x<.46f*w)b|=SELECT;
        if(y<.19f*h && x>.54f*w && x<.66f*w)b|=START;
        if(y>.925f*h && x>.18f*w && x<.27f*w)b|=L3;
        if(y>.925f*h && x>.73f*w && x<.82f*w)b|=R3;
        return b;
    }
    int down(int id,float x,float y,boolean controlsVisible) {
        int role=NONE;
        if(gearHit(x,y))role=GEAR;
        else if(controlsVisible && dist(x,y,leftX,leftY)<=stickR*1.35f && !hasRole(LEFT_STICK))role=LEFT_STICK;
        else if(controlsVisible && dist(x,y,rightX,rightY)<=stickR*1.35f && !hasRole(RIGHT_STICK))role=RIGHT_STICK;
        else if(controlsVisible && buttonsAt(x,y)!=0)role=BUTTON;
        if(role!=NONE)touches.put(id,new Contact(role,x,y));
        return role;
    }
    boolean hasRole(int role) { for(Contact c:touches.values())if(c.role==role)return true;return false; }
    void move(int id,float x,float y) {Contact c=touches.get(id);if(c!=null){c.x=x;c.y=y;}}
    int up(int id) {Contact c=touches.remove(id);return c==null?NONE:c.role;}
    void clear(){touches.clear();}
    private int[] axis(Contact c,float cx,float cy) {
        float x=(c.x-cx)/stickR,y=(c.y-cy)/stickR;
        float d=(float)Math.hypot(x,y);
        if(d<.12f)return new int[]{128,128};
        if(d>1){x/=d;y/=d;}
        int ax=Math.max(0,Math.min(255,Math.round(128+x*127)));
        int ay=Math.max(0,Math.min(255,Math.round(128+y*127)));
        return new int[]{ax,ay};
    }
    State state() {
        State out=new State();
        for(Contact c:touches.values()) {
            if(c.role==BUTTON)out.buttons|=buttonsAt(c.x,c.y);
            else if(c.role==LEFT_STICK) {int[] v=axis(c,leftX,leftY);out.lx=v[0];out.ly=v[1];out.active|=1;}
            else if(c.role==RIGHT_STICK) {int[] v=axis(c,rightX,rightY);out.rx=v[0];out.ry=v[1];out.active|=2;}
        }
        return out;
    }
    Contact contactForRole(int role){for(Contact c:touches.values())if(c.role==role)return c;return null;}
}

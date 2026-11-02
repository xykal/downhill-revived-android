package app.downhillrevived;
public final class TouchPadModelTest {
    private static void check(boolean x,String message){if(!x)throw new AssertionError(message);}
    public static void main(String[] args){
        TouchPadModel m=new TouchPadModel(); m.resize(1600,720);
        check(m.down(1,m.leftX,m.leftY,true)==TouchPadModel.LEFT_STICK,"left stick capture");
        m.move(1,m.leftX+m.stickR,m.leftY); TouchPadModel.State s=m.state();
        check(s.lx>=254 && s.ly==128 && s.active==1,"analog right");
        check(m.down(2,m.faceX,m.faceY+m.faceOffset,true)==TouchPadModel.BUTTON,"cross capture");
        s=m.state();check((s.buttons&TouchPadModel.CROSS)!=0 && s.lx>=254,"multi-touch");
        check(m.down(3,m.rightX,m.rightY,true)==TouchPadModel.RIGHT_STICK,"right stick capture");
        m.move(3,m.rightX,m.rightY-m.stickR);s=m.state();check(s.active==3 && s.ry<=2,"two sticks");
        m.up(1);s=m.state();check(s.lx==128 && (s.active&1)==0 && (s.buttons&TouchPadModel.CROSS)!=0,"independent release");
        m.up(2);s=m.state();check(s.buttons==0 && (s.active&2)!=0,"button release");
        m.clear();s=m.state();check(s.buttons==0 && s.active==0 && s.rx==128 && s.ry==128,"cancel releases all");
        check(m.down(4,m.dpadX-.09f*m.u,m.dpadY-.09f*m.u,true)==TouchPadModel.BUTTON,"dpad capture");
        s=m.state();check((s.buttons&(TouchPadModel.LEFT|TouchPadModel.UP))==(TouchPadModel.LEFT|TouchPadModel.UP),"diagonal");
        m.clear();check(m.down(20,m.leftX,.96f*m.h,true)==TouchPadModel.BUTTON,"L3 button capture");
        check((m.state().buttons & TouchPadModel.L3)!=0,"L3 button bit");
        m.clear();check(m.down(21,m.rightX,.96f*m.h,true)==TouchPadModel.BUTTON,"R3 button capture");
        check((m.state().buttons & TouchPadModel.R3)!=0,"R3 button bit");
        m.clear();check(m.down(8,m.gearX,m.gearY,false)==TouchPadModel.GEAR,"gear visible when hidden");
        check(m.down(9,m.faceX,m.faceY,true)==TouchPadModel.NONE,"blank face center not button");
        m.clear();check(m.down(10,m.leftX,m.leftY,false)==TouchPadModel.NONE,"hidden controls");
        m.down(11,m.leftX,m.leftY,true);m.resize(1200,720);check(m.state().active==0,"resize releases");
        System.out.println("PASS: dual sticks, multitouch, D-pad diagonals, release, cancel, hidden and resize");
    }
}

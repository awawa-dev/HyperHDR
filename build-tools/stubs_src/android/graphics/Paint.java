package android.graphics;
public class Paint {
    public static final int ANTI_ALIAS_FLAG = 1;
    public enum Align { CENTER, LEFT, RIGHT }
    public Paint() {}
    public Paint(int flags) {}
    public Shader setShader(Shader shader) { return shader; }
    public void setColor(int color) {}
    public void setTextAlign(Align align) {}
    public void setTextSize(float textSize) {}
}

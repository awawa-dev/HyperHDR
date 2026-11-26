package android.widget;
import android.content.Context;
import android.text.Editable;
public class EditText extends TextView {
    public EditText(Context context) { super(context); }
    public void setInputType(int type) {}
    @Override public Editable getText() { return null; }
    public void setSelection(int index) {}
}

package android.app;
import android.content.*;
import android.view.View;
public class AlertDialog {
    public void show() {}
    public static class Builder {
        public Builder(Context context) {}
        public Builder setTitle(CharSequence title) { return this; }
        public Builder setView(View view) { return this; }
        public Builder setItems(CharSequence[] items, DialogInterface.OnClickListener listener) { return this; }
        public Builder setSingleChoiceItems(CharSequence[] items, int checkedItem, DialogInterface.OnClickListener listener) { return this; }
        public Builder setPositiveButton(CharSequence text, DialogInterface.OnClickListener listener) { return this; }
        public Builder setNeutralButton(CharSequence text, DialogInterface.OnClickListener listener) { return this; }
        public Builder setNegativeButton(CharSequence text, DialogInterface.OnClickListener listener) { return this; }
        public AlertDialog create() { return null; }
        public AlertDialog show() { return null; }
    }
}

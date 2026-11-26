package android.content;
public interface DialogInterface {
    interface OnClickListener {
        void onClick(DialogInterface dialog, int which);
    }
    void dismiss();
}

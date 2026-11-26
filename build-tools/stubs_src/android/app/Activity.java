package android.app;
import android.content.*;
import android.os.Bundle;
import android.view.View;
public class Activity extends Context {
    public static final int RESULT_OK = -1;
    protected void onCreate(Bundle savedInstanceState) {}
    protected void onNewIntent(Intent intent) {}
    protected void onResume() {}
    protected void onPause() {}
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {}
    public Intent getIntent() { return null; }
    public void setIntent(Intent newIntent) {}
    public void startActivityForResult(Intent intent, int requestCode) {}
    public void setContentView(View view) {}
    public boolean moveTaskToBack(boolean nonRoot) { return false; }
}

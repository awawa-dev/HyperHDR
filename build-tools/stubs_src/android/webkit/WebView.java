package android.webkit;
import android.content.Context;
import android.view.ViewGroup;
public class WebView extends ViewGroup {
    public WebView(Context context) { super(context); }
    public WebSettings getSettings() { return null; }
    public void setWebChromeClient(WebChromeClient client) {}
    public void setWebViewClient(WebViewClient client) {}
    public void loadUrl(String url) {}
}

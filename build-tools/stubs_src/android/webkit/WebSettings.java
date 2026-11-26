package android.webkit;
public abstract class WebSettings {
    public static final int MIXED_CONTENT_ALWAYS_ALLOW = 0;
    public abstract void setJavaScriptEnabled(boolean flag);
    public abstract void setDomStorageEnabled(boolean flag);
    public abstract void setDatabaseEnabled(boolean flag);
    public abstract void setLoadWithOverviewMode(boolean overview);
    public abstract void setUseWideViewPort(boolean use);
    public abstract void setBuiltInZoomControls(boolean enabled);
    public abstract void setDisplayZoomControls(boolean enabled);
    public abstract void setMixedContentMode(int mode);
}

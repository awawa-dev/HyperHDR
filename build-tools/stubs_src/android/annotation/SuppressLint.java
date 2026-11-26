package android.annotation;
import java.lang.annotation.*;
@Retention(RetentionPolicy.CLASS)
public @interface SuppressLint { String[] value(); }

import com.android.apksig.ApkSigner;
import com.android.apksig.ApkVerifier;
import java.io.File;
import java.io.FileInputStream;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.util.Collections;

public class SignApk {
    public static void main(String[] args) throws Exception {
        File ksFile = new File(args[0]);
        File inApk = new File(args[1]);
        File outApk = new File(args[2]);

        KeyStore ks = KeyStore.getInstance("JKS");
        try (FileInputStream fis = new FileInputStream(ksFile)) {
            ks.load(fis, "android".toCharArray());
        }
        PrivateKey key = (PrivateKey) ks.getKey("androiddebugkey", "android".toCharArray());
        X509Certificate cert = (X509Certificate) ks.getCertificate("androiddebugkey");

        ApkSigner.SignerConfig signerConfig = new ApkSigner.SignerConfig.Builder(
                "CERT", key, Collections.singletonList(cert)).build();

        ApkSigner signer = new ApkSigner.Builder(Collections.singletonList(signerConfig))
                .setInputApk(inApk)
                .setOutputApk(outApk)
                .setMinSdkVersion(23)
                .setV1SigningEnabled(true)
                .setV2SigningEnabled(true)
                .setV3SigningEnabled(true)
                .build();
        signer.sign();

        ApkVerifier verifier = new ApkVerifier.Builder(outApk).setMinCheckedPlatformVersion(23).build();
        ApkVerifier.Result res = verifier.verify();
        System.out.println("APK Verified: " + res.isVerified()
                + " (v1=" + res.isVerifiedUsingV1Scheme()
                + ", v2=" + res.isVerifiedUsingV2Scheme()
                + ", v3=" + res.isVerifiedUsingV3Scheme() + ")");
    }
}

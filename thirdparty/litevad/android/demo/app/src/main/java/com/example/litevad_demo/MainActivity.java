package com.example.litevad_demo;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import androidx.core.app.ActivityCompat;

public class MainActivity extends Activity {
    private static final int LITEVAD_RESULT_ERROR = -1;
    private static final int LITEVAD_RESULT_FRAME_SILENCE = 0;
    private static final int LITEVAD_RESULT_FRAME_ACTIVE = 1;
    private static final int LITEVAD_RESULT_SPEECH_BEGIN = 2;
    private static final int LITEVAD_RESULT_SPEECH_END = 3;

    private final static String TAG = "LiteVadDemo";
    private long mVadHandle = 0;
    private AudioRecord mAudioRecord;
    Thread mRecordThread;
    private boolean mRecording = false;
    private boolean mStarted = false;
    private int mBuffSize;
    private int mStatus;
    private TextView mStatusView;

    private static final int GET_RECODE_AUDIO = 1;
    private static String[] PERMISSION_AUDIO = {
            Manifest.permission.RECORD_AUDIO
    };
    public static void verifyAudioPermissions(Activity activity) {
        int permission = ActivityCompat.checkSelfPermission(activity,
                Manifest.permission.RECORD_AUDIO);
        if (permission != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(activity, PERMISSION_AUDIO, GET_RECODE_AUDIO);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        verifyAudioPermissions(this);

        mStatus = LITEVAD_RESULT_ERROR;
        mStatusView = findViewById(R.id.statusView);
        mStatusView.setText("VAD Demo");
    }

    @Override
    protected void onDestroy() {
        if (mRecording) {
            stopRecording();
            try {
                mRecordThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        if (mAudioRecord != null) {
            mAudioRecord.stop();
            mAudioRecord.release();
        }
        if (mVadHandle != 0) {
            native_destroy(mVadHandle);
        }
        super.onDestroy();
    }

    public void onStartClick(View view) {
        if (!mStarted) {
            mStarted = true;
            startRecording();
        }
    }

    public void onStopClick(View view) {
        if (mStarted) {
            stopRecording();
            mStarted = false;
        }
    }

    private void startRecording() {
        int sampleRate = 16000;
        int channelConfig = AudioFormat.CHANNEL_IN_MONO;
        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
        int bytesPer10Ms = sampleRate/100*2;
        mBuffSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
        mBuffSize = (mBuffSize/bytesPer10Ms + 1)*bytesPer10Ms;
        mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, channelConfig, audioFormat, mBuffSize);
        mVadHandle = native_create(sampleRate, 1);
        mStatus = LITEVAD_RESULT_ERROR;

        mRecordThread = new Thread(new Runnable() {
            @Override
            public void run() {
                byte[] buffer = new byte[mBuffSize];
                mAudioRecord.startRecording();
                mRecording = true;
                while (mRecording) {
                    int read = mAudioRecord.read(buffer, 0, mBuffSize);
                    if (read > 0) {
                        int ret = native_process(mVadHandle, buffer, read);
                        if (ret != mStatus) {
                            switch (ret) {
                                case LITEVAD_RESULT_ERROR:
                                    Log.i(TAG, "-->LITEVAD_RESULT_ERROR");
                                    break;
                                case LITEVAD_RESULT_FRAME_SILENCE:
                                    Log.i(TAG, "-->LITEVAD_RESULT_FRAME_SILENCE");
                                    break;
                                case LITEVAD_RESULT_FRAME_ACTIVE:
                                    Log.i(TAG, "-->LITEVAD_RESULT_FRAME_ACTIVE");
                                    break;
                                case LITEVAD_RESULT_SPEECH_BEGIN:
                                    Log.i(TAG, "-->LITEVAD_RESULT_SPEECH_BEGIN");
                                    break;
                                case LITEVAD_RESULT_SPEECH_END:
                                    Log.i(TAG, "-->LITEVAD_RESULT_SPEECH_END");
                                    break;
                                default:
                                    break;
                            }
                            mStatus = ret;
                        }
                    }
                }

                native_destroy(mVadHandle);
                mVadHandle = 0;

                mAudioRecord.stop();
                mAudioRecord.release();
                mAudioRecord = null;
                mRecording = false;
            }
        });

        mRecordThread.start();
    }

    public void stopRecording() {
        mRecording = false;
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    private native long native_create(int sampleRate, int channelCount);
    private native int  native_process(long handle, byte[] buff, int size);
    private native void native_reset(long handle) throws IllegalStateException;
    private native void native_destroy(long handle) throws IllegalStateException;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("litevad-jni");
    }
}

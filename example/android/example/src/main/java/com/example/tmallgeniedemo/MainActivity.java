/*
 * Copyright (C) 2023- Qinglong<sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.tmallgeniedemo;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.app.ActivityCompat;

import com.sepnic.tmallgenie.TmallGenie;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import ai.kitt.snowboy.SnowboyDetect;

public class MainActivity extends Activity {
    private static final String TAG = "TmallGenieDemo";
    private final Context mThisContext = this;
    private AudioManager mAudioManager = null;
    private TmallGenie mTmallGenie = null;
    private boolean mIsRecording = false;

    private final String mDeviceBizType = null;  // FIXME: apply for your device key from https://product.aligenie.com/
    private final String mDeviceBizGroup = null; // FIXME: apply for your device key from https://product.aligenie.com/
    private final String mDeviceBizSecret = null;// FIXME: apply for your device key from https://product.aligenie.com/
    private final String mDeviceCaCert = null;   // FIXME: apply for your device key from https://product.aligenie.com/

    private TextView mCommandView;
    private TextView mStatusView;
    private TextView mAsrResultView;
    private TextView mNluResultView;

    static { System.loadLibrary("snowboy-detect-android"); }
    private SnowboyDetect mSnowboyDetect = null;
    private boolean mEnableSnowboyDetect = false;
    private String mSnowboyRes = null;
    private String mSnowboyUmdl = null;
    private static final int SNOWBOY_SAMPLE_RATE     = 16000;
    private static final int SNOWBOY_CHANNEL_COUNT   = 1;
    private static final int SNOWBOY_BITS_PER_SAMPLE = 16;
    //resources/models/snowboy.umdl:
    //    Universal model for the hotword "Snowboy".
    //    Set SetSensitivity to "0.5" and ApplyFrontend to false.
    //resources/models/alexa.umdl:
    //    Universal model for the hotword "Alexa".
    //    Set SetSensitivity to "0.6" and set ApplyFrontend to true.
    //resources/models/jarvis.umdl:
    //    Universal model for the hotword "Jarvis".
    //    It has two different models for the hotword Jarvis,
    //    so you have to use two sensitivites.
    //    Set SetSensitivity to "0.8,0.8" and ApplyFrontend to true.
    private static final String SNOWBOY_RES  = "common.res";
    private static final String SNOWBOY_UMDL = "jarvis.umdl";
    private static final String SNOWBOY_SENSIVIVITY = "0.8,0.8";
    private static final float SNOWBOY_AUDIO_GAIN = 1.0f;
    private static final boolean SNOWBOY_APPLY_FRONTEND = true;

    private static final int PERMISSIONS_REQUEST_CODE_AUDIO = 1;

    private void requestPermissions(Activity activity) {
        // request audio permissions
        if (ActivityCompat.checkSelfPermission(activity, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            String[] PERMISSION_AUDIO = { Manifest.permission.RECORD_AUDIO };
            ActivityCompat.requestPermissions(activity, PERMISSION_AUDIO, PERMISSIONS_REQUEST_CODE_AUDIO);
        }
    }

    @Override
    public void onRequestPermissionsResult(int permsRequestCode, String[] permissions, int[] grantResults) {
        switch (permsRequestCode) {
            case PERMISSIONS_REQUEST_CODE_AUDIO:
                if (grantResults[0] != PackageManager.PERMISSION_GRANTED){
                    Toast.makeText(this, "Failed to request RECORD_AUDIO permission", Toast.LENGTH_LONG).show();
                }
                break;
            default:
                break;
        }
    }

    private String prepareSnowboyResource(String file) {
        try {
            File cacheDir = getCacheDir();
            if (!cacheDir.exists()) {
                boolean res = cacheDir.mkdirs();
                if (!res) {
                    return null;
                }
            }
            File outFile = new File(cacheDir, file);
            if (!outFile.exists()) {
                boolean res = outFile.createNewFile();
                if (!res) {
                    return null;
                }
            } else {
                if (outFile.length() > 0) {
                    return outFile.getAbsolutePath();
                }
            }
            InputStream is = getAssets().open("snowboy/" + file);
            FileOutputStream fos = new FileOutputStream(outFile);
            byte[] buffer = new byte[1024];
            int byteRead = 0;
            while ((byteRead = is.read(buffer)) > 0) {
                fos.write(buffer, 0, byteRead);
            }
            fos.flush();
            is.close();
            fos.close();
            return outFile.getAbsolutePath();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    private final AudioManager.OnAudioFocusChangeListener mAudioFocusChange = new
            AudioManager.OnAudioFocusChangeListener() {
                @Override
                public void onAudioFocusChange(int focusChange) {
                    switch (focusChange) {
                        case AudioManager.AUDIOFOCUS_GAIN:
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS:
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                            break;
                    }
                }
            };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        requestPermissions(this);

        mSnowboyRes = prepareSnowboyResource(SNOWBOY_RES);
        mSnowboyUmdl = prepareSnowboyResource(SNOWBOY_UMDL);

        mAudioManager = (AudioManager) getSystemService(AUDIO_SERVICE);
        if (mAudioManager != null)
            mAudioManager.requestAudioFocus(mAudioFocusChange, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);

        mCommandView = findViewById(R.id.commandView);
        mAsrResultView = findViewById(R.id.asrResultView);
        mNluResultView = findViewById(R.id.nluResultView);
        mStatusView = findViewById(R.id.statusView);

        mTmallGenie = new TmallGenie(this, mDeviceBizType, mDeviceBizGroup, mDeviceBizSecret, mDeviceCaCert);
        mTmallGenie.setCommandListener(mCommandListener);
        mTmallGenie.setStatusListener(mStatusListener);
        mTmallGenie.setAsrResultListener(mAsrResultListener);
        mTmallGenie.setNluResultListener(mNluResultListener);
        mTmallGenie.setMemberQrCodeListener(mMemberQrCodeListener);

        if (!mTmallGenie.startService()) {
            Toast.makeText(this, "Failed to start tmallgenie service", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onDestroy() {
        mTmallGenie.stopVoiceEngine();
        mTmallGenie.stopService();
        mTmallGenie.release();
        if (mAudioManager != null)
            mAudioManager.abandonAudioFocus(mAudioFocusChange);
        super.onDestroy();
    }

    public void onRecordClick(View view) {
        if (!mIsRecording) {
            mTmallGenie.startRecord();
        } else {
            mTmallGenie.stopRecord();
        }
    }

    public void onKeywordDetectClick(View view) {
        if (!mEnableSnowboyDetect) {
            if (enableKeywordDetect()) {
                mEnableSnowboyDetect = true;
                Toast.makeText(this, "Keyword-detect: enabled", Toast.LENGTH_LONG).show();
            } else {
                Toast.makeText(this, "Keyword-detect: failed to enable", Toast.LENGTH_LONG).show();
            }
        } else {
            disableKeywordDetect();
            mEnableSnowboyDetect = false;
            Toast.makeText(this, "Keyword-detect: disabled", Toast.LENGTH_LONG).show();
        }
    }

    @SuppressLint("SetTextI18n")
    private final TmallGenie.OnCommandListener mCommandListener = (domain, command, payload) -> {
        mCommandView.setText("Domain=" + domain + ", Command=" + command + ", Payload=" + payload);
    };

    @SuppressLint("SetTextI18n")
    private final TmallGenie.OnStatusListener mStatusListener = (status) -> {
        switch (status) {
            case TmallGenie.GENIE_STATUS_NetworkDisconnected:
                mStatusView.setText("NetworkDisconnected");
                break;
            case TmallGenie.GENIE_STATUS_NetworkConnected:
                mStatusView.setText("NetworkConnected");
                break;
            case TmallGenie.GENIE_STATUS_GatewayDisconnected:
                mStatusView.setText("GatewayDisconnected");
                break;
            case TmallGenie.GENIE_STATUS_GatewayConnected:
                mStatusView.setText("GatewayConnected");
                break;
            case TmallGenie.GENIE_STATUS_Unauthorized:
                mStatusView.setText("Unauthorized");
                break;
            case TmallGenie.GENIE_STATUS_Authorized:
                mStatusView.setText("Authorized");
                break;
            case TmallGenie.GENIE_STATUS_SpeakerUnmuted:
                mStatusView.setText("SpeakerUnmuted");
                break;
            case TmallGenie.GENIE_STATUS_SpeakerMuted:
                mStatusView.setText("SpeakerMuted");
                break;
            case TmallGenie.GENIE_STATUS_MicphoneWakeup:
                mStatusView.setText("MicphoneWakeup");
                break;
            case TmallGenie.GENIE_STATUS_MicphoneStarted:
                mStatusView.setText("MicphoneStarted");
                mIsRecording = true;
                break;
            case TmallGenie.GENIE_STATUS_MicphoneStopped:
                mStatusView.setText("MicphoneStopped");
                mIsRecording = false;
                break;
            default:
                mStatusView.setText("UnknownError");
                break;
        }
    };

    private final TmallGenie.OnAsrResultListener mAsrResultListener = (result) -> {
        mAsrResultView.setText(result);
    };

    private final TmallGenie.OnNluResultListener mNluResultListener = (result) -> {
        mNluResultView.setText(result);
    };

    private final TmallGenie.OnMemberQrCodeListener mMemberQrCodeListener = (qrcode) -> {
        Toast.makeText(mThisContext, "Please scan the qrcode to bind the device as member, qrcode="+qrcode, Toast.LENGTH_LONG).show();
    };

    private final TmallGenie.OnRecordDataListener mRecordDataListener = (buffer, size) -> {
        if (mEnableSnowboyDetect) {
            if (!mIsRecording && mSnowboyDetect != null) {
                short[] sampleBuffer = new short[size / 2];
                ByteBuffer.wrap(buffer).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(sampleBuffer);
                int result = mSnowboyDetect.RunDetection(sampleBuffer, sampleBuffer.length);
                if (result > 0) {
                    Log.i(TAG, "Keyword detect, onMicphoneWakeup");
                    mTmallGenie.startRecord();
                }
            }
        }
    };

    private boolean enableKeywordDetect() {
        if (mSnowboyDetect == null) {
            if (mSnowboyRes != null && mSnowboyUmdl != null)
                mSnowboyDetect = new SnowboyDetect(mSnowboyRes, mSnowboyUmdl);
            if (mSnowboyDetect == null) {
                Log.e(TAG, "Failed to create SnowboyDetect");
                return false;
            }
            mSnowboyDetect.SetSensitivity(SNOWBOY_SENSIVIVITY);
            mSnowboyDetect.SetAudioGain(SNOWBOY_AUDIO_GAIN);
            mSnowboyDetect.ApplyFrontend(SNOWBOY_APPLY_FRONTEND);
        }
        if (!mTmallGenie.startVoiceEngine(SNOWBOY_SAMPLE_RATE, SNOWBOY_CHANNEL_COUNT, SNOWBOY_BITS_PER_SAMPLE, mRecordDataListener)) {
            return false;
        }
        mSnowboyDetect.Reset();
        return true;
    }

    private void disableKeywordDetect() {
        mTmallGenie.stopVoiceEngine();
    }
}

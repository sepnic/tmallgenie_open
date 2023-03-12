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
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.app.ActivityCompat;

import com.sepnic.tmallgenie.TmallGenie;

import java.io.File;
import java.io.IOException;

public class MainActivity extends Activity {
    private static final String TAG = "TmallGenieDemo";
    private final Context mThisContext = this;
    private AudioManager mAudioManager = null;
    private TmallGenie mTmallGenie = null;
    private TextView mCommandView;
    private TextView mStatusView;
    private TextView mAsrResultView;
    private TextView mNluResultView;
    private String mUserinfoFile = "/storage/emulated/0/TmallGenieUserinfo.txt";
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

    private void createUserinfoFile() {
        try {
            File cacheDir = getCacheDir();
            if (!cacheDir.exists() && !cacheDir.mkdirs()) {
                return;
            }
            File outFile = new File(cacheDir, "TmallGenieUserinfo.txt");
            if (!outFile.exists() && !outFile.createNewFile()) {
                return;
            }
            mUserinfoFile = outFile.getAbsolutePath();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private final AudioManager.OnAudioFocusChangeListener mAudioFocusChange = new
            AudioManager.OnAudioFocusChangeListener() {
                @Override
                public void onAudioFocusChange(int focusChange) {
                    switch (focusChange) {
                        case AudioManager.AUDIOFOCUS_GAIN:
                            if (mTmallGenie != null) {
                                mTmallGenie.native_onSpeakerMutedChanged(false);
                            }
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS:
                            if (mTmallGenie != null) {
                                mTmallGenie.stop();
                            }
                            mAudioManager.abandonAudioFocus(mAudioFocusChange);
                            break;
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                        case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                            if (mTmallGenie != null) {
                                mTmallGenie.native_onSpeakerMutedChanged(true);
                            }
                            break;
                    }
                }
            };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        requestPermissions(this);

        createUserinfoFile();

        mAudioManager = (AudioManager) getSystemService(AUDIO_SERVICE);
        if (mAudioManager != null)
            mAudioManager.requestAudioFocus(mAudioFocusChange, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);

        mCommandView = findViewById(R.id.commandView);
        mAsrResultView = findViewById(R.id.asrResultView);
        mNluResultView = findViewById(R.id.nluResultView);
        mStatusView = findViewById(R.id.statusView);

        mTmallGenie = new TmallGenie();
        mTmallGenie.setCommandListener(mCommandListener);
        mTmallGenie.setStatusListener(mStatusListener);
        mTmallGenie.setAsrResultListener(mAsrResultListener);
        mTmallGenie.setNluResultListener(mNluResultListener);
        mTmallGenie.setMemberQrCodeListener(mMemberQrCodeListener);
        if (mTmallGenie.init(mUserinfoFile, WifiUtils.getMacAddress(this)) && mTmallGenie.start()) {
            // todo: monitor network status
            mTmallGenie.native_onNetworkConnected();
        } else {
            Toast.makeText(this, "Failed to start tmallgenie service", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onDestroy() {
        mTmallGenie.stop();
        mTmallGenie.release();
        if (mAudioManager != null)
            mAudioManager.abandonAudioFocus(mAudioFocusChange);
        super.onDestroy();
    }

    public void onRecordClick(View view) {
        mTmallGenie.native_onTextRecognize("我想听刘德华的歌曲");
    }

    private final TmallGenie.OnCommandListener mCommandListener = new TmallGenie.OnCommandListener() {
        @SuppressLint("SetTextI18n")
        public void onCommand(int domain, int command, String payload) {
            mCommandView.setText("Domain=" + domain + ", Command=" + command + ", Payload=" + payload);
        }
    };

    private final TmallGenie.OnStatusListener mStatusListener = new TmallGenie.OnStatusListener() {
        @SuppressLint("SetTextI18n")
        public void onStatus(int status) {
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
                    break;
                case TmallGenie.GENIE_STATUS_MicphoneStopped:
                    mStatusView.setText("MicphoneStopped");
                    break;
                default:
                    mStatusView.setText("UnknownError");
                    break;
            }
        }
    };

    private final TmallGenie.OnAsrResultListener mAsrResultListener = new TmallGenie.OnAsrResultListener() {
        public void onAsrResult(String result) {
            mAsrResultView.setText(result);
        }
    };

    private final TmallGenie.OnNluResultListener mNluResultListener = new TmallGenie.OnNluResultListener() {
        public void onNluResult(String result) {
            mNluResultView.setText(result);
        }
    };

    private final TmallGenie.OnMemberQrCodeListener mMemberQrCodeListener = new TmallGenie.OnMemberQrCodeListener() {
        public void onMemberQrCode(String qrcode) {
            Toast.makeText(mThisContext, "Please scan the qrcode to bind the device as member, qrcode="+qrcode, Toast.LENGTH_LONG).show();
        }
    };
}

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

package com.sepnic.tmallgenie;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.ref.WeakReference;

public class TmallGenie {
    public static final int GENIE_DOMAIN_Account                    = 0;
    public static final int GENIE_DOMAIN_System                     = 1;
    public static final int GENIE_DOMAIN_Microphone                 = 2;
    public static final int GENIE_DOMAIN_Speaker                    = 3;
    public static final int GENIE_DOMAIN_Text                       = 4;
    public static final int GENIE_DOMAIN_Audio                      = 5;
    public static final int GENIE_DOMAIN_SystemControl              = 6;
    public static final int GENIE_DOMAIN_DotMatrixDisplay           = 7;
    public static final int GENIE_DOMAIN_Network                    = 8;
    public static final int GENIE_DOMAIN_Light                      = 9;
    public static final int GENIE_DOMAIN_Data                       = 10;
    public static final int GENIE_DOMAIN_Alarm                      = 11;

    // AliGenie.Account
    public static final int GENIE_COMMAND_GuestDeviceActivateResp   = 0x0;
    public static final int GENIE_COMMAND_MemberDeviceActivateResp  = 0x1;
    public static final int GENIE_COMMAND_UserInfoResp              = 0x2;
    // AliGenie.Microphone
    public static final int GENIE_COMMAND_StopListen                = 0x10;
    public static final int GENIE_COMMAND_ExpectSpeechStart         = 0x11;
    public static final int GENIE_COMMAND_ExpectSpeechStop          = 0x12;
    // AliGenie.Speaker
    public static final int GENIE_COMMAND_Speak                     = 0x20;
    public static final int GENIE_COMMAND_SetVolume                 = 0x21;
    public static final int GENIE_COMMAND_AdjustVolume              = 0x22;
    public static final int GENIE_COMMAND_SetMute                   = 0x23;
    // AliGenie.Text
    public static final int GENIE_COMMAND_ListenResult              = 0x30;
    // AliGenie.Audio
    public static final int GENIE_COMMAND_Play                      = 0x40;
    public static final int GENIE_COMMAND_PlayOnce                  = 0x41;
    public static final int GENIE_COMMAND_ClearQueue                = 0x42;
    // AliGenie.DotMatrixDisplay
    public static final int GENIE_COMMAND_Render                    = 0x50;
    // AliGenie.Light
    public static final int GENIE_COMMAND_Adjust                    = 0x60;
    // AliGenie.Data
    public static final int GENIE_COMMAND_DataSync                  = 0x70;
    // AliGenie.Alarm
    public static final int GENIE_COMMAND_Set                       = 0x80;
    // AliGenie.Network
    public static final int GENIE_COMMAND_NetworkConfig             = 0x100;
    // AliGenie.System
    public static final int GENIE_COMMAND_Setting                   = 0x1000;
    public static final int GENIE_COMMAND_ThrowException            = 0x1001;
    public static final int GENIE_COMMAND_Success                   = 0x1002;
    // AliGenie.System.Control
    public static final int GENIE_COMMAND_Pause                     = 0x2000;
    public static final int GENIE_COMMAND_Resume                    = 0x2001;
    public static final int GENIE_COMMAND_Exit                      = 0x2002;
    public static final int GENIE_COMMAND_Standby                   = 0x2003;
    public static final int GENIE_COMMAND_Volume                    = 0x2004;

    public static final int GENIE_STATUS_NetworkDisconnected        = 0;
    public static final int GENIE_STATUS_NetworkConnected           = 1;
    public static final int GENIE_STATUS_GatewayDisconnected        = 2;
    public static final int GENIE_STATUS_GatewayConnected           = 3;
    public static final int GENIE_STATUS_Unauthorized               = 4;
    public static final int GENIE_STATUS_Authorized                 = 5;
    public static final int GENIE_STATUS_SpeakerUnmuted             = 6;
    public static final int GENIE_STATUS_SpeakerMuted               = 7;
    public static final int GENIE_STATUS_MicphoneWakeup             = 8;
    public static final int GENIE_STATUS_MicphoneStarted            = 9;
    public static final int GENIE_STATUS_MicphoneStopped            = 10;

    private static final int WHAT_GENIE_COMMAND     = 0x00;
    private static final int WHAT_GENIE_STATUS      = 0x01;
    private static final int WHAT_GENIE_ASRRESULT   = 0x02;
    private static final int WHAT_GENIE_NLURESULT   = 0x03;

    private final static String TAG = "TmallGenieJava";
    private final Context mContext;
    private final AudioManager mAudioManager;
    private final EventHandler mEventHandler;
    private HandlerThread mHandlerThread;

    private final String mBizType;
    private final String mBizGroup;
    private final String mBizSecret;
    private final String mCaCert;
    private String mUserInfoFile = "/storage/emulated/0/TmallGenieUserInfo.txt";
    private String mUuid = null;
    private String mAccessToken = null;

    private boolean mIsCreated = false;
    private boolean mIsStarted = false;

    private void createUserInfoFile(Context context) {
        try {
            File cacheDir = context.getCacheDir();
            if (!cacheDir.exists() && !cacheDir.mkdirs()) {
                return;
            }
            File file = new File(cacheDir, "TmallGenieUserInfo.txt");
            if (!file.exists() && !file.createNewFile()) {
                return;
            }
            mUserInfoFile = file.getAbsolutePath();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void readUserInfoFile(Context context) {
        try {
            FileInputStream fis = new FileInputStream(mUserInfoFile);
            BufferedReader br = new BufferedReader(new InputStreamReader(fis));
            StringBuilder builder = new StringBuilder();
            String line;
            while ((line = br.readLine()) != null) {
                builder.append(line);
            }
            br.close();
            fis.close();

            String content = builder.toString();
            if (!content.isEmpty()) {
                JSONObject root = new JSONObject(content);
                mUuid = root.getString("uuid");
                mAccessToken = root.getString("accessToken");
            }
        } catch (IOException | JSONException e) {
            e.printStackTrace();
        }
    }

    public TmallGenie(Context context, String bizType, String bizGroup, String bizSecret, String caCert) {
        Looper looper;
        if ((looper = Looper.myLooper()) == null && (looper = Looper.getMainLooper()) == null) {
            mHandlerThread = new HandlerThread("TmallGenieEventThread");
            mHandlerThread.start();
            looper = mHandlerThread.getLooper();
        }
        mEventHandler = new EventHandler(looper);

        mContext = context.getApplicationContext();
        mBizType = bizType;
        mBizGroup = bizGroup;
        mBizSecret = bizSecret;
        mCaCert = caCert;

        createUserInfoFile(mContext);
        readUserInfoFile(mContext);

        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

        VolumeChangedReceiver volumeReceiver = new VolumeChangedReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction("android.media.VOLUME_CHANGED_ACTION");
        mContext.registerReceiver(volumeReceiver, filter);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            ConnectivityManager connectivityManager = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
            if (connectivityManager != null) {
                connectivityManager.requestNetwork(new NetworkRequest.Builder().build(), new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onAvailable(Network network) {
                        super.onAvailable(network);
                        native_onNetworkConnected();
                    }

                    @Override
                    public void onLost(Network network) {
                        super.onLost(network);
                        native_onNetworkDisconnected();
                    }
                });
            }
        }
    }

    private class VolumeChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals("android.media.VOLUME_CHANGED_ACTION")) {
                if (mAudioManager != null) {
                    int maxVolume  = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
                    int curVolume = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
                    native_onSpeakerVolumeChanged(curVolume*100/maxVolume);
                }
            }
        }
    }

    private class EventHandler extends Handler {

        public EventHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case WHAT_GENIE_COMMAND:
                    boolean handled = handleCommand(msg.arg1, msg.arg2, (String)msg.obj);
                    if (!handled && mOnCommandListener != null)
                        mOnCommandListener.onCommand(msg.arg1, msg.arg2, (String)msg.obj);
                    break;
                case WHAT_GENIE_STATUS:
                    if (mOnStatusListener != null)
                        mOnStatusListener.onStatus(msg.arg1);
                    break;
                case WHAT_GENIE_ASRRESULT:
                    if (mOnAsrResultListener != null)
                        mOnAsrResultListener.onAsrResult((String)msg.obj);
                    break;
                case WHAT_GENIE_NLURESULT:
                    if (mOnNluResultListener != null)
                        mOnNluResultListener.onNluResult((String)msg.obj);
                    break;
                default:
                    break;
            }
        }
    }

    private boolean handleCommand(int domain, int command, String payload) {
        switch (command) {
            case GENIE_COMMAND_GuestDeviceActivateResp:
            case GENIE_COMMAND_MemberDeviceActivateResp:
                try {
                    JSONObject root = new JSONObject(payload);
                    mUuid = root.getString("uuid");
                    mAccessToken = root.getString("accessToken");
                    FileOutputStream fos = new FileOutputStream(mUserInfoFile);
                    fos.write(payload.getBytes());
                    fos.close();
                } catch (JSONException | IOException e) {
                    e.printStackTrace();
                }
                return true;
            case GENIE_COMMAND_UserInfoResp:
                try {
                    JSONObject root = new JSONObject(payload);
                    String userType = root.getString("userType");
                    if (userType.equals("guest")) {
                        String qrCode = root.getString("qrCode");
                        if (mOnMemberQrCodeListener != null)
                            mOnMemberQrCodeListener.onMemberQrCode(qrCode);
                        return true;
                    }
                } catch (JSONException e) {
                    e.printStackTrace();
                }
                break;
            default:
                break;
        }
        return false;
    }

    /*
     * Called from native code when an interesting event happens.  This method
     * just uses the EventHandler system to post the event back to the main app thread.
     */

    private static int onGetVolumeFromNative(Object tmallgenie_ref) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return -1;
        }
        if (p.mAudioManager != null) {
            int maxVolume  = p.mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
            int curVolume = p.mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
            return curVolume*100/maxVolume;
        }
        return -1;
    }

    private static boolean onSetVolumeFromNative(Object tmallgenie_ref, int volumePercent) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return false;
        }
        if (p.mAudioManager != null) {
            int maxVolume  = p.mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
            int setVolume = volumePercent*maxVolume/100;
            setVolume = setVolume > 0 ? setVolume : 1;
            p.mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, setVolume, 0);
            return true;
        }
        return false;
    }

    private static void onFeedVoiceEngineFromNative(Object tmallgenie_ref, byte[] buffer, int size) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mOnRecordDataListener != null) {
            p.mOnRecordDataListener.onRecordData(buffer, size);
        }
    }

    private static void onCommandFromNative(Object tmallgenie_ref, int domain, int command, String payload) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_COMMAND, domain, command, payload);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onStatusFromNative(Object tmallgenie_ref, int status) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_STATUS, status, 0, null);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onAsrResultFromNative(Object tmallgenie_ref, String result) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_ASRRESULT, 0, 0, result);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onNluResultFromNative(Object tmallgenie_ref, String result) {
        TmallGenie p = (TmallGenie)((WeakReference<?>)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_NLURESULT, 0, 0, result);
            p.mEventHandler.sendMessage(m);
        }
    }

    public boolean startService() throws IllegalArgumentException {
        if (!mIsCreated) {
            String wifiMac = NetworkUtils.getWifiMac(mContext);
            if (wifiMac == null)
                Log.e(TAG, "Unable to get wifi mac, will throw exception");

            if (native_create(new WeakReference<TmallGenie>(this), wifiMac, mBizType, mBizGroup, mBizSecret, mCaCert, mUuid, mAccessToken))
                mIsCreated = true;
            else
                Log.e(TAG, "Failed to init genie service");
        }

        if (mIsCreated && !mIsStarted) {
            if (native_start()) {
                mIsStarted = true;
                if (mAudioManager != null) {
                    int maxVolume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
                    int curVolume = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
                    native_onSpeakerVolumeChanged(curVolume * 100 / maxVolume);
                }
                if (NetworkUtils.isMobileConnected(mContext) || NetworkUtils.isWifiConnected(mContext))
                    native_onNetworkConnected();
                else
                    Log.e(TAG, "Unable to send NetworkConnected event because network disconnected");
            } else {
                Log.e(TAG, "Failed to start genie service");
            }
        }
        return mIsStarted;
    }

    public void stopService() {
        if (mIsStarted) {
            native_stop();
            mIsStarted = false;
        }
    }

    public void release() {
        if (mHandlerThread != null)
            mHandlerThread.quitSafely();
        native_destroy();
        mOnCommandListener = null;
        mOnStatusListener = null;
        mOnAsrResultListener = null;
        mOnNluResultListener = null;
        mOnMemberQrCodeListener = null;
        mOnRecordDataListener = null;
        mIsCreated = false;
        mIsStarted = false;
    }

    public void startRecord() { native_onMicphoneWakeup("tian mao jing ling", 0, 0.618); }

    public void stopRecord() { native_onMicphoneSilence(); }

    public void mute() { native_onSpeakerMutedChanged(true); }

    public void unmute() { native_onSpeakerMutedChanged(false); }

    public boolean startVoiceEngine(int sampleRate, int ChannelCount, int bitsPerSample, OnRecordDataListener listener) {
        mOnRecordDataListener = listener;
        return native_startVoiceEngine(sampleRate, ChannelCount, bitsPerSample);
    }

    public void stopVoiceEngine() {
        native_stopVoiceEngine();
        mOnRecordDataListener = null;
    }

    public interface OnCommandListener { void onCommand(int domain, int command, String payload); }
    public void setCommandListener(OnCommandListener listener) { mOnCommandListener = listener; }
    private OnCommandListener mOnCommandListener = null;

    public interface OnStatusListener { void onStatus(int status); }
    public void setStatusListener(OnStatusListener listener) { mOnStatusListener = listener; }
    private OnStatusListener mOnStatusListener = null;

    public interface OnAsrResultListener { void onAsrResult(String result); }
    public void setAsrResultListener(OnAsrResultListener listener) { mOnAsrResultListener = listener; }
    private OnAsrResultListener mOnAsrResultListener = null;

    public interface OnNluResultListener { void onNluResult(String result); }
    public void setNluResultListener(OnNluResultListener listener) { mOnNluResultListener = listener; }
    private OnNluResultListener mOnNluResultListener = null;

    public interface OnMemberQrCodeListener { void onMemberQrCode(String qrcode); }
    public void setMemberQrCodeListener(OnMemberQrCodeListener listener) { mOnMemberQrCodeListener = listener; }
    private OnMemberQrCodeListener mOnMemberQrCodeListener = null;

    public interface OnRecordDataListener { void onRecordData(byte[] buffer, int size); }
    private OnRecordDataListener mOnRecordDataListener = null;

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    private native boolean native_create(Object tmallgenie_this, String wifiMac,
                                         String bizType, String bizGroup, String bizSecret, String caCert,
                                         String uuid, String accessToken) throws IllegalArgumentException;
    private native void native_destroy();
    private native boolean native_start();
    private native void native_stop();

    private native void native_onMicphoneWakeup(String wakeupWord, int doa, double confidence) throws IllegalArgumentException;
    private native void native_onMicphoneSilence();
    private native void native_onSpeakerVolumeChanged(int volume);
    private native void native_onSpeakerMutedChanged(boolean muted);

    public native void native_onNetworkConnected();
    public native void native_onNetworkDisconnected();

    public native void native_onQueryUserInfo();
    public native void native_onTextRecognize(String inputText) throws IllegalArgumentException;

    private native boolean native_startVoiceEngine(int sampleRate, int ChannelCount, int bitsPerSample);
    private native void native_stopVoiceEngine();

    // Used to load the 'native-lib' library on application startup.
    static { System.loadLibrary("tmallgenie-jni"); }
}

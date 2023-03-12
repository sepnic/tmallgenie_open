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

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

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
    private static final int WHAT_GENIE_QRCODE      = 0x04;

    private final static String TAG = "TmallGenieJava";
    private final EventHandler mEventHandler;
    private HandlerThread mHandlerThread;
    private boolean mIsInited = false;

    public TmallGenie() {
        Looper looper;
        if ((looper = Looper.myLooper()) == null && (looper = Looper.getMainLooper()) == null) {
            mHandlerThread = new HandlerThread("TmallGenieEventThread");
            mHandlerThread.start();
            looper = mHandlerThread.getLooper();
        }
        mEventHandler = new EventHandler(this, looper);
    }

    private class EventHandler extends Handler {

        public EventHandler(TmallGenie p, Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case WHAT_GENIE_COMMAND:
                    if (mOnCommandListener != null)
                        mOnCommandListener.onCommand(msg.arg1, msg.arg2, (String)msg.obj);
                    return;
                case WHAT_GENIE_STATUS:
                    if (mOnStatusListener != null)
                        mOnStatusListener.onStatus(msg.arg1);
                    return;
                case WHAT_GENIE_ASRRESULT:
                    if (mOnAsrResultListener != null)
                        mOnAsrResultListener.onAsrResult((String)msg.obj);
                    return;

                case WHAT_GENIE_NLURESULT:
                    if (mOnNluResultListener != null)
                        mOnNluResultListener.onNluResult((String)msg.obj);
                    return;

                case WHAT_GENIE_QRCODE:
                    if (mOnMemberQrCodeListener != null)
                        mOnMemberQrCodeListener.onMemberQrCode((String)msg.obj);
                    return;

                default:
                    return;
            }
        }
    }

    /*
     * Called from native code when an interesting event happens.  This method
     * just uses the EventHandler system to post the event back to the main app thread.
     */
    private static void onCommandFromNative(Object tmallgenie_ref, int domain, int command, String payload) {
        TmallGenie p = (TmallGenie)((WeakReference)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_COMMAND, domain, command, payload);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onStatusFromNative(Object tmallgenie_ref, int status) {
        TmallGenie p = (TmallGenie)((WeakReference)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_STATUS, status, 0, null);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onAsrResultFromNative(Object tmallgenie_ref, String result) {
        TmallGenie p = (TmallGenie)((WeakReference)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_ASRRESULT, 0, 0, result);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onNluResultFromNative(Object tmallgenie_ref, String result) {
        TmallGenie p = (TmallGenie)((WeakReference)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_NLURESULT, 0, 0, result);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static void onMemberQrCodeFromNative(Object tmallgenie_ref, String qrcode) {
        TmallGenie p = (TmallGenie)((WeakReference)tmallgenie_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(WHAT_GENIE_QRCODE, 0, 0, qrcode);
            p.mEventHandler.sendMessage(m);
        }
    }

    public interface OnCommandListener {
        void onCommand(int domain, int command, String payload);
    }
    public void setCommandListener(OnCommandListener listener) {
        mOnCommandListener = listener;
    }
    private OnCommandListener mOnCommandListener = null;

    public interface OnStatusListener {
        void onStatus(int status);
    }
    public void setStatusListener(OnStatusListener listener) {
        mOnStatusListener = listener;
    }
    private OnStatusListener mOnStatusListener = null;

    public interface OnAsrResultListener {
        void onAsrResult(String result);
    }
    public void setAsrResultListener(OnAsrResultListener listener) {
        mOnAsrResultListener = listener;
    }
    private OnAsrResultListener mOnAsrResultListener = null;

    public interface OnNluResultListener {
        void onNluResult(String result);
    }
    public void setNluResultListener(OnNluResultListener listener) {
        mOnNluResultListener = listener;
    }
    private OnNluResultListener mOnNluResultListener = null;

    public interface OnMemberQrCodeListener {
        void onMemberQrCode(String qrcode);
    }
    public void setMemberQrCodeListener(OnMemberQrCodeListener listener) {
        mOnMemberQrCodeListener = listener;
    }
    private OnMemberQrCodeListener mOnMemberQrCodeListener = null;

    public boolean init(String userinfoFile, String wifiMac) throws IllegalArgumentException {
        if (!mIsInited) {
            if (native_create(new WeakReference<TmallGenie>(this), userinfoFile, wifiMac))
                mIsInited = true;
        }
        return mIsInited;
    }

    public boolean start() {
        if (mIsInited)
            return native_start();
        else
            return false;
    }

    public void stop() {
        if (mIsInited)
            native_stop();
    }

    public void release() {
        if (mIsInited)
            native_destroy();
        if (mHandlerThread != null) {
            mHandlerThread.quitSafely();
        }
        mOnCommandListener = null;
        mOnStatusListener = null;
        mOnAsrResultListener = null;
        mOnNluResultListener = null;
        mOnMemberQrCodeListener = null;
        mIsInited = false;
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    private native boolean native_create(Object tmallgenie_this, String userinfoFile, String wifiMac)
            throws IllegalArgumentException;
    private native void native_destroy();
    private native boolean native_start();
    private native void native_stop();

    public native void native_onNetworkConnected();
    public native void native_onNetworkDisconnected();
    public native void native_onMicphoneWakeup(String wakeupWord, int doa, double confidence) throws IllegalArgumentException;
    public native void native_onMicphoneSilence();
    public native void native_onSpeakerVolumeChanged(int volume);
    public native void native_onSpeakerMutedChanged(boolean muted);
    public native void native_onQueryUserInfo();
    public native void native_onTextRecognize(String inputText) throws IllegalArgumentException;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("tmallgenie-jni");
    }
}

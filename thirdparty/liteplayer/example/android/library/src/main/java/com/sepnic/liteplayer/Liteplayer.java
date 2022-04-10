/*
 * Copyright (C) 2019-2022 Qinglong<sysu.zqlong@gmail.com>
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

package com.sepnic.liteplayer;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.media.AudioManager;
import android.media.AudioFormat;
import android.media.AudioTrack;
import java.lang.ref.WeakReference;

public class Liteplayer {
    private static final int LITEPLAYER_IDLE            = 0x00;
    private static final int LITEPLAYER_INITED          = 0x01;
    private static final int LITEPLAYER_PREPARED        = 0x02;
    private static final int LITEPLAYER_STARTED         = 0x03;
    private static final int LITEPLAYER_PAUSED          = 0x04;
    private static final int LITEPLAYER_SEEKCOMPLETED   = 0x05;
    private static final int LITEPLAYER_NEARLYCOMPLETED = 0x06;
    private static final int LITEPLAYER_COMPLETED       = 0x07;
    private static final int LITEPLAYER_STOPPED         = 0x08;
    private static final int LITEPLAYER_ERROR           = 0xFF;

    private final static String TAG = "LitelayerJava";
    private long mPlayerHandle;
    private EventHandler mEventHandler;
    private HandlerThread mHandlerThread;
    private AudioTrack mAudioTrack;
    private boolean mTrackTriggered;

    public Liteplayer() {
        Looper looper;
        if ((looper = Looper.myLooper()) == null && (looper = Looper.getMainLooper()) == null) {
            // Create our own looper here in case MP was created without one
            mHandlerThread = new HandlerThread("LiteplayerEventThread");
            mHandlerThread.start();
            looper = mHandlerThread.getLooper();
        }
        mEventHandler = new EventHandler(this, looper);
        mPlayerHandle = native_create(new WeakReference<Liteplayer>(this));
    }

    private class EventHandler extends Handler {
        private Liteplayer mLiteplayer;

        public EventHandler(Liteplayer p, Looper looper) {
            super(looper);
            mLiteplayer = p;
        }

        @Override
        public void handleMessage(Message msg) {
            if (mLiteplayer.mPlayerHandle == 0) {
                Log.w(TAG, "liteplayer went away with unhandled events");
                return;
            }
            switch(msg.what) {
                case LITEPLAYER_IDLE:
                    Log.i(TAG, "-->LITEPLAYER_IDLE");
                    if (mOnIdleListener != null)
                        mOnIdleListener.onIdle(mLiteplayer);
                    return;

                case LITEPLAYER_PREPARED:
                    Log.i(TAG, "-->LITEPLAYER_PREPARED");
                    if (mOnPreparedListener != null)
                        mOnPreparedListener.onPrepared(mLiteplayer);
                    return;

                case LITEPLAYER_STARTED:
                    Log.i(TAG, "-->LITEPLAYER_STARTED");
                    if (mOnStartedListener != null)
                        mOnStartedListener.onStarted(mLiteplayer);
                    return;

                case LITEPLAYER_PAUSED:
                    Log.i(TAG, "-->LITEPLAYER_PAUSED");
                    if (mOnPausedListener != null)
                        mOnPausedListener.onPaused(mLiteplayer);
                    return;

                case LITEPLAYER_SEEKCOMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_SEEKCOMPLETED");
                    if (mOnSeekCompletedListener != null)
                        mOnSeekCompletedListener.onSeekCompleted(mLiteplayer);
                    return;

                case LITEPLAYER_NEARLYCOMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
                    if (mOnNearlyCompletedListener != null)
                        mOnNearlyCompletedListener.onNearlyCompleted(mLiteplayer);
                    return;

                case LITEPLAYER_COMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_COMPLETED");
                    if (mOnCompletedListener != null)
                        mOnCompletedListener.onCompleted(mLiteplayer);
                    return;

                case LITEPLAYER_STOPPED:
                    Log.i(TAG, "-->LITEPLAYER_STOPPED");
                    if (mOnStoppedListener != null)
                        mOnStoppedListener.onStopped(mLiteplayer);
                    return;

                case LITEPLAYER_ERROR:
                    Log.e(TAG, "-->LITEPLAYER_ERROR: (" + msg.arg1 + "," + msg.arg2 + ")");
                    if (mOnErrorListener != null)
                        mOnErrorListener.onError(mLiteplayer, msg.arg1, msg.arg2);
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
    private static void postEventFromNative(Object liteplayer_ref, int state, int errno) {
        Liteplayer p = (Liteplayer)((WeakReference)liteplayer_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(state, errno, 0, null);
            p.mEventHandler.sendMessage(m);
        }
    }

    private static int openAudioTrackFromNative(Object liteplayer_ref, int sampleRateInHz, int numberOfChannels, int sampleBits) {
        Liteplayer p = (Liteplayer)((WeakReference)liteplayer_ref).get();
        if (p == null) {
            return -1;
        }

        if (sampleBits != 16) {
            Log.e(TAG, "Unsupported sample bits: " + sampleBits);
            return -1;
        }

        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
        int channelConfig;
        if (numberOfChannels == 1) {
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_MONO;
        } else if(numberOfChannels == 2) {
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            Log.e(TAG, "Unsupported channel count: " + numberOfChannels);
            return -1;
        }

        int bufferSizeInBytes = AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, audioFormat);
        p.mAudioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC,
                sampleRateInHz, channelConfig, audioFormat,
                bufferSizeInBytes, AudioTrack.MODE_STREAM);
        return 0;
    }

    private static int writeAudioTrackFromNative(Object liteplayer_ref, byte[] audioData, int sizeInBytes) {
        Liteplayer p = (Liteplayer)((WeakReference)liteplayer_ref).get();
        if (p == null || p.mAudioTrack == null) {
            return -1;
        }

        if (!p.mTrackTriggered) {
            p.mAudioTrack.play();
            p.mTrackTriggered = true;
        }
        return p.mAudioTrack.write(audioData, 0, sizeInBytes);
    }

    private static void closeAudioTrackFromNative(Object liteplayer_ref) {
        Liteplayer p = (Liteplayer)((WeakReference)liteplayer_ref).get();
        if (p == null || p.mAudioTrack == null) {
            return;
        }

        p.mTrackTriggered = false;
        p.mAudioTrack.stop();
        p.mAudioTrack.release();
        p.mAudioTrack = null;
    }

    public interface OnIdleListener {
        /**
         * Called when the player is idle.
         */
        void onIdle(Liteplayer p);
    }
    public void setOnIdleListener(OnIdleListener listener) {
        mOnIdleListener = listener;
    }
    private OnIdleListener mOnIdleListener;

    public interface OnPreparedListener {
        /**
         * Called when the media file is ready for playback.
         */
        void onPrepared(Liteplayer p);
    }
    public void setOnPreparedListener(OnPreparedListener listener) {
        mOnPreparedListener = listener;
    }
    private OnPreparedListener mOnPreparedListener;

    public interface OnStartedListener {
        /**
         * Called when the player is started.
         */
        void onStarted(Liteplayer p);
    }
    public void setOnStartedListener(OnStartedListener listener) {
        mOnStartedListener = listener;
    }
    private OnStartedListener mOnStartedListener;

    public interface OnPausedListener {
        /**
         * Called when the player is paused.
         */
        void onPaused(Liteplayer p);
    }
    public void setOnPausedListener(OnPausedListener listener) {
        mOnPausedListener = listener;
    }
    private OnPausedListener mOnPausedListener;

    public interface OnSeekCompletedListener {
        /**
         * Called when the player is seek completed.
         */
        void onSeekCompleted(Liteplayer p);
    }
    public void setOnSeekCompletedListener(OnSeekCompletedListener listener) {
        mOnSeekCompletedListener = listener;
    }
    private OnSeekCompletedListener mOnSeekCompletedListener;

    public interface OnNearlyCompletedListener {
        /**
         * Called when the player is nearly completed.
         */
        void onNearlyCompleted(Liteplayer p);
    }
    public void setOnNearlyCompletedListener(OnNearlyCompletedListener listener) {
        mOnNearlyCompletedListener = listener;
    }
    private OnNearlyCompletedListener mOnNearlyCompletedListener;

    public interface OnCompletedListener {
        /**
         * Called when the player reached the end of the file.
         */
        void onCompleted(Liteplayer p);
    }
    public void setOnCompletedListener(OnCompletedListener listener) {
        mOnCompletedListener = listener;
    }
    private OnCompletedListener mOnCompletedListener;

    public interface OnStoppedListener {
        /**
         * Called when the player is stopped.
         */
        void onStopped(Liteplayer p);
    }
    public void setOnStoppedListener(OnStoppedListener listener) {
        mOnStoppedListener = listener;
    }
    private OnStoppedListener mOnStoppedListener;

    public interface OnErrorListener {
        /**
         * Called when the player encounter error.
         */
        void onError(Liteplayer p, int what, int extra);
    }
    public void setOnErrorListener(OnErrorListener listener) {
        mOnErrorListener = listener;
    }
    private OnErrorListener mOnErrorListener;

    public void release() throws IllegalStateException {
        native_destroy(mPlayerHandle);
        mPlayerHandle = 0;
        if (mHandlerThread != null) {
            mHandlerThread.quitSafely();
        }
        mOnIdleListener = null;
        mOnPreparedListener = null;
        mOnStartedListener = null;
        mOnPausedListener = null;
        mOnNearlyCompletedListener = null;
        mOnCompletedListener = null;
        mOnStoppedListener = null;
        mOnErrorListener = null;
    }

    public int setDataSource(String path) throws IllegalStateException, IllegalArgumentException {
        return native_setDataSource(mPlayerHandle, path);
    }

    public int prepareAsync() throws IllegalStateException {
        return native_prepareAsync(mPlayerHandle);
    }

    public int start() throws IllegalStateException {
        return native_start(mPlayerHandle);
    }

    public int pause() throws IllegalStateException {
        return native_pause(mPlayerHandle);
    }

    public int resume() throws IllegalStateException {
        return native_resume(mPlayerHandle);
    }

    public int seekTo(int msec) throws IllegalStateException {
        return native_seekTo(mPlayerHandle, msec);
    }

    public int stop() throws IllegalStateException {
        return native_stop(mPlayerHandle);
    }

    public int reset() throws IllegalStateException {
        return native_reset(mPlayerHandle);
    }

    public int getCurrentPosition() throws IllegalStateException {
        return native_getCurrentPosition(mPlayerHandle);
    }

    public int getDuration() throws IllegalStateException {
        return native_getDuration(mPlayerHandle);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    private native long native_create(Object liteplayer_this);
    private native void native_destroy(long handle) throws IllegalStateException;
    private native int native_setDataSource(long handle, String path) throws IllegalStateException, IllegalArgumentException;
    private native int native_prepareAsync(long handle) throws IllegalStateException;
    private native int native_start(long handle) throws IllegalStateException;
    private native int native_pause(long handle) throws IllegalStateException;
    private native int native_resume(long handle) throws IllegalStateException;
    private native int native_seekTo(long handle, int msec) throws IllegalStateException;
    private native int native_stop(long handle) throws IllegalStateException;
    private native int native_reset(long handle) throws IllegalStateException;
    private native int native_getCurrentPosition(long handle) throws IllegalStateException;
    private native int native_getDuration(long handle) throws IllegalStateException;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("liteplayer-jni");
    }
}

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

package com.example.liteplayerdemo;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;
import androidx.core.app.ActivityCompat;

import com.sepnic.liteplayer.Liteplayer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends Activity {
    private final static String TAG = "LiteplayerDemo";
    private Liteplayer mLiteplayer;
    private TextView mStatusView;
    private int mStatus;

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

    // Storage Permissions
    //private static final int REQUEST_EXTERNAL_STORAGE = 1;
    //private static final String[] PERMISSIONS_STORAGE = {
    //        Manifest.permission.READ_EXTERNAL_STORAGE,
    //        Manifest.permission.WRITE_EXTERNAL_STORAGE
    //};
    /**
     * Checks if the app has permission to write to device storage
     * If the app does not has permission then the user will be prompted to
     * grant permissions
     */
    //private void verifyStoragePermissions(Activity activity) {
        // Check if we have write permission
    //    int permission = ActivityCompat.checkSelfPermission(activity, Manifest.permission.WRITE_EXTERNAL_STORAGE);
    //    if (permission != PackageManager.PERMISSION_GRANTED) {
            // We don't have permission so prompt the user
    //        ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE, REQUEST_EXTERNAL_STORAGE);
    //    }
    //}

    private String getCacheMusicFromAsset(String assetFile) {
        try {
            File cacheDir = getCacheDir();
            if (!cacheDir.exists()) {
                boolean res = cacheDir.mkdirs();
                if (!res) {
                    return "";
                }
            }
            File outFile = new File(cacheDir, assetFile);
            if (!outFile.exists()) {
                boolean res = outFile.createNewFile();
                if (!res) {
                    return "";
                }
            } else {
                if (outFile.length() > 0) {
                    return outFile.getAbsolutePath();
                }
            }
            InputStream is = getAssets().open(assetFile);
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
        return "";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //verifyStoragePermissions(this);

        mStatusView = findViewById(R.id.statusView);
        mStatusView.setText("Idle");
        mStatus = LITEPLAYER_IDLE;

        mLiteplayer = new Liteplayer();
        mLiteplayer.setOnIdleListener(mIdleListener);
        mLiteplayer.setOnPreparedListener(mPreparedListener);
        mLiteplayer.setOnStartedListener(mStartedListener);
        mLiteplayer.setOnPausedListener(mPausedListener);
        mLiteplayer.setOnSeekCompletedListener(mSeekCompletedListener);
        mLiteplayer.setOnCompletedListener(mCompletedListener);
        mLiteplayer.setOnErrorListener(mErrorListener);
    }

    @Override
    protected void onDestroy() {
        mLiteplayer.release();
        super.onDestroy();
    }
/*
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_UP:
                Log.d(TAG, "Receive KEYCODE_VOLUME_UP, start/pause playing");
                if (mStatus == LITEPLAYER_IDLE) {
                    //String music = "http://ailabsaicloudservice.alicdn.com/player/resources/23a2d715f019c0e345235f379fa26a30.mp3";
                    String music = getCacheMusicFromAsset("test.m4a");
                    mLiteplayer.setDataSource(music);
                    mLiteplayer.prepareAsync();
                } if (mStatus == LITEPLAYER_STARTED) {
                    mLiteplayer.pause();
                }
                else if (mStatus == LITEPLAYER_PAUSED || mStatus == LITEPLAYER_SEEKCOMPLETED) {
                    mLiteplayer.resume();
                }
                return true;
            case KeyEvent.KEYCODE_VOLUME_DOWN:
                Log.d(TAG, "Receive KEYCODE_VOLUME_DOWN, stop playing");
                if (mStatus != LITEPLAYER_IDLE) {
                    mLiteplayer.reset();
                }
                return true;
        }
        return super.onKeyDown(keyCode, event);
    }
*/
    public void onStartClick(View view) {
        if (mStatus == LITEPLAYER_IDLE) {
            //String music = Environment.getExternalStorageDirectory().getAbsolutePath() + "/test.mp3";
            //String music = "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3";
            String music = getCacheMusicFromAsset("test.m4a");
            mLiteplayer.setDataSource(music);
            mLiteplayer.prepareAsync();
        } else if (mStatus == LITEPLAYER_PAUSED || mStatus == LITEPLAYER_SEEKCOMPLETED) {
            mLiteplayer.resume();
        }
    }

    public void onPauseClick(View view) {
        if (mStatus == LITEPLAYER_STARTED) {
            mLiteplayer.pause();
        }
    }

    public void onSeekClick(View view) {
        int position = mLiteplayer.getCurrentPosition();
        int duration = mLiteplayer.getDuration();
        int seekOffset = position + 10*1000;
        if (seekOffset >= duration) {
            Log.e(TAG, "Failed to seek, postion: " + position + "ms, duration: " + duration + "ms");
            return;
        }
        mLiteplayer.seekTo(seekOffset);
    }

    public void onStopClick(View view) {
        if (mStatus != LITEPLAYER_IDLE) {
            mLiteplayer.reset();
        }
    }

    private final Liteplayer.OnIdleListener mIdleListener = new Liteplayer.OnIdleListener() {
        public void onIdle(Liteplayer p) {
            mStatus = LITEPLAYER_IDLE;
            mStatusView.setText("Idle");
        }
    };

    private final Liteplayer.OnPreparedListener mPreparedListener = new Liteplayer.OnPreparedListener() {
        public void onPrepared(Liteplayer p) {
            mStatus = LITEPLAYER_PREPARED;
            mStatusView.setText("Prepared");
            p.start();
        }
    };

    private final Liteplayer.OnStartedListener mStartedListener = new Liteplayer.OnStartedListener() {
        public void onStarted(Liteplayer p) {
            mStatus = LITEPLAYER_STARTED;
            mStatusView.setText("Started");
        }
    };

    private final Liteplayer.OnPausedListener mPausedListener = new Liteplayer.OnPausedListener() {
        public void onPaused(Liteplayer p) {
            mStatus = LITEPLAYER_PAUSED;
            mStatusView.setText("Paused");
        }
    };

    private final Liteplayer.OnSeekCompletedListener mSeekCompletedListener = new Liteplayer.OnSeekCompletedListener() {
        public void onSeekCompleted(Liteplayer p) {
            mStatusView.setText("SeekCompleted");
            if (mStatus != LITEPLAYER_PAUSED) {
                p.start();
            }
        }
    };

    private final Liteplayer.OnCompletedListener mCompletedListener = new Liteplayer.OnCompletedListener() {
        public void onCompleted(Liteplayer p) {
            mStatus = LITEPLAYER_COMPLETED;
            mStatusView.setText("Completed");
            mLiteplayer.reset();
        }
    };

    private final Liteplayer.OnErrorListener mErrorListener = new Liteplayer.OnErrorListener() {
        public void onError(Liteplayer p, int what, int extra) {
            mStatus = LITEPLAYER_ERROR;
            mStatusView.setText("Error");
            mLiteplayer.reset();
        }
    };
}

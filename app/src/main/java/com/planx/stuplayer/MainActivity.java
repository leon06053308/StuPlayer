package com.planx.stuplayer;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class  MainActivity extends AppCompatActivity implements SeekBar.OnSeekBarChangeListener{
    private static final String TAG = "Leo";
    private StuPlayer player;
    private static final String PATH = "/sdcard/demo.mp4";
    private TextView tv_state;
    private SurfaceView surfaceView;

    private SeekBar seekBar;
    private TextView tv_time;
    private boolean isTouch;
    private int duration;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        tv_state = findViewById(R.id.tv_state);
        surfaceView = findViewById(R.id.surfaceView);

        tv_time = findViewById(R.id.tv_time);
        seekBar = findViewById(R.id.seekBar);
        seekBar.setOnSeekBarChangeListener(this);

        requestPermission();
        //String path = new File(Environment.getExternalStorageState() + File.separator + "demo.mp4").getAbsolutePath();
    }

    private void init(){
        Log.d(TAG, "init...");
        player = new StuPlayer();
        player.setSurfaceView(surfaceView);
        player.setDataSource(PATH);

        player.setOnPreparedListener(new StuPlayer.OnPreparedListener() {
            @Override
            public void onPrepared() {

                duration = player.getDuration();

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (duration != 0){
                            tv_time.setText("00:00/" + getMinutes(duration) + ":" + getSeconds(duration));
                            tv_time.setVisibility(View.VISIBLE);
                            seekBar.setVisibility(View.VISIBLE);
                        }

                        //Toast.makeText(MainActivity.this , "?????????????????????????????????...", Toast.LENGTH_LONG).show();
                        tv_state.setTextColor(Color.GREEN);
                        tv_state.setText("???????????????... ");
                    }
                });

                player.start();
            }
        });

        player.setOnErrorListener(new StuPlayer.OnErrorListener() {
            @Override
            public void onError(final String errorInfo) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Toast.makeText(MainActivity.this, "???????????????????????????:" + errorInfo, Toast.LENGTH_SHORT).show();
                        tv_state.setTextColor(Color.RED); // ??????
                        tv_state.setText("??????,??????????????????:" + errorInfo);
                    }
                });
            }
        });

        player.setOnOnProgressListener(new StuPlayer.OnProgressListener() {
            @Override
            public void onProgress(int progress) {
                // TODO C++??????audio_time????????????????????? --> ?????????
                // ????????????????????????????????????????????????????????? ???????????????
                if (!isTouch) {

                    // C++?????????????????????????????????????????????UI
                    runOnUiThread(new Runnable() {
                        @SuppressLint("SetTextI18n")
                        @Override
                        public void run() {
                            if (duration != 0) {
                                // TODO ???????????? ?????????
                                // progress:C++??? ffmpeg????????????????????????????????????????????? 80????????????????????????????????????????????? -> 1???20??????
                                tv_time.setText(getMinutes(progress) + ":" + getSeconds(progress)
                                        + "/" +
                                        getMinutes(duration) + ":" + getSeconds(duration));

                                // TODO ????????? ????????? seekBar??????????????????????????????
                                // progress == C++?????? ???????????????  ----> seekBar????????????
                                // seekBar.setProgress(progress * 100 / duration ????????????seekBar???????????????????????????);
                                seekBar.setProgress(progress * 100 / duration);
                            }
                        }
                    });
                }
            }
        });

        player.prepare();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onStop() {
        super.onStop();
        player.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        player.release();
    }

    private void requestPermission() {

        // Here, thisActivity is the current activity
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            // Should we show an explanation?
            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.READ_EXTERNAL_STORAGE)) {

                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.READ_EXTERNAL_STORAGE},
                        100);

            } else {
                // No explanation needed, we can request the permission.
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.READ_CONTACTS},
                        100);
                // MY_PERMISSIONS_REQUEST_READ_CONTACTS is an
                // app-defined int constant. The callback method gets the
                // result of the request.
            }
        }else {
            init();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        switch (requestCode) {
            case 100: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    Log.i(TAG,"onRequestPermissionsResult granted");
                    // permission was granted, yay! Do the
                    // contacts-related task you need to do.
                    init();

                } else {
                    Log.i(TAG,"onRequestPermissionsResult denied");
                }
                return;
            }

            // other 'case' lines to check for other
            // permissions this app might request
        }
    }


    // 119 ---> 1.????????????
    private String getMinutes(int duration) { // ????????????duration????????????xxx??????
        int minutes = duration / 60;
        if (minutes <= 9) {
            return "0" + minutes;
        }
        return "" + minutes;
    }

    // 119 ---> 60 59
    private String getSeconds(int duration) { // ????????????duration????????????xxx???
        int seconds = duration % 60;
        if (seconds <= 9) {
            return "0" + seconds;
        }
        return "" + seconds;
    }

    /**
     * ???????????????????????????????????? ???????????????
     * @param seekBar ??????
     * @param progress 1~100
     * @param fromUser ?????????????????????????????????
     */
    @SuppressLint("SetTextI18n")
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            // progress ????????????????????? ???0 - 100??? ------>   ??? ??? ?????????
            tv_time.setText(getMinutes(progress * duration / 100)
                    + ":" +
                    getSeconds(progress * duration / 100) + "/" +
                    getMinutes(duration) + ":" + getSeconds(duration));
        }
    }

    // ??????????????????????????????
    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        isTouch = true;
    }

    // TODO ?????????????????? 3
    // ????????????SeekBar????????? ---> C++????????????????????????
    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        isTouch = false;

        int seekBarProgress = seekBar.getProgress(); // ????????????seekbar????????????

        // SeekBar1~100  -- ?????? -->  C++??????????????????61.546565???
        int playProgress = seekBarProgress * duration / 100;

        player.seek(playProgress);
    }
}
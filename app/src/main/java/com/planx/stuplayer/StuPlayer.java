package com.planx.stuplayer;

import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

public class StuPlayer implements SurfaceHolder.Callback {
    private static final String TAG = "StuPlayer";

    // 打不开视频
    // #define FFMPEG_CAN_NOT_OPEN_URL 1
    private static final int FFMPEG_CAN_NOT_OPEN_URL = 1;

    // 找不到媒体流
    // #define FFMPEG_CAN_NOT_FIND_STREAM 2
    private static final int FFMPEG_CAN_NOT_FIND_STREAM = 2;

    // 找不到解码器
    // #define FFMPEG_FIND_DECODER_FAIL 3
    private static final int FFMPEG_FIND_DECODER_FAIL = 3;

    // 无法根据解码器创建上下文
    // #define FFMPEG_ALLOC_CODEC_CONTEXT_FAIL 4
    private static final int FFMPEG_ALLOC_CODEC_CONTEXT_FAIL = 4;

    // 根据流信息配置上下文参数失败
    // #define FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL 6
    private static final int FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL = 6;

    // 打开解码器失败
    // #define FFMPEG_OPEN_DECODER_FAIL 7
    private static final int FFMPEG_OPEN_DECODER_FAIL = 7;

    // 没有音视频
    // #define FFMPEG_NOMEDIA 8
    private static final int FFMPEG_NOMEDIA = 8;

    static {
        System.loadLibrary("native-lib");
    }

    private String dataSource;

    private SurfaceHolder surfaceHolder;

    public void setDataSource(String source){
        dataSource = source;
    }

    public void prepare(){
        prepareNative(dataSource);
    }

    public void start(){
        startNative();
    }

    public void stop(){
        stopNative();
    }

    public void release(){
        releaseNative();
    }

    private OnPreparedListener onPreparedListener;
    private OnErrorListener onErrorListener;

    public StuPlayer(){}

    public void onPrepared(){
        if (onPreparedListener != null){
            onPreparedListener.onPrepared();
        }
    }

    public void setOnPreparedListener(OnPreparedListener listener){
        onPreparedListener = listener;
    }

    public int getDuration() {
        return getDurationNative();
    }

    public void seek(int playProgress) {
        seekNative(playProgress);
    }

    public interface OnPreparedListener{
        public void onPrepared();
    }

    public void onError(int errorCode){
        if (null != this.onErrorListener){
            String msg = null;
            switch (errorCode){
                case FFMPEG_CAN_NOT_OPEN_URL:
                    msg = "打不开视频";
                    break;
                case FFMPEG_CAN_NOT_FIND_STREAM:
                    msg = "找不到流媒体";
                    break;
                case FFMPEG_FIND_DECODER_FAIL:
                    msg = "找不到解码器";
                    break;
                case FFMPEG_ALLOC_CODEC_CONTEXT_FAIL:
                    msg = "无法根据解码器创建上下文";
                    break;
                case FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL:
                    msg = "根据流信息配置上下文参数失败";
                    break;
                case FFMPEG_OPEN_DECODER_FAIL:
                    msg = "打开解码器失败";
                    break;
                case FFMPEG_NOMEDIA:
                    msg = "没有音视频流";
                    break;
            }
            onErrorListener.onError(msg);
        }
    }

    interface OnErrorListener{
        void onError(String errorInfo);
    }

    public void setOnErrorListener(OnErrorListener listener){
        onErrorListener = listener;
    }

    public void setSurfaceView(SurfaceView surfaceView){
        Log.d(TAG, "setSurfaceView...");
        if (this.surfaceHolder != null){
            surfaceHolder.removeCallback(this);
        }
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);
    }

    private OnProgressListener onProgressListener;

    public interface OnProgressListener {

        public void onProgress(int progress);
    }

    public void onProgress(int progress) {
        if (onProgressListener != null) {
            onProgressListener.onProgress(progress);
        }
    }

    /**
     * 设置准备播放时进度的监听
     */
    public void setOnOnProgressListener(OnProgressListener onProgressListener) {
        this.onProgressListener = onProgressListener;
    }

    private native void prepareNative(String dataSource);

    private native void startNative();

    private native void stopNative();

    private native void releaseNative();

    private  native void setSurfaceNative(Surface surface);

    private native int getDurationNative();

    private native void seekNative(int playValue);

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
        Log.d(TAG, "surfaceCreated...");
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {
        Log.d(TAG, "surfaceChanged...");
        setSurfaceNative(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
        Log.d(TAG, "surfaceDestroyed...");
    }
}

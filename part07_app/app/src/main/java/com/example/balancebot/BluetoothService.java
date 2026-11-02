package com.example.balancebot;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import androidx.core.content.ContextCompat;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * HC-05 와 블루투스 클래식 SPP 로 통신하는 클래스입니다.
 *
 * 스레드 구성:
 *   작업 스레드 1개 : 소켓 연결 후 같은 스레드에서 수신 루프를 돕니다.
 *   송신 스레드 1개 : 단일 스레드 Executor 로 프레임을 순서대로 씁니다.
 *   메인 스레드      : Listener 콜백은 모두 Handler 로 메인 스레드에서 호출합니다.
 *
 * Android 의 Service 컴포넌트가 아니라 Activity 가 직접 만들어 쓰는 보통 클래스입니다.
 */
public class BluetoothService {

    /** SPP(Serial Port Profile) 표준 UUID 입니다. HC-05 는 이 UUID 로 접속을 받습니다. */
    public static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    /** 연결 상태입니다. */
    public enum State { DISCONNECTED, CONNECTING, CONNECTED }

    /** 메인 스레드에서 호출되는 콜백입니다. */
    public interface Listener {
        /** 연결 상태가 바뀌었습니다. deviceName 은 끊김일 때 null 일 수 있습니다. */
        void onStateChanged(State state, String deviceName);

        /** 한 줄을 받았습니다. 체크섬이나 형식이 틀린 줄이면 message 가 null 입니다. */
        void onLineReceived(String line, Protocol.Message message);

        /** 연결 실패, 끊김 같은 오류 메시지입니다. */
        void onError(String message);
    }

    /** 페어링 목록 다이얼로그에 보여 줄 기기 정보입니다. */
    public static final class DeviceItem {
        public final String name;
        public final String address;

        DeviceItem(String name, String address) {
            this.name = name;
            this.address = address;
        }
    }

    private final Context appContext;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final BluetoothAdapter adapter;
    private final ExecutorService writer = Executors.newSingleThreadExecutor();

    private volatile Listener listener;
    private volatile State state = State.DISCONNECTED;

    // 아래 세 필드는 this 로 동기화합니다.
    private Thread worker;
    private BluetoothSocket socket;
    private OutputStream output;

    public BluetoothService(Context context) {
        appContext = context.getApplicationContext();
        BluetoothManager manager = (BluetoothManager) appContext.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = (manager != null) ? manager.getAdapter() : null;
    }

    public void setListener(Listener listener) {
        this.listener = listener;
    }

    /** 휴대폰에 블루투스 하드웨어가 있는지 확인합니다. */
    public boolean isSupported() {
        return adapter != null;
    }

    /** 블루투스가 켜져 있는지 확인합니다. */
    @SuppressLint("MissingPermission")
    public boolean isEnabled() {
        return adapter != null && adapter.isEnabled();
    }

    /**
     * 페어링 목록 조회와 연결에 필요한 권한이 있는지 확인합니다.
     * Android 12(API 31) 이상은 BLUETOOTH_CONNECT 런타임 권한이 필요하고,
     * 그 아래 버전은 설치 시 자동 허용되는 BLUETOOTH, BLUETOOTH_ADMIN 으로 충분합니다.
     */
    public boolean hasConnectPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return ContextCompat.checkSelfPermission(appContext, Manifest.permission.BLUETOOTH_CONNECT)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    public State getState() {
        return state;
    }

    public boolean isConnected() {
        return state == State.CONNECTED;
    }

    /** 페어링(본딩)된 기기 목록을 돌려줍니다. 호출 전에 hasConnectPermission() 을 확인합니다. */
    @SuppressLint("MissingPermission")
    public List<DeviceItem> getBondedDevices() {
        List<DeviceItem> list = new ArrayList<>();
        if (adapter == null) {
            return list;
        }
        Set<BluetoothDevice> bonded = adapter.getBondedDevices();
        if (bonded == null) {
            return list;
        }
        for (BluetoothDevice d : bonded) {
            String name = d.getName();
            list.add(new DeviceItem(name != null ? name : "(이름 없음)", d.getAddress()));
        }
        return list;
    }

    /**
     * 주소로 기기에 연결합니다. 이미 연결되어 있으면 먼저 끊습니다.
     * 실제 연결은 작업 스레드에서 이루어지고 결과는 Listener 로 알려 줍니다.
     */
    public synchronized void connect(final String address, final String name) {
        if (adapter == null) {
            postError(appContext.getString(R.string.bt_not_supported));
            return;
        }
        disconnect();
        final BluetoothDevice device;
        try {
            device = adapter.getRemoteDevice(address);
        } catch (IllegalArgumentException e) {
            postError(appContext.getString(R.string.bt_connect_failed, name));
            return;
        }
        setState(State.CONNECTING, name);
        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                runWorker(device, name);
            }
        }, "bt-worker");
        worker = t;
        t.start();
    }

    /**
     * 연결을 끊습니다. 이미 송신 대기열에 들어간 프레임(예: $D,0,0)은 먼저 보낸 뒤 소켓을 닫습니다.
     */
    public synchronized void disconnect() {
        boolean wasActive = (worker != null) || (socket != null);
        worker = null;
        final BluetoothSocket s = socket;
        socket = null;
        output = null;
        if (s != null) {
            try {
                writer.execute(new Runnable() {
                    @Override
                    public void run() {
                        closeQuietly(s);
                    }
                });
            } catch (RuntimeException e) {
                // Executor 가 이미 종료된 경우 바로 닫습니다.
                closeQuietly(s);
            }
        }
        if (wasActive) {
            setState(State.DISCONNECTED, null);
        }
    }

    /** Activity 가 끝날 때 한 번 호출합니다. */
    public void shutdown() {
        listener = null;
        disconnect();
        writer.shutdown();
    }

    /**
     * 프레임 하나를 보냅니다. 연결되어 있지 않으면 false 를 돌려줍니다.
     * 실제 쓰기는 송신 스레드에서 하므로 UI 스레드가 멈추지 않습니다.
     */
    public boolean send(String frame) {
        final OutputStream out;
        final BluetoothSocket s;
        synchronized (this) {
            if (state != State.CONNECTED || output == null) {
                return false;
            }
            out = output;
            s = socket;
        }
        final byte[] bytes = frame.getBytes(StandardCharsets.US_ASCII);
        try {
            writer.execute(new Runnable() {
                @Override
                public void run() {
                    try {
                        out.write(bytes);
                        out.flush();
                    } catch (IOException e) {
                        // 소켓을 닫으면 수신 루프가 끊김을 감지하고 상태를 정리합니다.
                        closeQuietly(s);
                    }
                }
            });
        } catch (RuntimeException e) {
            return false;
        }
        return true;
    }

    // ------------------------------------------------------------------
    // 작업 스레드
    // ------------------------------------------------------------------

    @SuppressLint("MissingPermission")
    private void runWorker(BluetoothDevice device, String name) {
        // 주변 기기 검색 중이면 연결이 매우 느려지므로 검색을 멈춥니다.
        // Android 12 이상에서 BLUETOOTH_SCAN 권한이 없으면 SecurityException 이 나므로 무시합니다.
        try {
            if (adapter.isDiscovering()) {
                adapter.cancelDiscovery();
            }
        } catch (SecurityException ignored) {
            // 검색 중지는 필수가 아닙니다.
        }

        BluetoothSocket s;
        try {
            s = device.createRfcommSocketToServiceRecord(SPP_UUID);
        } catch (IOException | SecurityException e) {
            finishWithError(appContext.getString(R.string.bt_connect_failed, name));
            return;
        }

        synchronized (this) {
            if (worker != Thread.currentThread()) {
                closeQuietly(s);
                return;
            }
            socket = s;
        }

        // connect() 는 성공하거나 실패할 때까지(최대 약 12초) 블록됩니다.
        try {
            s.connect();
        } catch (IOException | SecurityException e) {
            closeQuietly(s);
            finishWithError(appContext.getString(R.string.bt_connect_failed, name));
            return;
        }

        InputStream in;
        synchronized (this) {
            if (worker != Thread.currentThread()) {
                // 연결 중에 사용자가 끊기를 눌렀습니다.
                closeQuietly(s);
                return;
            }
            try {
                in = s.getInputStream();
                output = s.getOutputStream();
            } catch (IOException e) {
                in = null;
            }
            if (in != null) {
                // 끊기와 엇갈리지 않도록 잠금 안에서 상태를 바꿉니다.
                setState(State.CONNECTED, name);
            }
        }
        if (in == null) {
            closeQuietly(s);
            finishWithError(appContext.getString(R.string.bt_connect_failed, name));
            return;
        }

        Protocol.LineAssembler assembler = new Protocol.LineAssembler();
        byte[] buffer = new byte[256];
        try {
            while (true) {
                int n = in.read(buffer);
                if (n < 0) {
                    break;
                }
                for (String line : assembler.feed(buffer, 0, n)) {
                    deliverLine(line);
                }
            }
        } catch (IOException e) {
            // 소켓이 닫히거나 HC-05 전원이 꺼지면 여기로 옵니다.
        }

        boolean lost;
        synchronized (this) {
            lost = (worker == Thread.currentThread());
            if (lost) {
                worker = null;
                socket = null;
                output = null;
            }
        }
        closeQuietly(s);
        if (lost) {
            postError(appContext.getString(R.string.bt_connection_lost));
            setState(State.DISCONNECTED, null);
        }
    }

    /** 연결 실패 처리: 내가 현재 작업 스레드일 때만 상태를 정리합니다. */
    private void finishWithError(String message) {
        boolean current;
        synchronized (this) {
            current = (worker == Thread.currentThread());
            if (current) {
                worker = null;
                socket = null;
                output = null;
            }
        }
        if (current) {
            postError(message);
            setState(State.DISCONNECTED, null);
        }
    }

    // ------------------------------------------------------------------
    // 메인 스레드로 전달
    // ------------------------------------------------------------------

    private void deliverLine(final String line) {
        // 체크섬이나 형식이 틀리면 tryParse 가 null 을 돌려줍니다.
        final Protocol.Message message = Protocol.tryParse(line);
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                Listener l = listener;
                if (l != null) {
                    l.onLineReceived(line, message);
                }
            }
        });
    }

    private void setState(final State newState, final String deviceName) {
        state = newState;
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                Listener l = listener;
                if (l != null) {
                    l.onStateChanged(newState, deviceName);
                }
            }
        });
    }

    private void postError(final String message) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                Listener l = listener;
                if (l != null) {
                    l.onError(message);
                }
            }
        });
    }

    private static void closeQuietly(BluetoothSocket s) {
        if (s == null) {
            return;
        }
        try {
            s.close();
        } catch (IOException ignored) {
            // 닫기 실패는 무시합니다.
        }
    }
}

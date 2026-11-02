package com.example.balancebot;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.List;

/**
 * 밸런싱 로봇 조종 앱의 유일한 화면입니다.
 *
 * 화면 위에서부터: 연결, 모터 허용, 텔레메트리, 조이스틱, PID 튜닝, 수신 로그.
 * 블루투스 입출력은 BluetoothService 가, 프레임 생성과 파싱은 Protocol 이 맡습니다.
 */
public class MainActivity extends AppCompatActivity
        implements BluetoothService.Listener, JoystickView.OnJoystickListener {

    /** 조이스틱 전송 주기 (SPEC 4장: 터치 중 50 ms 마다 $D) */
    private static final long JOY_PERIOD_MS = 50;

    private BluetoothService bt;
    private final Handler handler = new Handler(Looper.getMainLooper());

    // 연결
    private TextView tvConnState;
    private Button btnConnect;
    private Button btnDisconnect;

    // 모터
    private SwitchCompat switchMotor;
    private boolean ignoreSwitchEvent;

    // 텔레메트리
    private TextView tvAngle;
    private TextView tvOutput;
    private TextView tvBattery;
    private TextView tvState;

    // 조이스틱
    private JoystickView joystick;
    private TextView tvJoyValue;
    private boolean joyRunning;

    // PID 튜닝
    private EditText etKp;
    private EditText etKi;
    private EditText etKd;
    private EditText etTrim;
    private EditText etKs;
    private Button btnSendParams;
    private Button btnReadParams;
    private Button btnSaveParams;

    // 수신 확인
    private TextView tvLastRx;
    private TextView tvRxStats;
    private int rxGood;
    private int rxBad;

    /** 50 ms 마다 현재 조이스틱 값을 보내는 작업입니다. */
    private final Runnable joyTask = new Runnable() {
        @Override
        public void run() {
            if (!joyRunning) {
                return;
            }
            bt.send(Protocol.drive(joystick.getValueX(), joystick.getValueY()));
            handler.postDelayed(this, JOY_PERIOD_MS);
        }
    };

    /** 런타임 권한 요청 결과를 받습니다 (Android 12 이상). */
    private final ActivityResultLauncher<String[]> permissionLauncher =
            registerForActivityResult(new ActivityResultContracts.RequestMultiplePermissions(), result -> {
                if (bt.hasConnectPermission()) {
                    ensureEnabledThenPick();
                } else {
                    toast(getString(R.string.perm_denied));
                }
            });

    /** 블루투스 켜기 요청 결과를 받습니다. */
    private final ActivityResultLauncher<Intent> enableBtLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (bt.isEnabled()) {
                    showDevicePicker();
                } else {
                    toast(getString(R.string.bt_off));
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        bt = new BluetoothService(this);
        bt.setListener(this);

        bindViews();
        setupListeners();
        updateConnectionUi(BluetoothService.State.DISCONNECTED, null);
        clearTelemetry();
        updateRxStats();
    }

    private void bindViews() {
        tvConnState = findViewById(R.id.tvConnState);
        btnConnect = findViewById(R.id.btnConnect);
        btnDisconnect = findViewById(R.id.btnDisconnect);

        switchMotor = findViewById(R.id.switchMotor);

        tvAngle = findViewById(R.id.tvAngle);
        tvOutput = findViewById(R.id.tvOutput);
        tvBattery = findViewById(R.id.tvBattery);
        tvState = findViewById(R.id.tvState);

        joystick = findViewById(R.id.joystick);
        tvJoyValue = findViewById(R.id.tvJoyValue);

        etKp = findViewById(R.id.etKp);
        etKi = findViewById(R.id.etKi);
        etKd = findViewById(R.id.etKd);
        etTrim = findViewById(R.id.etTrim);
        etKs = findViewById(R.id.etKs);
        btnSendParams = findViewById(R.id.btnSendParams);
        btnReadParams = findViewById(R.id.btnReadParams);
        btnSaveParams = findViewById(R.id.btnSaveParams);

        tvLastRx = findViewById(R.id.tvLastRx);
        tvRxStats = findViewById(R.id.tvRxStats);
    }

    private void setupListeners() {
        btnConnect.setOnClickListener(v -> startConnectFlow());
        btnDisconnect.setOnClickListener(v -> {
            stopJoystickTimer();
            bt.send(Protocol.drive(0, 0));
            bt.disconnect();
        });

        switchMotor.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (ignoreSwitchEvent) {
                    return;
                }
                if (!sendOrWarn(Protocol.motor(isChecked))) {
                    setMotorSwitchSilently(false);
                }
            }
        });

        joystick.setOnJoystickListener(this);

        btnSendParams.setOnClickListener(v -> sendParams());
        btnReadParams.setOnClickListener(v -> sendOrWarn(Protocol.get()));
        btnSaveParams.setOnClickListener(v -> sendOrWarn(Protocol.save()));
    }

    @Override
    protected void onPause() {
        super.onPause();
        // 화면을 벗어나면 조이스틱을 멈추고 정지 명령을 보냅니다.
        if (joyRunning) {
            stopJoystickTimer();
            bt.send(Protocol.drive(0, 0));
        }
    }

    @Override
    protected void onDestroy() {
        stopJoystickTimer();
        if (bt.isConnected()) {
            bt.send(Protocol.drive(0, 0));
        }
        bt.shutdown();
        super.onDestroy();
    }

    // ------------------------------------------------------------------
    // 연결 흐름: 권한 확인 -> 블루투스 켜기 -> 페어링 목록 -> 연결
    // ------------------------------------------------------------------

    private void startConnectFlow() {
        if (!bt.isSupported()) {
            toast(getString(R.string.bt_not_supported));
            return;
        }
        if (!bt.hasConnectPermission()) {
            requestBluetoothPermissions();
            return;
        }
        ensureEnabledThenPick();
    }

    private void requestBluetoothPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // CONNECT 와 SCAN 은 같은 "근처 기기" 권한 그룹이라 대화상자가 한 번만 뜹니다.
            permissionLauncher.launch(new String[]{
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN
            });
        } else {
            // API 30 이하에서 BLUETOOTH, BLUETOOTH_ADMIN 은 설치 시 자동 허용되므로 여기에 오지 않습니다.
            ensureEnabledThenPick();
        }
    }

    @SuppressLint("MissingPermission")
    private void ensureEnabledThenPick() {
        if (bt.isEnabled()) {
            showDevicePicker();
        } else {
            enableBtLauncher.launch(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE));
        }
    }

    private void showDevicePicker() {
        final List<BluetoothService.DeviceItem> devices = bt.getBondedDevices();
        if (devices.isEmpty()) {
            new MaterialAlertDialogBuilder(this)
                    .setTitle(R.string.dlg_pick_title)
                    .setMessage(R.string.dlg_no_paired)
                    .setPositiveButton(android.R.string.ok, null)
                    .show();
            return;
        }
        String[] labels = new String[devices.size()];
        for (int i = 0; i < devices.size(); i++) {
            BluetoothService.DeviceItem d = devices.get(i);
            labels[i] = d.name + "\n" + d.address;
        }
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.dlg_pick_title)
                .setItems(labels, (dialog, which) -> {
                    BluetoothService.DeviceItem d = devices.get(which);
                    bt.connect(d.address, d.name);
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    // ------------------------------------------------------------------
    // BluetoothService.Listener (메인 스레드에서 호출됨)
    // ------------------------------------------------------------------

    @Override
    public void onStateChanged(BluetoothService.State state, String deviceName) {
        updateConnectionUi(state, deviceName);
        if (state == BluetoothService.State.CONNECTED) {
            rxGood = 0;
            rxBad = 0;
            updateRxStats();
            // 연결되면 로봇의 현재 파라미터를 받아 입력칸을 채웁니다.
            bt.send(Protocol.get());
        } else if (state == BluetoothService.State.DISCONNECTED) {
            stopJoystickTimer();
            setMotorSwitchSilently(false);
            clearTelemetry();
        }
    }

    @Override
    public void onLineReceived(String line, Protocol.Message message) {
        tvLastRx.setText(line);
        if (message == null || !message.isFromRobot()) {
            // 체크섬이나 형식이 틀린 줄은 무시하고 개수만 셉니다.
            rxBad++;
            updateRxStats();
            return;
        }
        rxGood++;
        updateRxStats();

        switch (message.type) {
            case Protocol.MSG_TELEMETRY:
                showTelemetry(message);
                break;
            case Protocol.MSG_PARAMS:
                fillParams(message);
                break;
            case Protocol.MSG_OK:
                if (message.okCommand() == Protocol.CMD_SAVE) {
                    toast(getString(R.string.msg_saved));
                }
                break;
            case Protocol.MSG_ERROR:
                int code = message.errorCode();
                toast(getString(R.string.msg_robot_error, code, Protocol.errorName(code)));
                break;
            default:
                // 모르는 종류는 무시합니다.
                break;
        }
    }

    @Override
    public void onError(String msg) {
        toast(msg);
    }

    // ------------------------------------------------------------------
    // JoystickView.OnJoystickListener
    // ------------------------------------------------------------------

    @Override
    public void onJoystickStart() {
        if (!bt.isConnected()) {
            return;
        }
        joyRunning = true;
        handler.removeCallbacks(joyTask);
        handler.post(joyTask); // 바로 한 번 보내고 이후 50 ms 마다 반복합니다.
    }

    @Override
    public void onJoystickMove(int x, int y) {
        tvJoyValue.setText(getString(R.string.joy_value, x, y));
    }

    @Override
    public void onJoystickRelease() {
        tvJoyValue.setText(getString(R.string.joy_value, 0, 0));
        boolean wasRunning = joyRunning;
        stopJoystickTimer();
        if (wasRunning) {
            bt.send(Protocol.drive(0, 0));
        }
    }

    private void stopJoystickTimer() {
        joyRunning = false;
        handler.removeCallbacks(joyTask);
    }

    // ------------------------------------------------------------------
    // PID 튜닝
    // ------------------------------------------------------------------

    /** 입력칸 5개를 x100 정수로 바꿔 $P, $T, $V 순서로 보냅니다. */
    private void sendParams() {
        Integer kp = readX100(etKp, Protocol.GAIN_MIN, Protocol.GAIN_MAX);
        Integer ki = readX100(etKi, Protocol.GAIN_MIN, Protocol.GAIN_MAX);
        Integer kd = readX100(etKd, Protocol.GAIN_MIN, Protocol.GAIN_MAX);
        Integer trim = readX100(etTrim, Protocol.TRIM_MIN, Protocol.TRIM_MAX);
        Integer ks = readX100(etKs, Protocol.GAIN_MIN, Protocol.GAIN_MAX);
        if (kp == null || ki == null || kd == null || trim == null || ks == null) {
            toast(getString(R.string.msg_check_input));
            return;
        }
        if (!sendOrWarn(Protocol.pid(kp, ki, kd))) {
            return;
        }
        bt.send(Protocol.trim(trim));
        bt.send(Protocol.speedGain(ks));
    }

    /**
     * EditText 값을 x100 정수로 읽습니다. 형식이나 범위가 틀리면 칸에 오류를 표시하고 null 을 돌려줍니다.
     */
    private Integer readX100(EditText et, int min, int max) {
        String text = et.getText().toString();
        int v;
        try {
            v = Protocol.parseX100(text);
        } catch (NumberFormatException e) {
            et.setError(getString(R.string.err_decimal));
            return null;
        }
        if (v < min || v > max) {
            et.setError(getString(R.string.err_range,
                    Protocol.formatX100(min), Protocol.formatX100(max)));
            return null;
        }
        et.setError(null);
        return v;
    }

    /** #K 를 받으면 입력칸을 로봇 값으로 바꿉니다. */
    private void fillParams(Protocol.Message m) {
        etKp.setText(Protocol.formatX100(m.kp()));
        etKi.setText(Protocol.formatX100(m.ki()));
        etKd.setText(Protocol.formatX100(m.kd()));
        etTrim.setText(Protocol.formatX100(m.trim()));
        etKs.setText(Protocol.formatX100(m.ks()));
        etKp.setError(null);
        etKi.setError(null);
        etKd.setError(null);
        etTrim.setError(null);
        etKs.setError(null);
    }

    // ------------------------------------------------------------------
    // 텔레메트리
    // ------------------------------------------------------------------

    private void showTelemetry(Protocol.Message m) {
        tvAngle.setText(getString(R.string.tele_angle, Protocol.formatX100(m.angleX100())));
        tvOutput.setText(getString(R.string.tele_output, m.motorOut()));
        // mV 를 10 으로 나누면 V x100 이 되므로 formatX100 으로 소수 둘째 자리까지 표시합니다.
        tvBattery.setText(getString(R.string.tele_battery, Protocol.formatX100(m.batteryMv() / 10)));
        tvState.setText(getString(R.string.tele_state, m.state(), Protocol.stateName(m.state())));
    }

    private void clearTelemetry() {
        String none = getString(R.string.none);
        tvAngle.setText(getString(R.string.tele_angle, none));
        tvOutput.setText(getString(R.string.tele_output_none));
        tvBattery.setText(getString(R.string.tele_battery, none));
        tvState.setText(getString(R.string.tele_state_none));
    }

    // ------------------------------------------------------------------
    // 보조
    // ------------------------------------------------------------------

    private void updateConnectionUi(BluetoothService.State state, String deviceName) {
        switch (state) {
            case CONNECTED:
                tvConnState.setText(getString(R.string.state_connected, deviceName));
                btnConnect.setEnabled(false);
                btnDisconnect.setEnabled(true);
                break;
            case CONNECTING:
                tvConnState.setText(getString(R.string.state_connecting, deviceName));
                btnConnect.setEnabled(false);
                btnDisconnect.setEnabled(true);
                break;
            case DISCONNECTED:
            default:
                tvConnState.setText(R.string.state_disconnected);
                btnConnect.setEnabled(true);
                btnDisconnect.setEnabled(false);
                break;
        }
        boolean connected = (state == BluetoothService.State.CONNECTED);
        switchMotor.setEnabled(connected);
        btnSendParams.setEnabled(connected);
        btnReadParams.setEnabled(connected);
        btnSaveParams.setEnabled(connected);
    }

    private void updateRxStats() {
        tvRxStats.setText(getString(R.string.rx_stats, rxGood, rxBad));
    }

    /** 연결되어 있으면 보내고, 아니면 안내 Toast 를 띄웁니다. */
    private boolean sendOrWarn(String frame) {
        if (!bt.send(frame)) {
            toast(getString(R.string.msg_not_connected));
            return false;
        }
        return true;
    }

    /** 리스너를 거치지 않고 스위치 상태만 바꿉니다. */
    private void setMotorSwitchSilently(boolean checked) {
        ignoreSwitchEvent = true;
        switchMotor.setChecked(checked);
        ignoreSwitchEvent = false;
    }

    private void toast(String text) {
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show();
    }
}

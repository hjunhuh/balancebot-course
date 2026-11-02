package com.example.balancebot;

import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * 밸런싱 로봇 블루투스 프로토콜 v1 (SPEC.md 4장) 구현입니다.
 *
 * 프레임 형식:
 *   앱 -> 로봇 : $TYPE[,필드...]*HH\n
 *   로봇 -> 앱 : #TYPE[,필드...]*HH\n
 * HH 는 시작 문자 다음부터 '*' 직전까지 모든 바이트의 XOR 값을 대문자 16진수 2자리로 쓴 것입니다.
 *
 * 이 클래스는 Android 클래스를 하나도 쓰지 않습니다.
 * 그래서 PC 에서 javac 로 컴파일하고 ProtocolTest 로 바로 검증할 수 있습니다.
 */
public final class Protocol {

    /** 앱에서 로봇으로 보내는 프레임의 시작 문자입니다. */
    public static final char APP_START = '$';
    /** 로봇에서 앱으로 오는 프레임의 시작 문자입니다. */
    public static final char ROBOT_START = '#';
    /** 프레임 최대 길이(바이트)입니다. */
    public static final int MAX_FRAME_LEN = 48;

    // 앱 -> 로봇 명령 종류
    public static final char CMD_DRIVE = 'D';
    public static final char CMD_PID = 'P';
    public static final char CMD_TRIM = 'T';
    public static final char CMD_SPEED = 'V';
    public static final char CMD_MOTOR = 'M';
    public static final char CMD_SAVE = 'S';
    public static final char CMD_GET = 'G';

    // 로봇 -> 앱 메시지 종류
    public static final char MSG_TELEMETRY = 'A';
    public static final char MSG_PARAMS = 'K';
    public static final char MSG_OK = 'O';
    public static final char MSG_ERROR = 'E';

    // 필드 범위 (SPEC 4장 표)
    public static final int JOY_MIN = -100;
    public static final int JOY_MAX = 100;
    public static final int GAIN_MIN = 0;
    public static final int GAIN_MAX = 100000;
    public static final int TRIM_MIN = -1000;
    public static final int TRIM_MAX = 1000;

    /** 상태 번호표 (SPEC 4장). 배열 인덱스가 상태 번호입니다. */
    private static final String[] STATE_NAMES = {
            "INIT", "CALIB", "WAIT_UPRIGHT", "BALANCE", "FALLEN", "IDLE", "LOW_BATT"
    };

    private Protocol() {
        // 정적 메서드만 제공하므로 객체를 만들지 않습니다.
    }

    // ------------------------------------------------------------------
    // 체크섬
    // ------------------------------------------------------------------

    /**
     * 본문(시작 문자와 '*' 를 뺀 부분)의 XOR 체크섬을 계산합니다.
     * 예: checksum("G") == 0x47, checksum("M,1") == 0x50
     */
    public static int checksum(CharSequence body) {
        int x = 0;
        for (int i = 0; i < body.length(); i++) {
            x ^= (body.charAt(i) & 0xFF);
        }
        return x;
    }

    /** 0..255 값을 대문자 16진수 2자리 문자열로 바꿉니다. */
    public static String toHex2(int value) {
        return String.format(Locale.US, "%02X", value & 0xFF);
    }

    /** 대문자 16진수 2자리를 숫자로 바꿉니다. 형식이 틀리면 -1 을 돌려줍니다. */
    private static int parseHex2(String hex) {
        if (hex.length() != 2) {
            return -1;
        }
        int hi = hexDigit(hex.charAt(0));
        int lo = hexDigit(hex.charAt(1));
        if (hi < 0 || lo < 0) {
            return -1;
        }
        return (hi << 4) | lo;
    }

    /** SPEC 은 대문자 16진수만 허용하므로 소문자는 오류로 봅니다. */
    private static int hexDigit(char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    }

    // ------------------------------------------------------------------
    // 프레임 만들기
    // ------------------------------------------------------------------

    /**
     * 정수 필드로 프레임을 만듭니다. 끝에 '\n' 이 붙습니다.
     * 예: buildFrame('$', 'M', 1) 은 "$M,1*50\n"
     */
    public static String buildFrame(char start, char type, int... fields) {
        String[] text = new String[fields.length];
        for (int i = 0; i < fields.length; i++) {
            text[i] = Integer.toString(fields[i]);
        }
        return buildFrameRaw(start, type, text);
    }

    /** 문자열 필드로 프레임을 만듭니다. #O,S 처럼 정수가 아닌 필드에 씁니다. */
    public static String buildFrameRaw(char start, char type, String... fields) {
        if (start != APP_START && start != ROBOT_START) {
            throw new IllegalArgumentException("시작 문자는 $ 또는 # 이어야 합니다: " + start);
        }
        StringBuilder body = new StringBuilder();
        body.append(type);
        for (String f : fields) {
            body.append(',').append(f);
        }
        String frame = start + body.toString() + '*' + toHex2(checksum(body)) + '\n';
        if (frame.length() > MAX_FRAME_LEN) {
            throw new IllegalArgumentException("프레임이 " + MAX_FRAME_LEN + "바이트를 넘습니다: " + frame.length());
        }
        return frame;
    }

    /** $D,x,y : 조이스틱. 범위를 넘는 값은 -100..100 으로 잘라서 보냅니다. */
    public static String drive(int x, int y) {
        return buildFrame(APP_START, CMD_DRIVE, clamp(x, JOY_MIN, JOY_MAX), clamp(y, JOY_MIN, JOY_MAX));
    }

    /** $P,kp,ki,kd : PID 게인 x100 */
    public static String pid(int kpX100, int kiX100, int kdX100) {
        checkRange("kp", kpX100, GAIN_MIN, GAIN_MAX);
        checkRange("ki", kiX100, GAIN_MIN, GAIN_MAX);
        checkRange("kd", kdX100, GAIN_MIN, GAIN_MAX);
        return buildFrame(APP_START, CMD_PID, kpX100, kiX100, kdX100);
    }

    /** $T,trim : 기준 각도 보정 x100 (도) */
    public static String trim(int trimX100) {
        checkRange("trim", trimX100, TRIM_MIN, TRIM_MAX);
        return buildFrame(APP_START, CMD_TRIM, trimX100);
    }

    /** $V,ks : 속도 보상 게인 x100 */
    public static String speedGain(int ksX100) {
        checkRange("ks", ksX100, GAIN_MIN, GAIN_MAX);
        return buildFrame(APP_START, CMD_SPEED, ksX100);
    }

    /** $M,e : 모터 허용 1, 정지 0 */
    public static String motor(boolean enable) {
        return buildFrame(APP_START, CMD_MOTOR, enable ? 1 : 0);
    }

    /** $S : 현재 파라미터 EEPROM 저장 */
    public static String save() {
        return buildFrame(APP_START, CMD_SAVE);
    }

    /** $G : 파라미터 요청 */
    public static String get() {
        return buildFrame(APP_START, CMD_GET);
    }

    // 아래 4개는 로봇 쪽 프레임입니다. 테스트와 시뮬레이션에 씁니다.

    /** #A,angle,out,mv,st */
    public static String telemetry(int angleX100, int out, int mv, int state) {
        return buildFrame(ROBOT_START, MSG_TELEMETRY, angleX100, out, mv, state);
    }

    /** #K,kp,ki,kd,trim,ks */
    public static String params(int kp, int ki, int kd, int trim, int ks) {
        return buildFrame(ROBOT_START, MSG_PARAMS, kp, ki, kd, trim, ks);
    }

    /** #O,c (c 는 명령 문자 그대로) */
    public static String ok(char command) {
        return buildFrameRaw(ROBOT_START, MSG_OK, String.valueOf(command));
    }

    /** #E,n */
    public static String error(int code) {
        return buildFrame(ROBOT_START, MSG_ERROR, code);
    }

    // ------------------------------------------------------------------
    // 파싱
    // ------------------------------------------------------------------

    /** 파싱 실패 원인입니다. */
    public enum Reason {
        /** 시작 문자, '*', 필드 개수, 정수 형식 등이 틀림 */
        FORMAT,
        /** 체크섬 불일치 */
        CHECKSUM,
        /** 48바이트 초과 */
        TOO_LONG
    }

    /** 파싱 실패를 알리는 예외입니다. */
    public static final class ProtocolException extends Exception {
        private static final long serialVersionUID = 1L;
        public final Reason reason;

        public ProtocolException(Reason reason, String message) {
            super(reason + ": " + message);
            this.reason = reason;
        }
    }

    /**
     * 프레임 종류별 필드 개수를 돌려줍니다. 모르는 종류는 -1 입니다.
     */
    public static int expectedFieldCount(char start, char type) {
        if (start == APP_START) {
            switch (type) {
                case CMD_DRIVE: return 2;
                case CMD_PID: return 3;
                case CMD_TRIM: return 1;
                case CMD_SPEED: return 1;
                case CMD_MOTOR: return 1;
                case CMD_SAVE: return 0;
                case CMD_GET: return 0;
                default: return -1;
            }
        }
        switch (type) {
            case MSG_TELEMETRY: return 4;
            case MSG_PARAMS: return 5;
            case MSG_OK: return 1;
            case MSG_ERROR: return 1;
            default: return -1;
        }
    }

    /**
     * 한 줄을 파싱합니다. 끝의 '\n' 과 모든 '\r' 은 무시합니다.
     * 체크섬이 틀리거나 형식이 틀리면 ProtocolException 을 던집니다.
     */
    public static Message parse(String line) throws ProtocolException {
        if (line == null) {
            throw new ProtocolException(Reason.FORMAT, "null");
        }
        String s = line.replace("\r", "").replace("\n", "");
        if (s.isEmpty()) {
            throw new ProtocolException(Reason.FORMAT, "빈 줄");
        }
        if (s.length() > MAX_FRAME_LEN) {
            throw new ProtocolException(Reason.TOO_LONG, "길이 " + s.length());
        }
        char start = s.charAt(0);
        if (start != APP_START && start != ROBOT_START) {
            throw new ProtocolException(Reason.FORMAT, "시작 문자 오류");
        }
        int star = s.indexOf('*');
        if (star < 2 || star != s.length() - 3) {
            throw new ProtocolException(Reason.FORMAT, "'*' 위치 오류");
        }
        String body = s.substring(1, star);
        int given = parseHex2(s.substring(star + 1));
        if (given < 0) {
            throw new ProtocolException(Reason.FORMAT, "체크섬 형식 오류");
        }
        int calc = checksum(body);
        if (calc != given) {
            throw new ProtocolException(Reason.CHECKSUM,
                    "계산 " + toHex2(calc) + ", 수신 " + toHex2(given));
        }

        String[] parts = body.split(",", -1);
        String typeText = parts[0];
        if (typeText.length() != 1 || typeText.charAt(0) < 'A' || typeText.charAt(0) > 'Z') {
            throw new ProtocolException(Reason.FORMAT, "종류 문자 오류");
        }
        char type = typeText.charAt(0);
        String[] fields = new String[parts.length - 1];
        System.arraycopy(parts, 1, fields, 0, fields.length);

        int expected = expectedFieldCount(start, type);
        if (expected >= 0 && expected != fields.length) {
            throw new ProtocolException(Reason.FORMAT,
                    "필드 개수 " + fields.length + ", 기대 " + expected);
        }

        boolean textField = (start == ROBOT_START && type == MSG_OK);
        int[] ints = new int[fields.length];
        for (int i = 0; i < fields.length; i++) {
            if (fields[i].isEmpty()) {
                throw new ProtocolException(Reason.FORMAT, "빈 필드");
            }
            if (textField) {
                if (fields[i].length() != 1) {
                    throw new ProtocolException(Reason.FORMAT, "#O 필드는 문자 1개");
                }
                ints[i] = fields[i].charAt(0);
            } else {
                ints[i] = parseIntStrict(fields[i]);
            }
        }
        return new Message(start, type, fields, ints);
    }

    /** parse 와 같지만 실패하면 예외 대신 null 을 돌려줍니다. */
    public static Message tryParse(String line) {
        try {
            return parse(line);
        } catch (ProtocolException e) {
            return null;
        }
    }

    /** 부호(-)와 숫자만 허용하는 엄격한 정수 변환입니다. */
    private static int parseIntStrict(String text) throws ProtocolException {
        int i = 0;
        if (text.charAt(0) == '-') {
            i = 1;
        }
        if (i >= text.length() || text.length() - i > 9) {
            throw new ProtocolException(Reason.FORMAT, "정수 형식 오류: " + text);
        }
        for (int k = i; k < text.length(); k++) {
            char c = text.charAt(k);
            if (c < '0' || c > '9') {
                throw new ProtocolException(Reason.FORMAT, "정수 형식 오류: " + text);
            }
        }
        return Integer.parseInt(text);
    }

    /** 파싱 결과 객체입니다. */
    public static final class Message {
        public final char start;
        public final char type;
        private final String[] fields;
        private final int[] values;

        Message(char start, char type, String[] fields, int[] values) {
            this.start = start;
            this.type = type;
            this.fields = fields;
            this.values = values;
        }

        public boolean isFromRobot() {
            return start == ROBOT_START;
        }

        public int fieldCount() {
            return fields.length;
        }

        public String field(int index) {
            return fields[index];
        }

        public int intField(int index) {
            return values[index];
        }

        /** 같은 내용의 프레임 문자열을 다시 만듭니다 ('\n' 포함). */
        public String toFrame() {
            return buildFrameRaw(start, type, fields);
        }

        private void require(char expectedStart, char expectedType) {
            if (start != expectedStart || type != expectedType) {
                throw new IllegalStateException("이 메시지는 " + start + type + " 입니다");
            }
        }

        // #A
        public int angleX100() { require(ROBOT_START, MSG_TELEMETRY); return values[0]; }
        public int motorOut() { require(ROBOT_START, MSG_TELEMETRY); return values[1]; }
        public int batteryMv() { require(ROBOT_START, MSG_TELEMETRY); return values[2]; }
        public int state() { require(ROBOT_START, MSG_TELEMETRY); return values[3]; }

        // #K
        public int kp() { require(ROBOT_START, MSG_PARAMS); return values[0]; }
        public int ki() { require(ROBOT_START, MSG_PARAMS); return values[1]; }
        public int kd() { require(ROBOT_START, MSG_PARAMS); return values[2]; }
        public int trim() { require(ROBOT_START, MSG_PARAMS); return values[3]; }
        public int ks() { require(ROBOT_START, MSG_PARAMS); return values[4]; }

        // #O
        public char okCommand() { require(ROBOT_START, MSG_OK); return fields[0].charAt(0); }

        // #E
        public int errorCode() { require(ROBOT_START, MSG_ERROR); return values[0]; }

        @Override
        public String toString() {
            return toFrame().trim();
        }
    }

    // ------------------------------------------------------------------
    // 수신 바이트를 줄 단위로 모으기
    // ------------------------------------------------------------------

    /**
     * 블루투스에서 조각조각 들어오는 바이트를 '\n' 기준으로 한 줄씩 모읍니다.
     * '\r' 은 버리고, 48바이트를 넘는 줄은 통째로 버립니다.
     * '$' 나 '#' 가 보이면 새 프레임 시작으로 보고 그 앞의 쓰레기 바이트를 버립니다.
     */
    public static final class LineAssembler {
        private final StringBuilder buf = new StringBuilder();
        private boolean overflow;

        public List<String> feed(byte[] data, int offset, int length) {
            List<String> lines = new ArrayList<String>();
            for (int i = offset; i < offset + length; i++) {
                int b = data[i] & 0xFF;
                if (b == '\r') {
                    continue;
                }
                if (b == '\n') {
                    if (!overflow && buf.length() > 0) {
                        lines.add(buf.toString());
                    }
                    buf.setLength(0);
                    overflow = false;
                    continue;
                }
                if (b == APP_START || b == ROBOT_START) {
                    buf.setLength(0);
                    overflow = false;
                }
                if (overflow) {
                    continue;
                }
                if (buf.length() >= MAX_FRAME_LEN) {
                    overflow = true;
                    buf.setLength(0);
                    continue;
                }
                buf.append((char) b);
            }
            return lines;
        }

        public void reset() {
            buf.setLength(0);
            overflow = false;
        }
    }

    // ------------------------------------------------------------------
    // 보조 함수
    // ------------------------------------------------------------------

    /** 상태 번호를 이름으로 바꿉니다. 예: 3 은 "BALANCE" */
    public static String stateName(int state) {
        if (state >= 0 && state < STATE_NAMES.length) {
            return STATE_NAMES[state];
        }
        return "UNKNOWN(" + state + ")";
    }

    /** 오류 번호를 설명으로 바꿉니다. */
    public static String errorName(int code) {
        switch (code) {
            case 1: return "체크섬";
            case 2: return "형식";
            case 3: return "범위";
            case 4: return "알 수 없는 명령";
            default: return "알 수 없는 오류";
        }
    }

    /** x100 정수를 소수 둘째 자리 문자열로 바꿉니다. 예: -125 는 "-1.25", 5 는 "0.05" */
    public static String formatX100(int valueX100) {
        long v = valueX100;
        String sign = v < 0 ? "-" : "";
        long a = Math.abs(v);
        long frac = a % 100;
        return sign + (a / 100) + "." + (frac < 10 ? "0" : "") + frac;
    }

    /**
     * 사람이 입력한 소수 문자열을 x100 정수로 바꿉니다. 예: "1.25" 는 125
     * 소수 셋째 자리 이상이 있거나 숫자가 아니면 NumberFormatException 을 던집니다.
     * 일부 키보드가 넣는 쉼표 소수점도 받아 줍니다.
     */
    public static int parseX100(String text) {
        if (text == null) {
            throw new NumberFormatException("빈 값");
        }
        String s = text.trim().replace(',', '.');
        if (s.isEmpty()) {
            throw new NumberFormatException("빈 값");
        }
        BigDecimal bd;
        try {
            bd = new BigDecimal(s);
        } catch (NumberFormatException e) {
            throw new NumberFormatException("숫자가 아닙니다: " + text);
        }
        if (bd.stripTrailingZeros().scale() > 2) {
            throw new NumberFormatException("소수 둘째 자리까지만 입력합니다: " + text);
        }
        try {
            return bd.movePointRight(2).intValueExact();
        } catch (ArithmeticException e) {
            throw new NumberFormatException("값이 너무 큽니다: " + text);
        }
    }

    /** 값을 min..max 로 자릅니다. */
    public static int clamp(int v, int min, int max) {
        return v < min ? min : (v > max ? max : v);
    }

    private static void checkRange(String name, int v, int min, int max) {
        if (v < min || v > max) {
            throw new IllegalArgumentException(name + " 범위 초과: " + v + " (" + min + ".." + max + ")");
        }
    }
}

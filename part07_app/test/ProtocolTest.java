import com.example.balancebot.Protocol;

import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * Protocol.java 를 PC 에서 검증하는 순수 Java 테스트입니다.
 * assert 문은 -ea 옵션이 없으면 꺼지므로 쓰지 않고, 직접 비교한 뒤 실패하면 예외를 던집니다.
 *
 * 실행 (part07_app 폴더에서):
 *   javac -encoding UTF-8 -d build/test app/src/main/java/com/example/balancebot/Protocol.java test/ProtocolTest.java
 *   java -cp build/test ProtocolTest
 */
public class ProtocolTest {

    private static int passed = 0;

    public static void main(String[] args) throws Exception {
        testSpecExamples();
        testCommandFrames();
        testTelemetryRoundTrip();
        testParamsOkErrorRoundTrip();
        testBadChecksumRejected();
        testFormatErrors();
        testDriveClamp();
        testRangeCheck();
        testX100Conversion();
        testLineAssembler();
        testNames();
        System.out.println("ProtocolTest: 모든 검사 통과 (" + passed + "개)");
    }

    // ---------------- 검사 도우미 ----------------

    private static void check(boolean cond, String what) {
        if (!cond) {
            throw new IllegalStateException("실패: " + what);
        }
        passed++;
    }

    private static void checkEq(Object expected, Object actual, String what) {
        boolean same = (expected == null) ? actual == null : expected.equals(actual);
        if (!same) {
            throw new IllegalStateException("실패: " + what + "\n  기대: " + show(expected) + "\n  실제: " + show(actual));
        }
        passed++;
    }

    private static String show(Object o) {
        return o == null ? "null" : ("[" + o.toString().replace("\n", "\\n").replace("\r", "\\r") + "]");
    }

    private interface Action {
        void run() throws Exception;
    }

    private static void checkThrows(Class<? extends Exception> type, Action action, String what) {
        try {
            action.run();
        } catch (Exception e) {
            if (type.isInstance(e)) {
                passed++;
                return;
            }
            throw new IllegalStateException("실패: " + what + " (다른 예외 " + e + ")");
        }
        throw new IllegalStateException("실패: " + what + " (예외가 나지 않음)");
    }

    private static Protocol.Reason reasonOf(String line) {
        try {
            Protocol.parse(line);
            return null;
        } catch (Protocol.ProtocolException e) {
            return e.reason;
        }
    }

    // ---------------- 검사 ----------------

    /** SPEC 4장 체크섬 예시 두 개를 글자 그대로 확인합니다. */
    private static void testSpecExamples() throws Exception {
        checkEq(0x47, Protocol.checksum("G"), "checksum(G) == 0x47");
        checkEq(0x50, Protocol.checksum("M,1"), "checksum(M,1) == 0x50");
        checkEq(0x50, 0x4D ^ 0x2C ^ 0x31, "0x4D^0x2C^0x31 == 0x50");
        checkEq("$G*47\n", Protocol.get(), "get() == $G*47");
        checkEq("$M,1*50\n", Protocol.motor(true), "motor(true) == $M,1*50");

        Protocol.Message g = Protocol.parse("$G*47");
        checkEq('$', g.start, "$G*47 시작 문자");
        checkEq('G', g.type, "$G*47 종류");
        checkEq(0, g.fieldCount(), "$G*47 필드 개수");

        Protocol.Message m = Protocol.parse("$M,1*50\r\n");
        checkEq('M', m.type, "$M,1*50 종류");
        checkEq(1, m.intField(0), "$M,1*50 필드");
    }

    private static void testCommandFrames() {
        checkEq("$M,0*51\n", Protocol.motor(false), "motor(false)");
        checkEq("$S*53\n", Protocol.save(), "save()");
        // 직접 XOR 로 계산한 값과 비교합니다.
        String p = Protocol.pid(2500, 0, 80);
        String body = "P,2500,0,80";
        checkEq("$" + body + "*" + Protocol.toHex2(Protocol.checksum(body)) + "\n", p, "pid 프레임");
        checkEq("$T,-150*" + Protocol.toHex2(Protocol.checksum("T,-150")) + "\n", Protocol.trim(-150), "trim 프레임");
        checkEq("$V,1200*" + Protocol.toHex2(Protocol.checksum("V,1200")) + "\n", Protocol.speedGain(1200), "speedGain 프레임");
        String d = Protocol.drive(0, 0);
        checkEq("$D,0,0*" + Protocol.toHex2(Protocol.checksum("D,0,0")) + "\n", d, "drive(0,0)");
        check(Protocol.toHex2(0x0A).equals("0A"), "toHex2 는 0 을 채운 2자리");
        check(Protocol.toHex2(0xAB).equals("AB"), "toHex2 는 대문자");
    }

    /** #A,-125,40,7600,3 프레임을 만든 뒤 다시 파싱해서 값이 같은지 봅니다. */
    private static void testTelemetryRoundTrip() throws Exception {
        String frame = Protocol.telemetry(-125, 40, 7600, 3);
        String body = "A,-125,40,7600,3";
        checkEq("#" + body + "*" + Protocol.toHex2(Protocol.checksum(body)) + "\n", frame, "#A 프레임 생성");

        Protocol.Message a = Protocol.parse(frame);
        check(a.isFromRobot(), "#A 는 로봇 메시지");
        checkEq('A', a.type, "#A 종류");
        checkEq(-125, a.angleX100(), "#A 각도");
        checkEq(40, a.motorOut(), "#A 출력");
        checkEq(7600, a.batteryMv(), "#A 배터리");
        checkEq(3, a.state(), "#A 상태");
        checkEq("BALANCE", Protocol.stateName(a.state()), "#A 상태 이름");
        checkEq("-1.25", Protocol.formatX100(a.angleX100()), "#A 각도 표시");
        checkEq(frame, a.toFrame(), "#A 왕복 후 같은 프레임");
    }

    private static void testParamsOkErrorRoundTrip() throws Exception {
        String k = Protocol.params(2500, 10, 80, -150, 1200);
        Protocol.Message km = Protocol.parse(k);
        checkEq(2500, km.kp(), "#K kp");
        checkEq(10, km.ki(), "#K ki");
        checkEq(80, km.kd(), "#K kd");
        checkEq(-150, km.trim(), "#K trim");
        checkEq(1200, km.ks(), "#K ks");
        checkEq(k, km.toFrame(), "#K 왕복");

        String o = Protocol.ok('S');
        checkEq("#O,S*" + Protocol.toHex2(Protocol.checksum("O,S")) + "\n", o, "#O,S 생성");
        checkEq('S', Protocol.parse(o).okCommand(), "#O,S 명령 문자");

        Protocol.Message e = Protocol.parse(Protocol.error(4));
        checkEq(4, e.errorCode(), "#E 오류 번호");
        checkEq("알 수 없는 명령", Protocol.errorName(e.errorCode()), "#E 오류 이름");

        // 가장 긴 #K 프레임도 48바이트 이내여야 합니다.
        String longest = Protocol.params(100000, 100000, 100000, -1000, 100000);
        check(longest.length() <= Protocol.MAX_FRAME_LEN, "가장 긴 #K 길이 " + longest.length());
    }

    /** 체크섬이 틀린 줄은 거부해야 합니다. */
    private static void testBadChecksumRejected() {
        String good = Protocol.telemetry(-125, 40, 7600, 3).trim();
        String hex = good.substring(good.length() - 2);
        String wrongHex = hex.equals("00") ? "01" : "00";
        String bad = good.substring(0, good.length() - 2) + wrongHex;
        checkEq(Protocol.Reason.CHECKSUM, reasonOf(bad), "틀린 체크섬 거부");
        check(Protocol.tryParse(bad) == null, "tryParse 는 틀린 체크섬에 null");

        // 본문 한 글자가 바뀐 경우 (전송 중 비트 오류 흉내)
        String tampered = good.replace("7600", "7700");
        checkEq(Protocol.Reason.CHECKSUM, reasonOf(tampered), "본문 변조 거부");

        checkEq(Protocol.Reason.CHECKSUM, reasonOf("$G*48"), "$G*48 거부");
        // SPEC 은 대문자 16진수만 허용합니다.
        // E(0x45) ^ ,(0x2C) ^ 4(0x34) = 0x5D 이므로 "#E,4*5D" 가 정답이고 "5d" 는 형식 오류입니다.
        checkEq("#E,4*5D\n", Protocol.error(4), "#E,4 체크섬 5D");
        checkEq(Protocol.Reason.FORMAT, reasonOf("#E,4*5d"), "소문자 체크섬 거부");
    }

    private static void testFormatErrors() {
        checkEq(Protocol.Reason.FORMAT, reasonOf(""), "빈 줄");
        checkEq(Protocol.Reason.FORMAT, reasonOf("G*47"), "시작 문자 없음");
        checkEq(Protocol.Reason.FORMAT, reasonOf("$G"), "* 없음");
        checkEq(Protocol.Reason.FORMAT, reasonOf("$G*4"), "체크섬 1자리");
        checkEq(Protocol.Reason.FORMAT, reasonOf("$g*67"), "소문자 종류");
        String b1 = "A,1,2,3";
        checkEq(Protocol.Reason.FORMAT, reasonOf("#" + b1 + "*" + Protocol.toHex2(Protocol.checksum(b1))), "#A 필드 부족");
        String b2 = "A,1,x,3,4";
        checkEq(Protocol.Reason.FORMAT, reasonOf("#" + b2 + "*" + Protocol.toHex2(Protocol.checksum(b2))), "#A 정수 아님");
        String b3 = "A,1,,3,4";
        checkEq(Protocol.Reason.FORMAT, reasonOf("#" + b3 + "*" + Protocol.toHex2(Protocol.checksum(b3))), "#A 빈 필드");
        StringBuilder sb = new StringBuilder("#A");
        while (sb.length() < 60) {
            sb.append(",1");
        }
        checkEq(Protocol.Reason.TOO_LONG, reasonOf(sb + "*00"), "48바이트 초과");
    }

    private static void testDriveClamp() throws Exception {
        String d = Protocol.drive(150, -200);
        Protocol.Message m = Protocol.parse(d);
        checkEq(100, m.intField(0), "drive x 상한");
        checkEq(-100, m.intField(1), "drive y 하한");
        checkEq("$D,100,-100*" + Protocol.toHex2(Protocol.checksum("D,100,-100")) + "\n", d, "drive 자르기 프레임");
    }

    private static void testRangeCheck() {
        checkThrows(IllegalArgumentException.class, new Action() {
            public void run() { Protocol.pid(-1, 0, 0); }
        }, "kp 음수 거부");
        checkThrows(IllegalArgumentException.class, new Action() {
            public void run() { Protocol.pid(0, 100001, 0); }
        }, "ki 상한 초과 거부");
        checkThrows(IllegalArgumentException.class, new Action() {
            public void run() { Protocol.trim(1001); }
        }, "trim 상한 초과 거부");
        checkThrows(IllegalArgumentException.class, new Action() {
            public void run() { Protocol.speedGain(-5); }
        }, "ks 음수 거부");
    }

    private static void testX100Conversion() {
        checkEq(125, Protocol.parseX100("1.25"), "1.25 -> 125");
        checkEq(-5, Protocol.parseX100("-0.05"), "-0.05 -> -5");
        checkEq(1200, Protocol.parseX100("12"), "12 -> 1200");
        checkEq(150, Protocol.parseX100("1.5"), "1.5 -> 150");
        checkEq(150, Protocol.parseX100("1,50"), "쉼표 소수점");
        checkEq(101, Protocol.parseX100(" 1.010 "), "끝의 0 은 허용");
        checkThrows(NumberFormatException.class, new Action() {
            public void run() { Protocol.parseX100("1.234"); }
        }, "소수 셋째 자리 거부");
        checkThrows(NumberFormatException.class, new Action() {
            public void run() { Protocol.parseX100("abc"); }
        }, "숫자 아님 거부");
        checkThrows(NumberFormatException.class, new Action() {
            public void run() { Protocol.parseX100(""); }
        }, "빈 값 거부");
        checkEq("-1.25", Protocol.formatX100(-125), "format -125");
        checkEq("0.05", Protocol.formatX100(5), "format 5");
        checkEq("-0.05", Protocol.formatX100(-5), "format -5");
        checkEq("25.00", Protocol.formatX100(2500), "format 2500");
        checkEq("0.00", Protocol.formatX100(0), "format 0");
    }

    private static void testLineAssembler() {
        Protocol.LineAssembler asm = new Protocol.LineAssembler();
        String a = Protocol.telemetry(-125, 40, 7600, 3);
        String k = Protocol.params(2500, 10, 80, -150, 1200);
        // 쓰레기 바이트 + \r\n 줄끝 + 두 프레임이 조각나서 들어오는 상황
        byte[] all = ("zz" + a.replace("\n", "\r\n") + k).getBytes(StandardCharsets.US_ASCII);
        int cut = 7;
        List<String> first = asm.feed(all, 0, cut);
        checkEq(0, first.size(), "조각 1 에는 완성된 줄 없음");
        List<String> rest = asm.feed(all, cut, all.length - cut);
        checkEq(2, rest.size(), "나머지에서 두 줄 완성");
        checkEq(a.trim(), rest.get(0), "첫 줄 (쓰레기와 \\r 제거)");
        checkEq(k.trim(), rest.get(1), "둘째 줄");
        check(Protocol.tryParse(rest.get(0)) != null, "모은 줄 파싱 성공");

        // 너무 긴 줄은 버리고 다음 줄은 정상 처리
        StringBuilder junk = new StringBuilder();
        for (int i = 0; i < 80; i++) {
            junk.append('1');
        }
        byte[] b = (junk + "\n" + a).getBytes(StandardCharsets.US_ASCII);
        List<String> lines = asm.feed(b, 0, b.length);
        checkEq(1, lines.size(), "긴 줄 버림");
        checkEq(a.trim(), lines.get(0), "긴 줄 다음 줄 정상");
    }

    private static void testNames() {
        checkEq("INIT", Protocol.stateName(0), "상태 0");
        checkEq("CALIB", Protocol.stateName(1), "상태 1");
        checkEq("WAIT_UPRIGHT", Protocol.stateName(2), "상태 2");
        checkEq("BALANCE", Protocol.stateName(3), "상태 3");
        checkEq("FALLEN", Protocol.stateName(4), "상태 4");
        checkEq("IDLE", Protocol.stateName(5), "상태 5");
        checkEq("LOW_BATT", Protocol.stateName(6), "상태 6");
        checkEq("UNKNOWN(9)", Protocol.stateName(9), "모르는 상태");
        checkEq("체크섬", Protocol.errorName(1), "오류 1");
        checkEq("형식", Protocol.errorName(2), "오류 2");
        checkEq("범위", Protocol.errorName(3), "오류 3");
    }
}

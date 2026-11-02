package com.example.balancebot;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * Canvas 로 직접 그리는 원형 조이스틱입니다.
 *
 * 출력 좌표: x, y 모두 -100..100
 *   x : 오른쪽이 +  (로봇 회전, +오른쪽)
 *   y : 위쪽이 +    (로봇 전진, +앞)
 * 화면 좌표는 아래쪽이 + 이므로 y 는 부호를 뒤집습니다.
 *
 * 전송 주기(50 ms)는 이 뷰가 아니라 MainActivity 가 관리합니다.
 * 이 뷰는 현재 값과 터치 시작, 끝만 알려 줍니다.
 */
public class JoystickView extends View {

    /** 조이스틱 값 변화를 알려 주는 콜백입니다. 모두 메인 스레드에서 호출됩니다. */
    public interface OnJoystickListener {
        /** 손가락이 닿았습니다. */
        void onJoystickStart();

        /** 손가락이 움직였습니다. x, y 는 -100..100 입니다. */
        void onJoystickMove(int x, int y);

        /** 손가락을 뗐습니다. 손잡이는 가운데로 돌아가고 값은 0, 0 입니다. */
        void onJoystickRelease();
    }

    private final Paint basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ringPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint crossPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint knobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private float centerX;
    private float centerY;
    private float baseRadius;
    private float knobRadius;

    // 손잡이 중심의 가운데 기준 변위 (픽셀)
    private float knobDx;
    private float knobDy;

    private int valueX;
    private int valueY;
    private boolean touching;

    private OnJoystickListener listener;

    public JoystickView(Context context) {
        super(context);
        init();
    }

    public JoystickView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public JoystickView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        basePaint.setStyle(Paint.Style.FILL);
        basePaint.setColor(Color.argb(40, 128, 128, 128));

        ringPaint.setStyle(Paint.Style.STROKE);
        ringPaint.setColor(Color.GRAY);

        crossPaint.setStyle(Paint.Style.STROKE);
        crossPaint.setColor(Color.argb(120, 128, 128, 128));

        knobPaint.setStyle(Paint.Style.FILL);
        knobPaint.setColor(Color.rgb(0x3F, 0x51, 0xB5));

        float density = getResources().getDisplayMetrics().density;
        ringPaint.setStrokeWidth(2f * density);
        crossPaint.setStrokeWidth(1f * density);
    }

    public void setOnJoystickListener(OnJoystickListener l) {
        listener = l;
    }

    public int getValueX() {
        return valueX;
    }

    public int getValueY() {
        return valueY;
    }

    public boolean isTouching() {
        return touching;
    }

    /** 높이를 너비와 같게 해서 항상 정사각형으로 그립니다. */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int w = MeasureSpec.getSize(widthMeasureSpec);
        int h = MeasureSpec.getSize(heightMeasureSpec);
        int hMode = MeasureSpec.getMode(heightMeasureSpec);
        int size = (hMode == MeasureSpec.UNSPECIFIED || h == 0) ? w : Math.min(w, h);
        setMeasuredDimension(size, size);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        centerX = w / 2f;
        centerY = h / 2f;
        float half = Math.min(w, h) / 2f;
        knobRadius = half * 0.25f;
        // 손잡이가 끝까지 가도 뷰 밖으로 나가지 않도록 베이스 반지름을 줄입니다.
        baseRadius = half - knobRadius;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawCircle(centerX, centerY, baseRadius, basePaint);
        canvas.drawCircle(centerX, centerY, baseRadius, ringPaint);
        canvas.drawLine(centerX - baseRadius, centerY, centerX + baseRadius, centerY, crossPaint);
        canvas.drawLine(centerX, centerY - baseRadius, centerX, centerY + baseRadius, crossPaint);
        canvas.drawCircle(centerX + knobDx, centerY + knobDy, knobRadius, knobPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) {
            return false;
        }
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                touching = true;
                // ScrollView 가 세로 드래그를 가로채지 않도록 막습니다.
                if (getParent() != null) {
                    getParent().requestDisallowInterceptTouchEvent(true);
                }
                updateKnob(event.getX(), event.getY());
                if (listener != null) {
                    listener.onJoystickStart();
                }
                return true;

            case MotionEvent.ACTION_MOVE:
                updateKnob(event.getX(), event.getY());
                return true;

            case MotionEvent.ACTION_UP:
                performClick();
                release();
                return true;

            case MotionEvent.ACTION_CANCEL:
                release();
                return true;

            default:
                return super.onTouchEvent(event);
        }
    }

    /** 접근성 도구를 위해 performClick 을 재정의합니다. */
    @Override
    public boolean performClick() {
        return super.performClick();
    }

    /** 손가락 위치를 받아 손잡이를 옮기고 -100..100 값으로 바꿉니다. */
    private void updateKnob(float touchX, float touchY) {
        if (baseRadius <= 0f) {
            return;
        }
        float dx = touchX - centerX;
        float dy = touchY - centerY;
        float dist = (float) Math.sqrt(dx * dx + dy * dy);
        // 원 밖으로 나가면 원 둘레에 붙여 둡니다.
        if (dist > baseRadius) {
            float k = baseRadius / dist;
            dx *= k;
            dy *= k;
        }
        knobDx = dx;
        knobDy = dy;

        int x = Math.round(dx / baseRadius * 100f);
        int y = Math.round(-dy / baseRadius * 100f); // 화면 아래가 + 이므로 뒤집습니다.
        valueX = Protocol.clamp(x, Protocol.JOY_MIN, Protocol.JOY_MAX);
        valueY = Protocol.clamp(y, Protocol.JOY_MIN, Protocol.JOY_MAX);

        invalidate();
        if (listener != null) {
            listener.onJoystickMove(valueX, valueY);
        }
    }

    /** 손을 떼면 가운데로 돌아갑니다. */
    private void release() {
        boolean wasTouching = touching;
        touching = false;
        knobDx = 0f;
        knobDy = 0f;
        valueX = 0;
        valueY = 0;
        invalidate();
        if (getParent() != null) {
            getParent().requestDisallowInterceptTouchEvent(false);
        }
        if (wasTouching && listener != null) {
            listener.onJoystickRelease();
        }
    }
}

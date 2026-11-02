#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
밸런싱 로봇 기구 부품 파라메트릭 생성 스크립트 (Part 6)

실행: 저장소 루트에서  python3 part06_mech/cad/generate.py

하는 일
  1. 상단의 치수 변수로 하판, 중판, 상판, 모터 브래킷을 만들어 STL로 저장합니다.
     (trimesh 로 기본 도형을 만들고 manifold 엔진으로 불리언 빼기를 합니다)
  2. 레이저 커팅용 플레이트 외곽선 DXF를 저장합니다.
  3. 배치 검사(구멍 사이 벽 두께, 부품 간섭, 바퀴와 플레이트 간섭, 지면 여유)를 합니다.
  4. 무게중심 높이와 넘어지는 시간 상수를 계산합니다.
  5. 2D 치수 도면 PNG와 조립 미리보기 PNG를 그립니다.
  6. 모든 메시가 watertight 인지 검사하고 결과를 출력합니다.

좌표계 (조립 기준)
  X: 바퀴 축 방향 (왼쪽 바퀴 쪽이 X=0 근처), 플레이트 길이 120 mm
  Y: 앞뒤 방향 (로봇이 넘어지는 방향), 플레이트 폭 75 mm
  Z: 위쪽, 지면 Z=0
  플레이트 도면 원점은 플레이트 왼쪽 아래 모서리입니다.
"""

import math
import os
import sys

import numpy as np
import trimesh

# =====================================================================
# 1. 치수 변수 (여기만 바꾸고 다시 실행하면 모든 결과물이 갱신됩니다)
# =====================================================================

# --- 플레이트 공통 ---
PLATE_L = 120.0          # X 방향 길이 [mm]
PLATE_W = 75.0           # Y 방향 폭 [mm]
PLATE_T = 4.0            # 두께 [mm]
PLATE_R = 4.0            # 모서리 라운드 반지름 [mm]
M4_HOLE_D = 4.4          # M4 관통홀 지름 [mm]
M4_EDGE = 6.0            # 모서리 M4 홀 중심이 가장자리에서 들어온 거리 [mm]
M3_HOLE_D = 3.4          # M3 관통홀 지름 [mm]
SUPPORT_LEN = 50.0       # 층간 M4 서포트 길이 [mm]
SUPPORT_HEX = 7.0        # M4 서포트 육각 대변 [mm]
M4_HEAD_D = 7.0          # M4 볼트 머리 지름 [mm]
M3_HEAD_D = 5.5          # M3 볼트 머리 지름 [mm]
M3_NUT_AF = 5.5          # M3 너트 대변 [mm]
M3_NUT_T = 2.4           # M3 너트 두께 [mm]
MIN_WALL = 2.0           # 구멍과 구멍, 구멍과 가장자리 사이 최소 살 두께 [mm]

# --- 모터와 바퀴 ---
MOTOR_D = 25.0           # 기어드 모터 몸통 지름 [mm]
MOTOR_LEN = 52.0         # 기어박스 앞면부터 모터 뒷면까지 [mm]
SHAFT_D = 4.0            # 모터 축 지름 [mm]
SHAFT_LEN = 12.0         # 기어박스 앞면에서 나온 축 길이 [mm]
MOTOR_FACE_X = 2.0       # 기어박스 앞면이 플레이트 가장자리에서 안쪽으로 들어온 거리 [mm]
WHEEL_D = 130.0          # 바퀴 지름 [mm]
WHEEL_W = 26.0           # 바퀴 폭 [mm]
WHEEL_GAP = 3.0          # 바퀴 안쪽 면과 플레이트 가장자리 사이 간격 [mm]
MIN_WHEEL_CLEAR = 2.0    # 바퀴와 차체 사이 최소 간격 [mm]
MIN_SHAFT_ENGAGE = 5.0   # 바퀴 허브에 축이 물리는 최소 길이 [mm]
MIN_GROUND_CLEAR = 20.0  # 차체 가장 낮은 점과 지면 사이 최소 간격 [mm]

# --- 모터 브래킷 (클램프 블록) ---
BRK_LEN = 20.0           # 모터 축 방향 길이 [mm]
BRK_CLEAR = 0.2          # 모터 구멍 지름 여유 (지름 기준) [mm]
BRK_TOP_WALL = 4.0       # 모터 구멍 위쪽 벽 [mm]
BRK_SIDE_WALL = 4.4      # 모터 구멍 옆 벽 [mm]
BRK_BOT_WALL = 2.5       # 모터 구멍 아래 벽 [mm]
BRK_FLANGE_T = 5.0       # 하판에 붙는 플랜지 두께 [mm]
BRK_FLANGE_W = 51.0      # 플랜지 전체 폭 [mm]
BRK_MOUNT_PITCH = 42.0   # 하판 고정 M3 홀 간격 [mm]
BRK_EAR_W = 12.0         # 클램프 귀 폭 [mm]
BRK_EAR_H = 9.0          # 클램프 귀 높이 [mm]
BRK_SLIT = 2.0           # 클램프 슬릿 폭 [mm]
BRK_FACE_GAP = 3.0       # 기어박스 앞면에서 브래킷까지 거리 [mm]

# --- 하판 전장품 ---
L298N_BOARD = 43.0       # L298N 모듈 한 변 [mm]
L298N_PITCH = 37.0       # L298N 장착 홀 간격 (정사각) [mm]
BOARD_STANDOFF = 10.0    # 보드용 M3 서포트 높이 [mm]
BOTTOM_SLOT = (6.0, 16.0)        # 하판 배선 슬롯 (폭, 길이) [mm]
BOTTOM_SLOT_DX = 27.0            # 하판 배선 슬롯의 중심선으로부터 X 거리 [mm]

# --- 중판 전장품 ---
UNO_SIZE = (68.58, 53.34)        # Arduino Uno 보드 크기 [mm]
UNO_HOLES = [(13.97, 2.54), (15.24, 50.80), (66.04, 7.62), (66.04, 35.56)]
MIDDLE_SLOT = (6.0, 20.0)        # 중판 배선 슬롯 (폭, 길이) [mm]
MIDDLE_SLOT_X = 14.0             # 중판 배선 슬롯 중심의 가장자리로부터 X 거리 [mm]

# --- 상판 전장품 ---
BATT_HOLDER = (82.0, 77.0, 21.0)  # 18650 4구 홀더 (X, Y, 높이) [mm]
TIE_SLOT = (4.0, 8.0)             # 케이블타이 슬롯 (폭, 길이) [mm], 3.6 mm 폭 타이용
TIE_SLOT_GAP = 4.0                # 홀더 끝과 타이 슬롯 중심 사이 X 거리 [mm]
TIE_SLOT_DY = 22.0                # 타이 슬롯의 중심선으로부터 Y 거리 [mm]
HC05_SLOT = (3.4, 17.0)           # HC-05 핀헤더 꽂는 슬롯 (폭, 길이) [mm]
HC05_SLOT_X = 112.0               # HC-05 슬롯 중심 X [mm]
HC05_SIZE = (16.0, 44.0, 4.0)     # HC-05 모듈 (폭, 높이, 두께) [mm]
BATT_WIRE_SLOT = (5.0, 14.0)      # 배터리 선 통과 슬롯 (폭, 길이) [mm]
BATT_WIRE_SLOT_X = 7.0            # 배터리 선 슬롯 중심 X [mm]

# --- 무게 추정값 [g] (무게중심 계산용) ---
PLA_DENSITY = 1.24e-3     # g/mm^3
PLA_FILL = 0.6            # 벽 3개, 인필 30% 일 때 체적 대비 실제 채움 비율 추정
MASS = {
    "motor": 95.0, "battery_cell": 47.0, "holder": 25.0, "uno": 28.0,
    "l298n": 33.0, "hc05": 4.0, "support_m4": 4.0, "wires": 30.0,
}

# --- 출력 경로 ---
HERE = os.path.dirname(os.path.abspath(__file__))
PART_DIR = os.path.dirname(HERE)
STL_DIR = os.path.join(PART_DIR, "stl")
IMG_DIR = os.path.join(PART_DIR, "img")
DXF_DIR = os.path.join(PART_DIR, "dxf")
CIRCLE_SEG = 64          # 구멍 원 분할 수

# =====================================================================
# 2. 파생 치수
# =====================================================================
CX, CY = PLATE_L / 2.0, PLATE_W / 2.0              # 플레이트 중심
BORE_R = (MOTOR_D + BRK_CLEAR) / 2.0                # 브래킷 모터 구멍 반지름
BRK_AXIS_DROP = BRK_TOP_WALL + BORE_R               # 브래킷 윗면에서 모터 축까지
BRK_BODY_HALF = BORE_R + BRK_SIDE_WALL              # 브래킷 몸통 반폭
BRK_BODY_BOT = BRK_AXIS_DROP + BORE_R + BRK_BOT_WALL  # 윗면에서 몸통 바닥까지
BRK_TOTAL_H = BRK_BODY_BOT + BRK_EAR_H              # 브래킷 전체 높이
BRK_CLAMP_Z = BRK_BODY_BOT + BRK_EAR_H / 2.0        # 윗면에서 클램프 홀 중심까지
BRK_X0 = MOTOR_FACE_X + BRK_FACE_GAP                # 왼쪽 브래킷 시작 X
BRK_CXL = BRK_X0 + BRK_LEN / 2.0                    # 왼쪽 브래킷 중심 X
BRK_CXR = PLATE_L - BRK_CXL                         # 오른쪽 브래킷 중심 X

AXLE_Z = WHEEL_D / 2.0                              # 지면 기준 바퀴 축 높이
BOTTOM_Z = AXLE_Z + BRK_AXIS_DROP                   # 하판 아랫면 높이
MIDDLE_Z = BOTTOM_Z + PLATE_T + SUPPORT_LEN         # 중판 아랫면 높이
TOP_Z = MIDDLE_Z + PLATE_T + SUPPORT_LEN            # 상판 아랫면 높이

UNO_X0 = CX - UNO_SIZE[0] / 2.0                     # Uno 보드 왼쪽 아래 X
UNO_Y0 = CY - UNO_SIZE[1] / 2.0
HOLDER_X0 = CX - BATT_HOLDER[0] / 2.0
HOLDER_Y0 = CY - BATT_HOLDER[1] / 2.0


# =====================================================================
# 3. 플레이트 형상 정의 (메시, 도면, DXF, 검사가 모두 이 목록을 씁니다)
# =====================================================================
class Feature:
    """구멍 또는 슬롯. 구멍은 길이와 폭이 같은 슬롯으로 취급합니다."""

    def __init__(self, fid, x, y, width, length=None, axis="y", note=""):
        self.fid, self.x, self.y = fid, float(x), float(y)
        self.w = float(width)
        self.l = float(width if length is None else length)
        self.axis, self.note = axis, note

    @property
    def is_hole(self):
        return abs(self.l - self.w) < 1e-9

    def segment(self):
        h = (self.l - self.w) / 2.0
        if self.axis == "x":
            return (self.x - h, self.y), (self.x + h, self.y)
        return (self.x, self.y - h), (self.x, self.y + h)

    def size_text(self):
        if self.is_hole:
            return "Ø%.1f" % self.w
        return "%.1f x %.1f" % (self.w, self.l)


def corner_holes():
    pts = [(M4_EDGE, M4_EDGE), (PLATE_L - M4_EDGE, M4_EDGE),
           (PLATE_L - M4_EDGE, PLATE_W - M4_EDGE), (M4_EDGE, PLATE_W - M4_EDGE)]
    return [Feature("A%d" % (i + 1), x, y, M4_HOLE_D, note="M4 서포트")
            for i, (x, y) in enumerate(pts)]


def bottom_features():
    f = corner_holes()
    m = BRK_MOUNT_PITCH / 2.0
    for i, (x, y) in enumerate([(BRK_CXL, CY - m), (BRK_CXL, CY + m),
                                (BRK_CXR, CY - m), (BRK_CXR, CY + m)]):
        f.append(Feature("B%d" % (i + 1), x, y, M3_HOLE_D, note="모터 브래킷 M3"))
    p = L298N_PITCH / 2.0
    for i, (x, y) in enumerate([(CX - p, CY - p), (CX + p, CY - p),
                                (CX + p, CY + p), (CX - p, CY + p)]):
        f.append(Feature("C%d" % (i + 1), x, y, M3_HOLE_D, note="L298N M3"))
    f.append(Feature("S1", CX - BOTTOM_SLOT_DX, CY, *BOTTOM_SLOT, note="모터 배선"))
    f.append(Feature("S2", CX + BOTTOM_SLOT_DX, CY, *BOTTOM_SLOT, note="모터 배선"))
    return f


def middle_features():
    f = corner_holes()
    for i, (hx, hy) in enumerate(UNO_HOLES):
        f.append(Feature("U%d" % (i + 1), UNO_X0 + hx, UNO_Y0 + hy, M3_HOLE_D,
                         note="Uno M3"))
    f.append(Feature("S1", MIDDLE_SLOT_X, CY, *MIDDLE_SLOT, note="배선"))
    f.append(Feature("S2", PLATE_L - MIDDLE_SLOT_X, CY, *MIDDLE_SLOT, note="배선"))
    return f


def top_features():
    f = corner_holes()
    tx = [HOLDER_X0 - TIE_SLOT_GAP, HOLDER_X0 + BATT_HOLDER[0] + TIE_SLOT_GAP]
    ty = [CY - TIE_SLOT_DY, CY + TIE_SLOT_DY]
    k = 1
    for x in tx:
        for y in ty:
            f.append(Feature("T%d" % k, x, y, *TIE_SLOT, note="케이블타이"))
            k += 1
    f.append(Feature("H1", HC05_SLOT_X, CY, *HC05_SLOT, note="HC-05 핀헤더"))
    f.append(Feature("S1", BATT_WIRE_SLOT_X, CY, *BATT_WIRE_SLOT, note="배터리 선"))
    return f


PLATES = {
    "bottom": ("하판", bottom_features),
    "middle": ("중판", middle_features),
    "top": ("상판", top_features),
}


# =====================================================================
# 4. 메시 생성 (trimesh 기본 도형 + manifold 불리언)
# =====================================================================
def box(x0, y0, z0, x1, y1, z1):
    m = trimesh.creation.box(extents=(x1 - x0, y1 - y0, z1 - z0))
    m.apply_translation(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    return m


def cyl_z(x, y, r, z0, z1, seg=CIRCLE_SEG):
    m = trimesh.creation.cylinder(radius=r, height=z1 - z0, sections=seg)
    m.apply_translation((x, y, (z0 + z1) / 2))
    return m


def cyl_axis(p0, p1, r, seg=48):
    """p0 에서 p1 까지의 원통 (조립 미리보기와 브래킷 가로 구멍용)."""
    return trimesh.creation.cylinder(radius=r, segment=np.array([p0, p1], float),
                                     sections=seg)


def union(meshes):
    return meshes[0] if len(meshes) == 1 else trimesh.boolean.union(meshes, engine="manifold")


def difference(base, cutters):
    return trimesh.boolean.difference([base] + list(cutters), engine="manifold")


def rounded_plate(l, w, t, r):
    parts = [box(r, 0, 0, l - r, w, t), box(0, r, 0, l, w - r, t)]
    for x, y in [(r, r), (l - r, r), (l - r, w - r), (r, w - r)]:
        parts.append(cyl_z(x, y, r, 0, t))
    return union(parts)


def feature_cutter(f, z0=-1.0, z1=PLATE_T + 1.0):
    (ax_, ay), (bx, by) = f.segment()
    r = f.w / 2.0
    if f.is_hole:
        return cyl_z(f.x, f.y, r, z0, z1)
    parts = [cyl_z(ax_, ay, r, z0, z1), cyl_z(bx, by, r, z0, z1)]
    if f.axis == "x":
        parts.append(box(ax_, f.y - r, z0, bx, f.y + r, z1))
    else:
        parts.append(box(f.x - r, ay, z0, f.x + r, by, z1))
    return union(parts)


def exact_plate_volume(features):
    """원을 다각형으로 근사하지 않은 이론 부피 (Fusion 의 Properties 값과 비교용)."""
    area = PLATE_L * PLATE_W - (4.0 - math.pi) * PLATE_R ** 2
    for f in features:
        area -= math.pi * (f.w / 2) ** 2 + f.w * (f.l - f.w)
    return area * PLATE_T


def make_plate(features):
    base = rounded_plate(PLATE_L, PLATE_W, PLATE_T, PLATE_R)
    return difference(base, [feature_cutter(f) for f in features])


def bracket_profile_boxes():
    """브래킷 단면 (u: 가로, v: 윗면 기준 아래로 +) 를 이루는 사각형들."""
    return [
        (-BRK_FLANGE_W / 2, 0.0, BRK_FLANGE_W / 2, BRK_FLANGE_T),        # 플랜지
        (-BRK_BODY_HALF, 0.0, BRK_BODY_HALF, BRK_BODY_BOT),               # 몸통
        (-BRK_EAR_W / 2, BRK_BODY_BOT - 1.0, BRK_EAR_W / 2, BRK_TOTAL_H),  # 클램프 귀
    ]


def make_bracket():
    """출력 방향으로 만든 브래킷: 단면이 XY 평면(베드), 모터 축이 Z 방향.

    STL 좌표: x = u (가로), y = 바닥에서 위로 (윗면 = BRK_TOTAL_H), z = 0..BRK_LEN
    모터 구멍 축이 Z 방향이므로 서포트 없이 출력됩니다.
    """
    H = BRK_TOTAL_H
    solids = [box(u0, H - v1, 0, u1, H - v0, BRK_LEN)
              for (u0, v0, u1, v1) in bracket_profile_boxes()]
    body = union(solids)
    ay = H - BRK_AXIS_DROP
    cut = [
        cyl_z(0.0, ay, BORE_R, -1, BRK_LEN + 1, seg=96),                   # 모터 구멍
        box(-BRK_SLIT / 2, -1.0, -1, BRK_SLIT / 2, ay, BRK_LEN + 1),        # 슬릿
        cyl_axis((-BRK_EAR_W, H - BRK_CLAMP_Z, BRK_LEN / 2),
                 (BRK_EAR_W, H - BRK_CLAMP_Z, BRK_LEN / 2), M3_HOLE_D / 2),  # 클램프 홀
    ]
    for s in (-1, 1):                                                       # 하판 고정 홀
        cut.append(cyl_axis((s * BRK_MOUNT_PITCH / 2, H - BRK_FLANGE_T - 1, BRK_LEN / 2),
                            (s * BRK_MOUNT_PITCH / 2, H + 1, BRK_LEN / 2), M3_HOLE_D / 2))
    return difference(body, cut)


# =====================================================================
# 5. 배치와 간섭 검사
# =====================================================================
def seg_dist(p1, q1, p2, q2):
    """2D 선분 두 개 사이 최단 거리."""
    p1, q1, p2, q2 = (np.asarray(v, float) for v in (p1, q1, p2, q2))

    def pt_seg(p, a, b):
        ab = b - a
        L2 = ab.dot(ab)
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, (p - a).dot(ab) / L2))
        return np.linalg.norm(p - (a + t * ab))

    d1, d2 = q1 - p1, q2 - p2
    cross = d1[0] * d2[1] - d1[1] * d2[0]
    if abs(cross) > 1e-12:
        r = p2 - p1
        t = (r[0] * d2[1] - r[1] * d2[0]) / cross
        u = (r[0] * d1[1] - r[1] * d1[0]) / cross
        if 0 <= t <= 1 and 0 <= u <= 1:
            return 0.0
    return min(pt_seg(p1, p2, q2), pt_seg(q1, p2, q2), pt_seg(p2, p1, q1), pt_seg(q2, p1, q1))


def shape_gap(a, b):
    """a, b 는 ('cap', p, q, r) 또는 ('rect', x0, y0, x1, y1). 겹치면 음수."""
    if a[0] == "rect" and b[0] == "rect":
        dx = max(a[1] - b[3], b[1] - a[3], 0.0)
        dy = max(a[2] - b[4], b[2] - a[4], 0.0)
        if dx == 0 and dy == 0:
            return -min(a[3] - b[1], b[3] - a[1], a[4] - b[2], b[4] - a[2])
        return math.hypot(dx, dy)
    if a[0] == "rect":
        a, b = b, a
    if b[0] == "cap":
        return seg_dist(a[1], a[2], b[1], b[2]) - a[3] - b[3]
    # 캡슐과 사각형: 캡슐 선분 위 점을 촘촘히 샘플링
    _, x0, y0, x1, y1 = b
    best = 1e9
    for t in np.linspace(0, 1, 41):
        px, py = np.asarray(a[1]) * (1 - t) + np.asarray(a[2]) * t
        dx = max(x0 - px, 0.0, px - x1)
        dy = max(y0 - py, 0.0, py - y1)
        d = math.hypot(dx, dy) if (dx or dy) else -min(px - x0, x1 - px, py - y0, y1 - py)
        best = min(best, d - a[3])
    return best


def cap(f, grow=0.0):
    p, q = f.segment()
    return ("cap", p, q, f.w / 2 + grow)


def circle(x, y, r):
    return ("cap", (x, y), (x, y), r)


def rect(x0, y0, x1, y1):
    return ("rect", x0, y0, x1, y1)


def check_features(name, feats, results):
    """구멍끼리, 구멍과 가장자리 사이 살 두께를 확인합니다."""
    worst = 1e9
    for i, a in enumerate(feats):
        p, q = a.segment()
        r = a.w / 2
        edge = min(min(p[0], q[0]) - r, PLATE_L - max(p[0], q[0]) - r,
                   min(p[1], q[1]) - r, PLATE_W - max(p[1], q[1]) - r)
        worst = min(worst, edge)
        if edge < MIN_WALL:
            results.append((False, "%s %s 가장자리 살 %.2f mm < %.1f" % (name, a.fid, edge, MIN_WALL)))
        for b in feats[i + 1:]:
            g = shape_gap(cap(a), cap(b))
            worst = min(worst, g)
            if g < MIN_WALL:
                results.append((False, "%s %s-%s 살 %.2f mm < %.1f" % (name, a.fid, b.fid, g, MIN_WALL)))
    results.append((worst >= MIN_WALL, "%s 최소 살 두께 %.2f mm (기준 %.1f)" % (name, worst, MIN_WALL)))


def check_pairs(label, shapes_a, shapes_b, need, results):
    g = min(shape_gap(a, b) for a in shapes_a for b in shapes_b)
    results.append((g >= need, "%s 간격 %.2f mm (기준 %.1f)" % (label, g, need)))


def run_checks():
    res = []
    feats = {k: fn() for k, (_, fn) in PLATES.items()}
    for k, (kr, _) in PLATES.items():
        check_features(kr, feats[k], res)

    corners = [(f.x, f.y) for f in corner_holes()]
    sup = [circle(x, y, SUPPORT_HEX / math.sqrt(3)) for x, y in corners]   # 육각 외접원
    heads = [circle(x, y, M4_HEAD_D / 2) for x, y in corners]

    # 하판 아랫면: 브래킷 발자국과 M4 볼트 머리
    brk = [rect(cx - BRK_LEN / 2, CY - BRK_FLANGE_W / 2, cx + BRK_LEN / 2, CY + BRK_FLANGE_W / 2)
           for cx in (BRK_CXL, BRK_CXR)]
    check_pairs("하판 아랫면 브래킷 / M4 볼트 머리", brk, heads, 1.0, res)
    # 하판 아랫면: 브래킷 너트가 브래킷 몸통에 걸리지 않는지
    nut_r = M3_NUT_AF / math.sqrt(3)
    nut_gap = BRK_MOUNT_PITCH / 2 - nut_r - BRK_BODY_HALF
    res.append((nut_gap >= 0.3, "브래킷 M3 너트 / 몸통 간격 %.2f mm (기준 0.3)" % nut_gap))
    bolt_room = 16.0 - (PLATE_T + BRK_FLANGE_T + M3_NUT_T)
    res.append((bolt_room >= 0.0, "브래킷 고정 M3x16 너트 체결 후 남는 나사 %.1f mm" % bolt_room))
    clamp_room = 16.0 - (BRK_EAR_W + M3_NUT_T)
    res.append((clamp_room >= 0.0, "클램프 M3x16 너트 체결 후 남는 나사 %.1f mm" % clamp_room))
    # 하판 윗면: L298N 보드와 서포트, 브래킷 볼트 머리
    l298 = [rect(CX - L298N_BOARD / 2, CY - L298N_BOARD / 2, CX + L298N_BOARD / 2, CY + L298N_BOARD / 2)]
    bheads = [circle(f.x, f.y, M3_HEAD_D / 2) for f in feats["bottom"] if f.fid.startswith("B")]
    check_pairs("하판 윗면 L298N / M4 서포트", l298, sup, 1.0, res)
    check_pairs("하판 윗면 L298N / 브래킷 볼트 머리", l298, bheads, 1.0, res)
    # 중판: Uno 보드와 서포트, 배선 슬롯
    uno = [rect(UNO_X0, UNO_Y0, UNO_X0 + UNO_SIZE[0], UNO_Y0 + UNO_SIZE[1])]
    check_pairs("중판 Uno / M4 서포트", uno, sup, 1.0, res)
    check_pairs("중판 Uno / 배선 슬롯", uno,
                [cap(f) for f in feats["middle"] if f.fid.startswith("S")], 1.0, res)
    # 상판: 배터리 홀더와 서포트, 타이 슬롯, HC-05 슬롯
    hold = [rect(HOLDER_X0, HOLDER_Y0, HOLDER_X0 + BATT_HOLDER[0], HOLDER_Y0 + BATT_HOLDER[1])]
    check_pairs("상판 배터리 홀더 / M4 서포트", hold, sup, 1.0, res)
    check_pairs("상판 배터리 홀더 / 타이 슬롯", hold,
                [cap(f) for f in feats["top"] if f.fid.startswith("T")], 1.0, res)
    check_pairs("상판 배터리 홀더 / HC-05 슬롯", hold,
                [cap(f) for f in feats["top"] if f.fid == "H1"], 1.0, res)

    # 모터 두 개가 서로 닿지 않는지
    motor_gap = PLATE_L - 2 * (MOTOR_FACE_X + MOTOR_LEN)
    res.append((motor_gap >= 2.0, "좌우 모터 뒷면 사이 간격 %.1f mm (기준 2.0)" % motor_gap))
    # 브래킷이 모터 몸통 범위 안에 있는지
    res.append((BRK_X0 + BRK_LEN <= MOTOR_FACE_X + MOTOR_LEN,
                "브래킷이 모터 몸통 길이 안에 있음 (%.1f..%.1f mm)" % (BRK_X0, BRK_X0 + BRK_LEN)))

    # 바퀴: 플레이트 좌우 바깥에 있어야 합니다 (X 구간이 겹치면 안 됨)
    wheel_in = -WHEEL_GAP                     # 왼쪽 바퀴 안쪽 면 X
    wheel_out = -WHEEL_GAP - WHEEL_W          # 왼쪽 바퀴 바깥 면 X
    body_min_x = min(0.0, MOTOR_FACE_X, BRK_X0, M4_EDGE - M4_HEAD_D / 2)
    clear = body_min_x - wheel_in
    overlap = not (wheel_in < 0.0 or wheel_out > PLATE_L)
    res.append((not overlap and clear >= MIN_WHEEL_CLEAR,
                "바퀴 X 구간 [%.1f, %.1f], 플레이트 [0, %.1f]: %s, 간격 %.1f mm (기준 %.1f)"
                % (wheel_out, wheel_in, PLATE_L, "겹침" if overlap else "겹침 없음", clear,
                   MIN_WHEEL_CLEAR)))
    engage = (SHAFT_LEN - MOTOR_FACE_X) - WHEEL_GAP
    res.append((engage >= MIN_SHAFT_ENGAGE,
                "축이 바퀴 허브에 물리는 길이 %.1f mm (기준 %.1f)" % (engage, MIN_SHAFT_ENGAGE)))
    # 지면 여유: 브래킷 바닥이 가장 낮은 점
    low = AXLE_Z - (BRK_TOTAL_H - BRK_AXIS_DROP)
    res.append((low >= MIN_GROUND_CLEAR, "차체 최저점(브래킷 바닥) 지면 위 %.1f mm (기준 %.1f)"
                % (low, MIN_GROUND_CLEAR)))
    # 모터 축 높이 (하판 기준)
    res.append((BRK_AXIS_DROP > BORE_R, "모터 축: 하판 아랫면에서 %.1f mm 아래, 지면에서 %.1f mm"
                % (BRK_AXIS_DROP, AXLE_Z)))
    return res


def tilt_contact_angle(parts):
    """몸체를 축 중심으로 기울일 때 바퀴 말고 다른 곳이 처음 지면에 닿는 각도 [도].

    바퀴 지름 130 mm 안쪽(옆에서 본 원 안)에 있는 점은 아무리 기울여도 지면에 닿지 않습니다.
    원 밖으로 나온 점(위쪽 플레이트, 배터리)만 지면에 닿을 수 있습니다.
    """
    pts = np.vstack([m.vertices[:, 1:3] for name, m, _, _ in parts
                     if name not in ("wheel", "shaft")])
    dy = pts[:, 0] - CY
    dz = pts[:, 1] - AXLE_Z
    for deg in np.arange(0.0, 180.01, 0.25):
        t = math.radians(deg)
        z_fwd = dz * math.cos(t) - dy * math.sin(t)     # 앞으로 기울임
        z_bwd = dz * math.cos(t) + dy * math.sin(t)     # 뒤로 기울임
        if min(z_fwd.min(), z_bwd.min()) <= -AXLE_Z:
            return deg
    return 180.0


# =====================================================================
# 6. 조립 모델 (미리보기와 무게중심 계산용)
# =====================================================================
def assembly_parts(meshes):
    """(이름, 메시, 색, 질량 g) 목록을 돌려줍니다. 좌표는 조립 좌표계."""
    parts = []

    def add(name, m, color, mass):
        parts.append((name, m, color, mass))

    col_plate = (0.93, 0.62, 0.20)
    for key, z in (("bottom", BOTTOM_Z), ("middle", MIDDLE_Z), ("top", TOP_Z)):
        m = meshes["plate_" + key].copy()
        m.apply_translation((0, 0, z))
        add("plate_" + key, m, col_plate, m.volume * PLA_DENSITY * PLA_FILL)

    # 브래킷: STL(출력 방향) 좌표를 조립 좌표로 돌립니다.
    # STL (x=u, y=높이, z=길이)  ->  조립 (X=길이, Y=CY+u, Z=하판아랫면-윗면기준깊이)
    T = np.array([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]], float)
    for cx in (BRK_CXL, BRK_CXR):
        m = meshes["motor_bracket"].copy()
        m.apply_transform(T)
        m.apply_translation((cx - BRK_LEN / 2, CY, BOTTOM_Z - BRK_TOTAL_H))
        add("bracket", m, (0.30, 0.55, 0.85), m.volume * PLA_DENSITY * PLA_FILL)

    for side in (0, 1):
        def X(x):
            return x if side == 0 else PLATE_L - x
        add("motor", cyl_axis((X(MOTOR_FACE_X), CY, AXLE_Z), (X(MOTOR_FACE_X + MOTOR_LEN), CY, AXLE_Z),
                              MOTOR_D / 2, 32), (0.70, 0.70, 0.72), MASS["motor"])
        add("shaft", cyl_axis((X(MOTOR_FACE_X), CY, AXLE_Z), (X(MOTOR_FACE_X - SHAFT_LEN), CY, AXLE_Z),
                              SHAFT_D / 2, 12), (0.5, 0.5, 0.5), 0.0)
        add("wheel", cyl_axis((X(-WHEEL_GAP), CY, AXLE_Z), (X(-WHEEL_GAP - WHEEL_W), CY, AXLE_Z),
                              WHEEL_D / 2, 48), (0.18, 0.18, 0.20), 0.0)   # 바퀴는 몸체 질량에서 제외

    for f in corner_holes():
        for z0 in (BOTTOM_Z + PLATE_T, MIDDLE_Z + PLATE_T):
            add("support", cyl_z(f.x, f.y, SUPPORT_HEX / 2, z0, z0 + SUPPORT_LEN, 6),
                (0.80, 0.75, 0.45), MASS["support_m4"])

    zb = BOTTOM_Z + PLATE_T + BOARD_STANDOFF
    h = L298N_BOARD / 2
    add("l298n", box(CX - h, CY - h, zb, CX + h, CY + h, zb + 1.6), (0.75, 0.15, 0.15), MASS["l298n"] * 0.4)
    add("l298n_hs", box(CX - 12, CY - 8, zb + 1.6, CX + 12, CY + 8, zb + 26), (0.15, 0.15, 0.15),
        MASS["l298n"] * 0.6)
    zm = MIDDLE_Z + PLATE_T + BOARD_STANDOFF
    add("uno", box(UNO_X0, UNO_Y0, zm, UNO_X0 + UNO_SIZE[0], UNO_Y0 + UNO_SIZE[1], zm + 12),
        (0.10, 0.45, 0.60), MASS["uno"])
    add("wires", box(CX - 5, CY - 5, MIDDLE_Z - 20, CX + 5, CY + 5, MIDDLE_Z - 10), (0.6, 0.6, 0.6),
        MASS["wires"])
    zt = TOP_Z + PLATE_T
    add("battery", box(HOLDER_X0, HOLDER_Y0, zt, HOLDER_X0 + BATT_HOLDER[0],
                       HOLDER_Y0 + BATT_HOLDER[1], zt + BATT_HOLDER[2]),
        (0.20, 0.62, 0.30), 4 * MASS["battery_cell"] + MASS["holder"])
    add("hc05", box(HC05_SLOT_X - HC05_SIZE[2] / 2, CY - HC05_SIZE[0] / 2, zt,
                    HC05_SLOT_X + HC05_SIZE[2] / 2, CY + HC05_SIZE[0] / 2, zt + HC05_SIZE[1]),
        (0.20, 0.35, 0.80), MASS["hc05"])
    return parts


def body_dynamics(parts, battery_dz=0.0):
    """바퀴를 뺀 몸체의 무게중심 높이(축 기준)와 넘어지는 시간 상수.

    축을 고정한 역진자로 보면  I * theta'' = m g L * theta  이므로
    시간 상수 tau = sqrt(I / (m g L)) 입니다. 값이 클수록 천천히 넘어집니다.
    """
    m_tot, mz, I = 0.0, 0.0, 0.0
    for name, mesh, _, m in parts:
        if m <= 0:
            continue
        c = mesh.bounds.mean(axis=0)
        dz = c[2] - AXLE_Z + (battery_dz if name == "battery" else 0.0)
        dy = c[1] - CY
        m_tot += m
        mz += m * dz
        I += m * (dz * dz + dy * dy)
    L = mz / m_tot                     # mm
    g = 9810.0                         # mm/s^2
    tau = math.sqrt(I / (m_tot * g * L))
    return m_tot, L, tau


# =====================================================================
# 7. DXF (레이저 커팅용 외곽선, R12 형식)
# =====================================================================
def write_dxf(path, feats):
    out = ["0", "SECTION", "2", "HEADER", "9", "$INSUNITS", "70", "4", "0", "ENDSEC",
           "0", "SECTION", "2", "ENTITIES"]

    def line(a, b):
        out.extend(["0", "LINE", "8", "CUT", "10", "%.4f" % a[0], "20", "%.4f" % a[1], "30", "0",
                    "11", "%.4f" % b[0], "21", "%.4f" % b[1], "31", "0"])

    def arc(c, r, a0, a1):
        out.extend(["0", "ARC", "8", "CUT", "10", "%.4f" % c[0], "20", "%.4f" % c[1], "30", "0",
                    "40", "%.4f" % r, "50", "%.4f" % a0, "51", "%.4f" % a1])

    L, W, R = PLATE_L, PLATE_W, PLATE_R
    line((R, 0), (L - R, 0)); line((L, R), (L, W - R))
    line((L - R, W), (R, W)); line((0, W - R), (0, R))
    arc((L - R, R), R, 270, 360); arc((L - R, W - R), R, 0, 90)
    arc((R, W - R), R, 90, 180); arc((R, R), R, 180, 270)
    for f in feats:
        r = f.w / 2
        if f.is_hole:
            out.extend(["0", "CIRCLE", "8", "CUT", "10", "%.4f" % f.x, "20", "%.4f" % f.y, "30", "0",
                        "40", "%.4f" % r])
            continue
        p, q = f.segment()
        if f.axis == "y":
            line((p[0] + r, p[1]), (q[0] + r, q[1])); line((q[0] - r, q[1]), (p[0] - r, p[1]))
            arc(q, r, 0, 180); arc(p, r, 180, 360)
        else:
            line((p[0], p[1] - r), (q[0], q[1] - r)); line((q[0], q[1] + r), (p[0], p[1] + r))
            arc(q, r, 270, 90); arc(p, r, 90, 270)
    out.extend(["0", "ENDSEC", "0", "EOF"])
    with open(path, "w", encoding="ascii") as fp:
        fp.write("\n".join(out) + "\n")


# =====================================================================
# 8. 도면과 미리보기 그림 (matplotlib)
# =====================================================================
def setup_matplotlib():
    import matplotlib
    matplotlib.use("Agg")
    from matplotlib import font_manager
    names = {f.name for f in font_manager.fontManager.ttflist}
    for cand in ("Noto Sans CJK KR", "NanumGothic", "Malgun Gothic", "AppleGothic"):
        if cand in names:
            matplotlib.rcParams["font.family"] = cand
            break
    matplotlib.rcParams["axes.unicode_minus"] = False
    import matplotlib.pyplot as plt
    return plt


LINE_C = "#1f2a44"
DIM_C = "#1b5fa8"
CL_C = "#c0392b"


def dim(ax, p1, p2, off, text=None, orient="h", fs=9, text_side=1, text_shift=0.0):
    """선형 치수. orient='h' 이면 off 는 치수선의 y, 'v' 이면 치수선의 x."""
    (x1, y1), (x2, y2) = p1, p2
    if text is None:
        text = "%g" % round(abs((x2 - x1) if orient == "h" else (y2 - y1)), 2)
    kw = dict(color=DIM_C, lw=0.7)
    if orient == "h":
        for x, y in ((x1, y1), (x2, y2)):
            s = 1 if off > y else -1
            ax.plot([x, x], [y + s * 1.0, off + s * 1.5], **kw)
        ax.annotate("", xy=(x1, off), xytext=(x2, off),
                    arrowprops=dict(arrowstyle="<|-|>", color=DIM_C, lw=0.7, mutation_scale=7,
                                    shrinkA=0, shrinkB=0))
        ax.text((x1 + x2) / 2 + text_shift, off + text_side * 1.0, text, color=DIM_C, fontsize=fs,
                ha="center", va="bottom" if text_side > 0 else "top")
    else:
        for x, y in ((x1, y1), (x2, y2)):
            s = 1 if off > x else -1
            ax.plot([x + s * 1.0, off + s * 1.5], [y, y], **kw)
        ax.annotate("", xy=(off, y1), xytext=(off, y2),
                    arrowprops=dict(arrowstyle="<|-|>", color=DIM_C, lw=0.7, mutation_scale=7,
                                    shrinkA=0, shrinkB=0))
        ax.text(off + text_side * 1.0, (y1 + y2) / 2 + text_shift, text, color=DIM_C, fontsize=fs,
                ha="left" if text_side > 0 else "right", va="center")


def draw_plate_outline(ax, feats, lw=1.4):
    import matplotlib.patches as mp
    ax.add_patch(mp.FancyBboxPatch((PLATE_R, PLATE_R), PLATE_L - 2 * PLATE_R, PLATE_W - 2 * PLATE_R,
                                   boxstyle="round,pad=%g" % PLATE_R, fc="#fdf3e3", ec=LINE_C, lw=lw))
    for f in feats:
        if f.is_hole:
            ax.add_patch(mp.Circle((f.x, f.y), f.w / 2, fc="white", ec=LINE_C, lw=1.0))
            ax.plot([f.x - f.w * 0.9, f.x + f.w * 0.9], [f.y, f.y], color=CL_C, lw=0.4)
            ax.plot([f.x, f.x], [f.y - f.w * 0.9, f.y + f.w * 0.9], color=CL_C, lw=0.4)
        else:
            (px, py), (qx, qy) = f.segment()
            w, l = (f.l, f.w) if f.axis == "x" else (f.w, f.l)
            ax.add_patch(mp.FancyBboxPatch((f.x - w / 2 + f.w / 2, f.y - l / 2 + f.w / 2),
                                           w - f.w, l - f.w, boxstyle="round,pad=%g" % (f.w / 2),
                                           fc="white", ec=LINE_C, lw=1.0))
            ax.plot([f.x, f.x], [f.y - l / 2 - 1.5, f.y + l / 2 + 1.5], color=CL_C, lw=0.4, ls="-.")


def label_features(ax, feats, offsets=None):
    """구멍 ID 표기. 모서리 홀은 플레이트 안쪽 대각선 방향에 둡니다."""
    offsets = offsets or {}
    for f in feats:
        half = (f.l if f.axis == "y" else f.w) / 2
        ha, va = "left", "bottom"
        if f.fid.startswith("A"):
            sx = 1 if f.x < CX else -1
            sy = 1 if f.y < CY else -1
            dx, dy = sx * (f.w / 2 + 0.6), sy * (f.w / 2 + 0.6)
            ha = "left" if sx > 0 else "right"
            va = "bottom" if sy > 0 else "top"
        else:
            dx, dy = f.w / 2 + 1.2, half + 0.8
        dx, dy = offsets.get(f.fid, (dx, dy))
        ax.text(f.x + dx, f.y + dy, f.fid, fontsize=8, color="#7a3e00", ha=ha, va=va,
                fontweight="bold")


def feature_table(ax, feats, title):
    ax.axis("off")
    rows = [[f.fid, f.size_text(), "%.2f" % f.x, "%.2f" % f.y, f.note] for f in feats]
    tb = ax.table(cellText=rows, colLabels=["ID", "크기", "X", "Y", "용도"], loc="upper center",
                  cellLoc="center", colWidths=[0.11, 0.25, 0.17, 0.17, 0.30])
    tb.auto_set_font_size(False)
    tb.set_fontsize(8.5)
    tb.scale(1.0, 1.25)
    for (r, c), cell in tb.get_celld().items():
        cell.set_edgecolor("#9aa5b5")
        if r == 0:
            cell.set_facecolor("#e3ebf6")
            cell.set_text_props(fontweight="bold")
    ax.set_title(title, fontsize=10, loc="left")


def plate_drawing(plt, key, feats, path):
    kr = PLATES[key][0]
    fig = plt.figure(figsize=(13.5, 6.6), dpi=110)
    ax = fig.add_axes([0.03, 0.08, 0.56, 0.82])
    axt = fig.add_axes([0.61, 0.08, 0.37, 0.80])
    draw_plate_outline(ax, feats)
    ax.plot([CX, CX], [-3, PLATE_W + 3], color=CL_C, lw=0.5, ls="-.")
    ax.plot([-3, PLATE_L + 3], [CY, CY], color=CL_C, lw=0.5, ls="-.")

    # 공통 치수: 전체, 모서리 홀 간격, 모서리 거리
    dim(ax, (0, 0), (PLATE_L, 0), -19, text_side=-1)
    dim(ax, (0, 0), (0, PLATE_W), -19, orient="v", text_side=-1)
    dim(ax, (M4_EDGE, PLATE_W - M4_EDGE), (PLATE_L - M4_EDGE, PLATE_W - M4_EDGE), PLATE_W + 9)
    dim(ax, (PLATE_L - M4_EDGE, M4_EDGE), (PLATE_L - M4_EDGE, PLATE_W - M4_EDGE), PLATE_L + 9,
        orient="v")
    dim(ax, (0, 0), (M4_EDGE, M4_EDGE), -9, text="%g" % M4_EDGE, text_side=-1, text_shift=-6)
    dim(ax, (0, 0), (M4_EDGE, M4_EDGE), -9, orient="v", text="%g" % M4_EDGE, text_side=-1,
        text_shift=-5)

    offsets = {}
    if key == "bottom":
        c = [f for f in feats if f.fid.startswith("C")]
        dim(ax, (c[0].x, c[0].y), (c[1].x, c[1].y), c[0].y - 7, text_side=-1)
        dim(ax, (c[1].x, c[1].y), (c[2].x, c[2].y), c[1].x + 3, orient="v", text_side=-1)
        b = [f for f in feats if f.fid.startswith("B")]
        dim(ax, (b[0].x, b[0].y), (b[1].x, b[1].y), b[0].x + 8, orient="v")
        dim(ax, (0, PLATE_W), (b[1].x, b[1].y), PLATE_W + 3)
        s = [f for f in feats if f.fid.startswith("S")]
        dim(ax, (s[0].x, s[0].y + 8), (CX, CY + 8), CY + 13)
        offsets = {"C1": (-7.5, -5.5), "C2": (2.0, -5.5), "C3": (2.0, 2.0), "C4": (-7.5, 2.0),
                   "B1": (2.0, -5.0), "B3": (2.0, 2.5), "B4": (2.0, -5.0)}
        extra = "모터 브래킷 B1..B4 는 아랫면, L298N C1..C4 는 윗면에 장착 (M3x10 서포트)"
    elif key == "middle":
        ax.add_patch(plt.Rectangle((UNO_X0, UNO_Y0), UNO_SIZE[0], UNO_SIZE[1], fc="none",
                                   ec="#2e86c1", lw=0.9, ls="--"))
        ax.text(UNO_X0 + UNO_SIZE[0] / 2, UNO_Y0 + UNO_SIZE[1] / 2 - 7,
                "Arduino Uno 외곽\n%.2f x %.2f" % UNO_SIZE, color="#2e86c1", fontsize=8,
                ha="center", va="center")
        ux1 = UNO_X0 + UNO_SIZE[0]
        uy1 = UNO_Y0 + UNO_SIZE[1]
        dim(ax, (0, uy1), (UNO_X0, uy1), uy1 + 2.5, text_shift=5.5)
        dim(ax, (ux1, 0), (ux1, UNO_Y0), ux1 + 5, orient="v")
        offsets = {"U1": (2.0, 1.6), "U2": (2.0, -5.2), "U3": (-7.0, 1.6), "U4": (-7.0, 1.6)}
        extra = "Uno 홀 좌표 = 보드 원점 (%.2f, %.2f) + Uno 홀 좌표" % (UNO_X0, UNO_Y0)
    else:
        ax.add_patch(plt.Rectangle((HOLDER_X0, HOLDER_Y0), BATT_HOLDER[0], BATT_HOLDER[1], fc="none",
                                   ec="#27ae60", lw=0.9, ls="--"))
        ax.text(CX, CY - 8, "18650 4구 홀더 외곽\n%g x %g" % BATT_HOLDER[:2], color="#1e8449",
                fontsize=8, ha="center", va="center")
        t = [f for f in feats if f.fid.startswith("T")]
        dim(ax, (t[0].x, t[0].y), (t[2].x, t[2].y), t[0].y - 9, text_side=-1)
        dim(ax, (t[2].x, t[2].y), (t[3].x, t[3].y), t[2].x - 7, orient="v", text_side=-1)
        h = [f for f in feats if f.fid == "H1"][0]
        dim(ax, (0, h.y + h.l / 2), (h.x, h.y + h.l / 2), h.y + h.l / 2 + 8)
        offsets = {"T2": (2.2, 1.0), "T4": (-7.6, 1.0), "T1": (2.2, -6.0), "T3": (-7.6, -6.0),
                   "H1": (2.4, 2.0), "S1": (3.0, -4.0)}
        extra = "케이블타이는 T1-T3, T2-T4 로 홀더를 X 방향으로 감습니다"
    label_features(ax, feats, offsets)

    ax.set_xlim(-30, PLATE_L + 22)
    ax.set_ylim(-30, PLATE_W + 22)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("%s (plate_%s)   %g x %g x %g mm,  단위 mm,  원점: 왼쪽 아래 모서리"
                 % (kr, key, PLATE_L, PLATE_W, PLATE_T), fontsize=12, x=0.03, ha="left", y=0.97)
    ax.text(-30, -29, extra + "\n모서리 라운드 R%g, 모든 구멍 관통" % PLATE_R, fontsize=8.5,
            color="#333", va="bottom")
    feature_table(axt, feats, "구멍과 슬롯 좌표표 (슬롯 크기 = 폭 x 길이, 길이 방향 Y)")
    fig.savefig(path, facecolor="white")
    plt.close(fig)


def bracket_drawing(plt, path):
    import matplotlib.patches as mp
    fig = plt.figure(figsize=(13.5, 6.6), dpi=110)
    ax = fig.add_axes([0.02, 0.06, 0.55, 0.84])
    ax2 = fig.add_axes([0.60, 0.30, 0.38, 0.60])
    # 정면도: 단면 (u 가로, 위쪽 +, v 는 윗면 기준 아래로 +)
    H = BRK_TOTAL_H

    def Y(v):
        return H - v

    ax.add_patch(mp.Polygon(np.array([
        (-BRK_FLANGE_W / 2, Y(0)), (BRK_FLANGE_W / 2, Y(0)), (BRK_FLANGE_W / 2, Y(BRK_FLANGE_T)),
        (BRK_BODY_HALF, Y(BRK_FLANGE_T)), (BRK_BODY_HALF, Y(BRK_BODY_BOT)),
        (BRK_EAR_W / 2, Y(BRK_BODY_BOT)), (BRK_EAR_W / 2, Y(H)), (BRK_SLIT / 2, Y(H)),
        (BRK_SLIT / 2, Y(BRK_AXIS_DROP)), (-BRK_SLIT / 2, Y(BRK_AXIS_DROP)), (-BRK_SLIT / 2, Y(H)),
        (-BRK_EAR_W / 2, Y(H)), (-BRK_EAR_W / 2, Y(BRK_BODY_BOT)), (-BRK_BODY_HALF, Y(BRK_BODY_BOT)),
        (-BRK_BODY_HALF, Y(BRK_FLANGE_T)), (-BRK_FLANGE_W / 2, Y(BRK_FLANGE_T))]),
        closed=True, fc="#e4eefa", ec=LINE_C, lw=1.3))
    ax.add_patch(mp.Circle((0, Y(BRK_AXIS_DROP)), BORE_R, fc="white", ec=LINE_C, lw=1.3))
    ax.add_patch(mp.Rectangle((-BRK_SLIT / 2, Y(H) - 0.01), BRK_SLIT, H - BRK_AXIS_DROP,
                              fc="white", ec="none"))
    ax.plot([-BRK_SLIT / 2] * 2, [Y(H), Y(BRK_AXIS_DROP) - BORE_R * 0.98], color=LINE_C, lw=1.0)
    ax.plot([BRK_SLIT / 2] * 2, [Y(H), Y(BRK_AXIS_DROP) - BORE_R * 0.98], color=LINE_C, lw=1.0)
    # 숨은선: 클램프 홀, 고정 홀
    cz = Y(BRK_CLAMP_Z)
    for yy in (cz - M3_HOLE_D / 2, cz + M3_HOLE_D / 2):
        ax.plot([-BRK_EAR_W / 2, BRK_EAR_W / 2], [yy, yy], color=LINE_C, lw=0.8, ls="--")
    for s in (-1, 1):
        u = s * BRK_MOUNT_PITCH / 2
        for xx in (u - M3_HOLE_D / 2, u + M3_HOLE_D / 2):
            ax.plot([xx, xx], [Y(0), Y(BRK_FLANGE_T)], color=LINE_C, lw=0.8, ls="--")
        ax.plot([u, u], [Y(0) + 2, Y(BRK_FLANGE_T) - 2], color=CL_C, lw=0.4, ls="-.")
    ax.plot([0, 0], [Y(H) - 3, Y(0) + 3], color=CL_C, lw=0.5, ls="-.")
    ay = Y(BRK_AXIS_DROP)
    ax.plot([-BORE_R - 3, BORE_R + 3], [ay, ay], color=CL_C, lw=0.5, ls="-.")
    ax.plot([-BRK_EAR_W / 2 - 3, BRK_EAR_W / 2 + 3], [cz, cz], color=CL_C, lw=0.4, ls="-.")

    # 치수
    dim(ax, (-BRK_FLANGE_W / 2, Y(0)), (BRK_FLANGE_W / 2, Y(0)), Y(0) + 15)
    dim(ax, (-BRK_MOUNT_PITCH / 2, Y(0)), (BRK_MOUNT_PITCH / 2, Y(0)), Y(0) + 7)
    dim(ax, (-BRK_BODY_HALF, Y(BRK_BODY_BOT)), (BRK_BODY_HALF, Y(BRK_BODY_BOT)), Y(H) - 8,
        text_side=-1, text_shift=-11)
    dim(ax, (-BRK_EAR_W / 2, Y(H)), (BRK_EAR_W / 2, Y(H)), Y(H) - 16, text_side=-1)
    dim(ax, (BRK_FLANGE_W / 2, Y(0)), (BRK_FLANGE_W / 2, Y(H)), BRK_FLANGE_W / 2 + 19, orient="v")
    dim(ax, (BRK_FLANGE_W / 2, Y(0)), (BRK_FLANGE_W / 2, Y(BRK_FLANGE_T)), BRK_FLANGE_W / 2 + 6,
        orient="v")
    dim(ax, (BRK_FLANGE_W / 2, Y(0)), (BORE_R, ay), BRK_FLANGE_W / 2 + 11, orient="v",
        text="%.1f" % BRK_AXIS_DROP)
    dim(ax, (-BRK_EAR_W / 2, Y(H)), (-BRK_EAR_W / 2, cz), -BRK_FLANGE_W / 2 - 6, orient="v",
        text_side=-1, text="%.1f" % (H - BRK_CLAMP_Z))
    ax.annotate("Ø%.1f\n(모터 Ø%g + 여유 %.1f)" % (2 * BORE_R, MOTOR_D, BRK_CLEAR),
                xy=(BORE_R * 0.87, ay - BORE_R * 0.5), xytext=(19.5, ay - 11), fontsize=9,
                color=DIM_C, arrowprops=dict(arrowstyle="-|>", color=DIM_C, lw=0.7))
    ax.annotate("슬릿 %g" % BRK_SLIT, xy=(BRK_SLIT / 2, Y(BRK_BODY_BOT) + 1.0),
                xytext=(18, Y(BRK_BODY_BOT) - 4), fontsize=9, color=DIM_C,
                arrowprops=dict(arrowstyle="-|>", color=DIM_C, lw=0.7))
    ax.annotate("클램프 M3 Ø%.1f 관통\n(M3x16 + 너트)" % M3_HOLE_D, xy=(BRK_EAR_W / 2, cz),
                xytext=(18, cz - 13), fontsize=9, color=DIM_C,
                arrowprops=dict(arrowstyle="-|>", color=DIM_C, lw=0.7))
    ax.annotate("하판 고정\nM3 Ø%.1f x2" % M3_HOLE_D, xy=(-BRK_MOUNT_PITCH / 2, Y(BRK_FLANGE_T) - 0.3),
                xytext=(-BRK_FLANGE_W / 2 - 25, Y(BRK_FLANGE_T) - 12), fontsize=9, color=DIM_C,
                arrowprops=dict(arrowstyle="-|>", color=DIM_C, lw=0.7))
    ax.text(-BRK_FLANGE_W / 2 - 25, Y(H) - 30, "정면도 (모터 축 방향에서 봄)", fontsize=10,
            fontweight="bold")
    ax.set_xlim(-BRK_FLANGE_W / 2 - 27, BRK_FLANGE_W / 2 + 30)
    ax.set_ylim(Y(H) - 32, Y(0) + 22)
    ax.set_aspect("equal")
    ax.axis("off")

    # 평면도 (위에서 봄): 가로 u, 세로 w(모터 축 방향)
    ax2.add_patch(mp.Rectangle((-BRK_FLANGE_W / 2, 0), BRK_FLANGE_W, BRK_LEN, fc="#e4eefa",
                               ec=LINE_C, lw=1.3))
    for s in (-1, 1):
        ax2.add_patch(mp.Circle((s * BRK_MOUNT_PITCH / 2, BRK_LEN / 2), M3_HOLE_D / 2, fc="white",
                                ec=LINE_C, lw=1.0))
        ax2.plot([s * BRK_BODY_HALF] * 2, [0, BRK_LEN], color=LINE_C, lw=0.8, ls="--")
    ax2.plot([-BRK_FLANGE_W / 2 - 3, BRK_FLANGE_W / 2 + 3], [BRK_LEN / 2] * 2, color=CL_C, lw=0.5,
             ls="-.")
    ax2.plot([0, 0], [-3, BRK_LEN + 3], color=CL_C, lw=0.5, ls="-.")
    dim(ax2, (BRK_FLANGE_W / 2, 0), (BRK_FLANGE_W / 2, BRK_LEN), BRK_FLANGE_W / 2 + 6, orient="v")
    dim(ax2, (-BRK_FLANGE_W / 2, 0), (-BRK_FLANGE_W / 2, BRK_LEN / 2), -BRK_FLANGE_W / 2 - 6,
        orient="v", text_side=-1)
    dim(ax2, (-BRK_MOUNT_PITCH / 2, BRK_LEN / 2), (BRK_MOUNT_PITCH / 2, BRK_LEN / 2), BRK_LEN + 7)
    ax2.text(-BRK_FLANGE_W / 2 - 12, -9, "평면도 (하판에 닿는 면)", fontsize=10, fontweight="bold")
    ax2.set_xlim(-BRK_FLANGE_W / 2 - 16, BRK_FLANGE_W / 2 + 16)
    ax2.set_ylim(-12, BRK_LEN + 14)
    ax2.set_aspect("equal")
    ax2.axis("off")

    fig.text(0.60, 0.07,
             "모터 브래킷 (motor_bracket) x 2개, 단위 mm\n"
             "길이 %g, 모터 축은 하판 아랫면에서 %.1f mm 아래\n"
             "출력: 정면도 단면을 베드에 눕혀 출력 (서포트 불필요)\n"
             "고정: 하판 위에서 M3x16, 플랜지 아래 M3 너트" % (BRK_LEN, BRK_AXIS_DROP),
             fontsize=9.5, va="bottom")
    fig.suptitle("모터 브래킷 도면 (25 mm 기어드 모터 클램프)", fontsize=12, x=0.03, ha="left", y=0.97)
    fig.savefig(path, facecolor="white")
    plt.close(fig)


def assembly_preview(plt, parts, dyn, path):
    import matplotlib.patches as mp
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(14, 7.2), dpi=110)
    ax = fig.add_axes([0.0, 0.0, 0.58, 0.93], projection="3d")
    tris, cols = [], []
    light = np.array([-0.4, -0.7, 0.8])
    light /= np.linalg.norm(light)
    for name, m, color, _ in parts:
        if name == "wires":
            continue
        mm = m
        if name.startswith("plate") or name in ("battery", "l298n", "uno", "bracket"):
            v, f = trimesh.remesh.subdivide_to_size(m.vertices, m.faces, max_edge=7.0)
            mm = trimesh.Trimesh(v, f, process=False)
        n = mm.face_normals
        shade = 0.45 + 0.55 * np.clip(n @ light, 0, 1)
        c = np.clip(np.outer(shade, color), 0, 1)
        tris.append(mm.vertices[mm.faces])
        cols.append(np.hstack([c, np.ones((len(c), 1))]))
    tris = np.concatenate(tris)
    cols = np.concatenate(cols)
    pc = Poly3DCollection(tris, facecolors=cols, edgecolors=cols, linewidths=0.15)
    ax.add_collection3d(pc)
    lo = tris.reshape(-1, 3).min(axis=0)
    hi = tris.reshape(-1, 3).max(axis=0)
    ax.set_xlim(lo[0], hi[0]); ax.set_ylim(lo[1], hi[1]); ax.set_zlim(0, hi[2])
    ax.set_box_aspect((hi[0] - lo[0], hi[1] - lo[1], hi[2]))
    ax.view_init(elev=18, azim=-58)
    ax.set_axis_off()
    ax.set_title("조립 미리보기 (단순 모델)", fontsize=12)

    # 오른쪽: 옆면 개략도 (X 축 방향에서 봄), 무게중심 표시
    a2 = fig.add_axes([0.60, 0.07, 0.38, 0.84])
    m_tot, L, tau = dyn
    a2.add_patch(mp.Circle((CY, AXLE_Z), WHEEL_D / 2, fc="#d5d8dc", ec="#2c3e50", lw=1.2))
    a2.add_patch(mp.Circle((CY, AXLE_Z), MOTOR_D / 2, fc="#aeb6bf", ec="#2c3e50", lw=0.8))
    for z in (BOTTOM_Z, MIDDLE_Z, TOP_Z):
        a2.add_patch(mp.Rectangle((0, z), PLATE_W, PLATE_T, fc="#f0a030", ec="#7e5109", lw=0.8))
    for y in (M4_EDGE, PLATE_W - M4_EDGE):
        for z in (BOTTOM_Z + PLATE_T, MIDDLE_Z + PLATE_T):
            a2.add_patch(mp.Rectangle((y - SUPPORT_HEX / 2, z), SUPPORT_HEX, SUPPORT_LEN,
                                      fc="#e6d690", ec="#7d6608", lw=0.6))
    a2.add_patch(mp.Rectangle((HOLDER_Y0, TOP_Z + PLATE_T), BATT_HOLDER[1], BATT_HOLDER[2],
                              fc="#58d68d", ec="#196f3d", lw=0.8))
    a2.text(CY, TOP_Z + PLATE_T + BATT_HOLDER[2] / 2, "배터리", ha="center", va="center", fontsize=9)
    a2.add_patch(mp.Rectangle((UNO_Y0, MIDDLE_Z + PLATE_T + BOARD_STANDOFF), UNO_SIZE[1], 12,
                              fc="#5dade2", ec="#1b4f72", lw=0.8))
    a2.text(CY, MIDDLE_Z + PLATE_T + BOARD_STANDOFF + 6, "Uno", ha="center", va="center", fontsize=9)
    a2.add_patch(mp.Rectangle((CY - L298N_BOARD / 2, BOTTOM_Z + PLATE_T + BOARD_STANDOFF),
                              L298N_BOARD, 26, fc="#ec7063", ec="#78281f", lw=0.8))
    a2.text(CY, BOTTOM_Z + PLATE_T + BOARD_STANDOFF + 13, "L298N", ha="center", va="center", fontsize=9)
    a2.plot([-50, PLATE_W + 40], [0, 0], color="#34495e", lw=1.5)
    cz = AXLE_Z + L
    a2.plot([CY], [AXLE_Z], "o", color="#2c3e50", ms=4)
    a2.plot([CY], [cz], "o", color="#c0392b", ms=9, mec="white", mew=1.5, zorder=5)
    a2.annotate("무게중심 (바퀴 제외)", xy=(CY, cz), xytext=(CY + 42, cz + 18), fontsize=9,
                color="#c0392b", arrowprops=dict(arrowstyle="-|>", color="#c0392b", lw=0.8))
    dim(a2, (CY, AXLE_Z), (CY, cz), -45, orient="v", text="L=%.0f" % L, text_side=-1)
    dim(a2, (CY, 0), (CY, AXLE_Z), -45, orient="v", text="축 높이\n%g" % AXLE_Z, text_side=-1)
    a2.text(PLATE_W + 40, 6, "몸체 %.0f g\nL = %.0f mm\n넘어짐 시간상수 %.0f ms"
            % (m_tot, L, tau * 1000), ha="right", va="bottom", fontsize=9.5,
            bbox=dict(fc="white", ec="#aab", lw=0.6))
    a2.set_xlim(-75, PLATE_W + 45)
    a2.set_ylim(-5, TOP_Z + PLATE_T + BATT_HOLDER[2] + 12)
    a2.set_aspect("equal")
    a2.axis("off")
    a2.set_title("옆면 개략도 (바퀴 축 방향에서 봄, 단위 mm)", fontsize=11)
    fig.savefig(path, facecolor="white")
    plt.close(fig)


# =====================================================================
# 9. 메인
# =====================================================================
def check_mesh(name, m):
    bodies = len(m.split(only_watertight=False))
    ok = bool(m.is_watertight and m.is_volume and bodies == 1)
    return ok, "%-16s watertight=%-5s volume=%-5s bodies=%d  부피 %8.1f mm^3  외형 %s" % (
        name, m.is_watertight, m.is_volume, bodies, m.volume,
        " x ".join("%.1f" % e for e in m.extents))


def main():
    for d in (STL_DIR, IMG_DIR, DXF_DIR):
        os.makedirs(d, exist_ok=True)

    print("== 1. 메시 생성 ==")
    meshes = {}
    for key, (_, fn) in PLATES.items():
        meshes["plate_" + key] = make_plate(fn())
    meshes["motor_bracket"] = make_bracket()

    all_ok = True
    for name, m in meshes.items():
        ok, msg = check_mesh(name, m)
        all_ok &= ok
        key = name.replace("plate_", "")
        if key in PLATES:
            msg += "  (이론 부피 %.1f)" % exact_plate_volume(PLATES[key][1]())
        path = os.path.join(STL_DIR, name + ".stl")
        m.export(path)
        print("[%s] %s  -> %s (%d bytes)" % ("OK" if ok else "NG", msg,
                                             os.path.relpath(path, PART_DIR), os.path.getsize(path)))

    print("\n== 2. DXF (레이저 커팅용) ==")
    for key, (_, fn) in PLATES.items():
        path = os.path.join(DXF_DIR, "plate_%s.dxf" % key)
        write_dxf(path, fn())
        print("  %s" % os.path.relpath(path, PART_DIR))

    print("\n== 3. 배치와 간섭 검사 ==")
    checks = run_checks()
    for ok, msg in checks:
        all_ok &= ok
        print("[%s] %s" % ("OK" if ok else "NG", msg))

    print("\n== 4. 무게중심 ==")
    parts = assembly_parts(meshes)
    bad = [name for name, m, _, _ in parts if not m.is_watertight]
    all_ok &= not bad
    print("[%s] 조립 모델 부품 %d개 중 watertight 아닌 것 %d개 %s"
          % ("NG" if bad else "OK", len(parts), len(bad), ", ".join(bad)))
    ang = tilt_contact_angle(parts)
    ok = ang >= 45.0
    all_ok &= ok
    print("[%s] 몸체가 지면에 처음 닿는 기울기 %.1f 도 (기준 45, 바퀴 Ø%g 원 밖의 부품이 닿음)"
          % ("OK" if ok else "NG", ang, WHEEL_D))
    dyn = body_dynamics(parts)
    print("  배터리 상판:  몸체 %.0f g, 축에서 무게중심까지 L = %.1f mm, 시간상수 %.0f ms"
          % (dyn[0], dyn[1], dyn[2] * 1000))
    low = body_dynamics(parts, battery_dz=-(TOP_Z - BOTTOM_Z))
    print("  배터리 하판:  몸체 %.0f g, 축에서 무게중심까지 L = %.1f mm, 시간상수 %.0f ms (비교용)"
          % (low[0], low[1], low[2] * 1000))

    print("\n== 5. 그림 ==")
    plt = setup_matplotlib()
    for key, (_, fn) in PLATES.items():
        path = os.path.join(IMG_DIR, "drawing_%s.png" % key)
        plate_drawing(plt, key, fn(), path)
        print("  %s" % os.path.relpath(path, PART_DIR))
    path = os.path.join(IMG_DIR, "drawing_bracket.png")
    bracket_drawing(plt, path)
    print("  %s" % os.path.relpath(path, PART_DIR))
    path = os.path.join(IMG_DIR, "assembly_preview.png")
    assembly_preview(plt, parts, dyn, path)
    print("  %s" % os.path.relpath(path, PART_DIR))

    print("\n결과: %s" % ("모든 검사 통과" if all_ok else "실패 항목이 있습니다. 치수를 확인하세요"))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())

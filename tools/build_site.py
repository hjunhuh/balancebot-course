#!/usr/bin/env python3
"""
build_site.py
각 파트 README.md 와 SPEC.md 를 모아 학습 사이트(단일 HTML)를 만듭니다.
  - <!-- include: 경로 --> 를 해당 파일의 코드 블록으로 치환
  - ![](경로) 이미지를 data URI 로 내장
사용: python3 tools/build_site.py  ->  site/index.html
"""
import base64
import html
import json
import mimetypes
import os
import re
import sys

import markdown

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PARTS = [
    ("part-01", "part01_orientation", "오리엔테이션"),
    ("part-02", "part02_c", "임베디드 Basic C"),
    ("part-03", "part03_avr", "ATmega328P 기초"),
    ("part-04", "part04_design", "개발 프로세스와 설계"),
    ("part-05", "part05_hw", "HW 개발 (KiCad)"),
    ("part-06", "part06_mech", "기구 설계 (Fusion 360)"),
    ("part-07", "part07_app", "App 개발 (Android)"),
    ("part-08", "part08_fw", "SW 개발 (Microchip Studio)"),
    ("part-09", "part09_test", "통합 TEST"),
    ("part-10", "part10_portfolio", "포트폴리오"),
    ("spec", None, "공통 사양서"),
]

LANG = {".c": "c", ".h": "c", ".java": "java", ".xml": "xml", ".py": "python",
        ".gradle": "java", ".properties": "ini", ".sh": "bash",
        ".csv": "plaintext", ".md": "markdown", "Makefile": "makefile"}

INCLUDE_RE = re.compile(r"^<!--\s*include:\s*(\S+)\s*-->\s*$", re.M)
IMG_RE = re.compile(r"!\[([^\]]*)\]\(([^)\s]+)\)")

missing = []


def lang_of(path):
    base = os.path.basename(path)
    if base in LANG:
        return LANG[base]
    return LANG.get(os.path.splitext(path)[1], "plaintext")


def include(m):
    rel = m.group(1)
    full = os.path.join(ROOT, rel)
    if not os.path.isfile(full):
        missing.append(rel)
        return f"\n> 파일 없음: `{rel}`\n"
    with open(full, encoding="utf-8") as f:
        code = f.read().rstrip("\n")
    n = code.count("\n") + 1
    # 마크다운 변환을 피하려고 HTML 로 바로 만듭니다
    return (f'\n<details class="code" data-lines="{n}"{" open" if n <= 45 else ""}>'
            f'<summary><span class="path">{html.escape(rel)}</span>'
            f'<span class="lines">{n}줄</span></summary>'
            f'<pre><code class="language-{lang_of(rel)}">{html.escape(code)}</code></pre>'
            f'</details>\n')


def image(m):
    alt, rel = m.group(1), m.group(2)
    if rel.startswith("http"):
        return m.group(0)
    full = os.path.join(ROOT, rel)
    if not os.path.isfile(full):
        missing.append(rel)
        return f"*(이미지 없음: {rel})*"
    mime = mimetypes.guess_type(full)[0] or "application/octet-stream"
    with open(full, "rb") as f:
        b64 = base64.b64encode(f.read()).decode()
    return f'<figure><img alt="{html.escape(alt)}" src="data:{mime};base64,{b64}"><figcaption>{html.escape(alt)}</figcaption></figure>'


def slug(text, used):
    s = re.sub(r"[^0-9A-Za-z가-힣]+", "-", text).strip("-").lower() or "s"
    base, i = s, 2
    while s in used:
        s = f"{base}-{i}"
        i += 1
    used.add(s)
    return s


def convert(md_text, key):
    md_text = INCLUDE_RE.sub(include, md_text)
    md_text = IMG_RE.sub(image, md_text)
    body = markdown.markdown(md_text, extensions=["tables", "fenced_code", "sane_lists"])
    # h1 은 파트 제목으로 따로 쓰므로 제거
    m = re.search(r"<h1>(.*?)</h1>", body)
    title = re.sub("<.*?>", "", m.group(1)) if m else key
    body = re.sub(r"<h1>.*?</h1>", "", body, count=1)
    # 표는 가로 스크롤 상자로 감쌈
    body = body.replace("<table>", '<div class="tbl"><table>').replace("</table>", "</table></div>")
    # 일반 코드 블록도 가로 스크롤 가능한 pre 그대로 사용, 언어 없는 블록 표시
    body = body.replace("<pre><code>", '<pre><code class="language-plaintext">')
    # h2 에 id 와 완료 토글
    used, toc = set(), []

    def h2(mm):
        text = re.sub("<.*?>", "", mm.group(1))
        sid = f"{key}-{slug(text, used)}"
        toc.append((sid, html.unescape(text)))
        return (f'<h2 id="{sid}"><span class="h2t">{mm.group(1)}</span>'
                f'<button class="done" type="button" data-sec="{sid}" aria-pressed="false">완료 표시</button></h2>')

    body = re.sub(r"<h2>(.*?)</h2>", h2, body)
    return title, body, toc


def main():
    parts = []
    for key, folder, short in PARTS:
        path = os.path.join(ROOT, folder, "README.md") if folder else os.path.join(ROOT, "SPEC.md")
        if not os.path.isfile(path):
            print("README 없음:", path)
            sys.exit(1)
        with open(path, encoding="utf-8") as f:
            title, body, toc = convert(f.read(), key)
        parts.append({"key": key, "short": short, "title": title, "body": body, "toc": toc})

    if missing:
        print("누락 파일:", missing)
        sys.exit(1)

    with open(os.path.join(ROOT, "tools", "site_template.html"), encoding="utf-8") as f:
        tpl = f.read()

    nav, tmpls = [], []
    for i, p in enumerate(parts):
        num = p["key"].split("-")[1] if p["key"].startswith("part") else "S"
        nav.append(f'<a class="nav-part" href="#{p["key"]}" data-key="{p["key"]}">'
                   f'<span class="num">{num}</span><span class="nm">{html.escape(p["short"])}</span>'
                   f'<span class="prog" data-prog="{p["key"]}"></span></a>')
        toc_html = "".join(f'<li><a href="#{p["key"]}" data-jump="{sid}">{html.escape(t)}</a></li>'
                           for sid, t in p["toc"])
        tmpls.append(f'<template id="t-{p["key"]}" data-title="{html.escape(p["title"])}">'
                     f'<ol class="toc">{toc_html}</ol>{p["body"]}</template>')

    sections = {p["key"]: [sid for sid, _ in p["toc"]] for p in parts}
    out = (tpl.replace("{{NAV}}", "\n".join(nav))
              .replace("{{TEMPLATES}}", "\n".join(tmpls))
              .replace("{{SECTIONS}}", json.dumps(sections, ensure_ascii=False))
              .replace("{{ORDER}}", json.dumps([p["key"] for p in parts])))
    os.makedirs(os.path.join(ROOT, "site"), exist_ok=True)
    dst = os.path.join(ROOT, "site", "index.html")
    with open(dst, "w", encoding="utf-8") as f:
        f.write(out)
    total = sum(len(p["toc"]) for p in parts)
    print(f"site/index.html  {os.path.getsize(dst) / 1024:.0f} KB, 파트 {len(parts)}, 섹션 {total}")


if __name__ == "__main__":
    main()

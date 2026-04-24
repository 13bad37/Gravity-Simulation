#!/usr/bin/env python3

from __future__ import annotations

import re
from urllib.parse import quote
from pathlib import Path


README_PATH = Path("README.md")
ROADMAP_HEADING = re.compile(r"^##\s+Roadmap\s*$")
H2_HEADING = re.compile(r"^##\s+")
CHECKBOX = re.compile(r"^\s*-\s*\[([ xX])\]\s+")
ROADMAP_BADGE_TAG = re.compile(r'<img\b[^>]*\balt="Roadmap progress"[^>]*>')
IMG_SRC = re.compile(r'\bsrc="[^"]*"')


def extract_roadmap_lines(readme_text: str) -> list[str]:
    lines = readme_text.splitlines()
    in_roadmap = False
    roadmap_lines: list[str] = []

    for line in lines:
        if not in_roadmap:
            if ROADMAP_HEADING.match(line):
                in_roadmap = True
            continue

        if H2_HEADING.match(line):
            break

        roadmap_lines.append(line)

    if not roadmap_lines:
        raise RuntimeError("Could not find a Roadmap section in README.md")

    return roadmap_lines


def count_checkboxes(lines: list[str]) -> tuple[int, int]:
    done = 0
    total = 0

    for line in lines:
        match = CHECKBOX.match(line)
        if not match:
            continue

        total += 1
        if match.group(1).lower() == "x":
            done += 1

    if total == 0:
        raise RuntimeError("Roadmap section contains no checklist items")

    return done, total


def pick_color(progress: float) -> str:
    if progress >= 0.80:
        return "2ea44f"
    if progress >= 0.55:
        return "b08900"
    if progress >= 0.30:
        return "d97706"
    return "cf222e"


def build_badge_url(done: int, total: int, color: str) -> str:
    message = quote(f"{done}/{total} complete", safe="")
    return f"https://img.shields.io/badge/Roadmap-{message}-{color}?style=for-the-badge"


def main() -> None:
    readme_text = README_PATH.read_text(encoding="utf-8")
    roadmap_lines = extract_roadmap_lines(readme_text)
    done, total = count_checkboxes(roadmap_lines)
    progress = done / total

    badge_url = build_badge_url(done, total, pick_color(progress))

    badge_tag_match = ROADMAP_BADGE_TAG.search(readme_text)
    if badge_tag_match is None:
        raise RuntimeError("Could not find the roadmap badge image in README.md")

    badge_tag = badge_tag_match.group(0)
    updated_badge_tag = IMG_SRC.sub(f'src="{badge_url}"', badge_tag, count=1)
    updated_readme = readme_text.replace(badge_tag, updated_badge_tag, 1)

    if updated_readme == readme_text:
        print("Roadmap badge is already up to date.")
        return

    README_PATH.write_text(updated_readme, encoding="utf-8")

    print(f"Updated roadmap badge: {done}/{total} complete")


if __name__ == "__main__":
    main()

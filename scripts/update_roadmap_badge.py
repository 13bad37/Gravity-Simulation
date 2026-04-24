#!/usr/bin/env python3

from __future__ import annotations

import re
from urllib.parse import quote
from pathlib import Path


README_PATH = Path("README.md")
ROADMAP_HEADING = re.compile(r"^##\s+Roadmap\s*$")
H2_HEADING = re.compile(r"^##\s+")
CHECKBOX = re.compile(r"^\s*-\s*\[([ xX])\]\s+")
ROADMAP_BADGE = re.compile(r'(<img alt="Roadmap progress" src=")([^"]+)("([^>]*)/>)')


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
    updated_readme = ROADMAP_BADGE.sub(rf'\1{badge_url}\3', readme_text, count=1)

    if updated_readme == readme_text:
        raise RuntimeError("Could not find the roadmap badge image in README.md")

    README_PATH.write_text(updated_readme, encoding="utf-8")

    print(f"Updated roadmap badge: {done}/{total} complete")


if __name__ == "__main__":
    main()

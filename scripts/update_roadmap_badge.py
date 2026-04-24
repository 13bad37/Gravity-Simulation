#!/usr/bin/env python3

from __future__ import annotations

import html
import re
from pathlib import Path


README_PATH = Path("README.md")
BADGE_PATH = Path("assets/badges/roadmap-progress.svg")
ROADMAP_HEADING = re.compile(r"^##\s+Roadmap\s*$")
H2_HEADING = re.compile(r"^##\s+")
CHECKBOX = re.compile(r"^\s*-\s*\[([ xX])\]\s+")


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
        return "#2ea44f"
    if progress >= 0.55:
        return "#b08900"
    if progress >= 0.30:
        return "#d97706"
    return "#cf222e"


def text_width(text: str) -> int:
    return 12 + (7 * len(text))


def build_badge(label: str, message: str, color: str) -> str:
    label_width = text_width(label)
    message_width = text_width(message)
    total_width = label_width + message_width
    label_center = label_width / 2
    message_center = label_width + (message_width / 2)

    label = html.escape(label)
    message = html.escape(message)

    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{total_width}" height="20" role="img" aria-label="{label}: {message}">
  <title>{label}: {message}</title>
  <linearGradient id="smooth" x2="0" y2="100%">
    <stop offset="0" stop-color="#ffffff" stop-opacity=".08"/>
    <stop offset="1" stop-opacity=".08"/>
  </linearGradient>
  <clipPath id="round">
    <rect width="{total_width}" height="20" rx="3" fill="#fff"/>
  </clipPath>
  <g clip-path="url(#round)">
    <rect width="{label_width}" height="20" fill="#1f2328"/>
    <rect x="{label_width}" width="{message_width}" height="20" fill="{color}"/>
    <rect width="{total_width}" height="20" fill="url(#smooth)"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,DejaVu Sans,sans-serif" font-size="11">
    <text x="{label_center}" y="15" fill="#010101" fill-opacity=".3">{label}</text>
    <text x="{label_center}" y="14">{label}</text>
    <text x="{message_center}" y="15" fill="#010101" fill-opacity=".3">{message}</text>
    <text x="{message_center}" y="14">{message}</text>
  </g>
</svg>
"""


def main() -> None:
    readme_text = README_PATH.read_text(encoding="utf-8")
    roadmap_lines = extract_roadmap_lines(readme_text)
    done, total = count_checkboxes(roadmap_lines)
    progress = done / total

    badge_svg = build_badge(
        label="roadmap",
        message=f"{done}/{total} complete",
        color=pick_color(progress),
    )

    BADGE_PATH.parent.mkdir(parents=True, exist_ok=True)
    BADGE_PATH.write_text(badge_svg, encoding="utf-8")

    print(f"Updated roadmap badge: {done}/{total} complete")


if __name__ == "__main__":
    main()

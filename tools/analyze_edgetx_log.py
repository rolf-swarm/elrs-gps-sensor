#!/usr/bin/env python3
"""Analyze EdgeTX CSV logs containing ELRS link telemetry and GPS."""

from __future__ import annotations

import argparse
import csv
import math
import re
from pathlib import Path
from typing import Iterable


LAT_NAMES = {
    "lat",
    "latitude",
    "gpslat",
    "gps_lat",
    "gps lat",
    "gps latitude",
}

LON_NAMES = {
    "lon",
    "lng",
    "long",
    "longitude",
    "gpslon",
    "gpslng",
    "gps_lon",
    "gps_lng",
    "gps lon",
    "gps lng",
    "gps longitude",
}

GPS_PAIR_RE = re.compile(
    r"(?P<lat>[+-]?\d+(?:\.\d+)?)\D+(?P<lon>[+-]?\d+(?:\.\d+)?)"
)


def normalize_header(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", " ", value.strip().lower()).strip()


def compact_header(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", value.strip().lower())


def distance_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    earth_radius_m = 6371000.0
    p1 = math.radians(lat1)
    p2 = math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)

    a = math.sin(dp / 2.0) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2.0) ** 2
    return 2.0 * earth_radius_m * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))


def parse_float(value: str | None) -> float | None:
    if value is None:
        return None

    text = value.strip()
    if not text:
        return None

    text = text.replace(",", ".")
    try:
        return float(text)
    except ValueError:
        return None


def read_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    sample = path.read_text(encoding="utf-8-sig", errors="replace")[:4096]
    dialect = csv.Sniffer().sniff(sample, delimiters=",;\t")

    with path.open("r", encoding="utf-8-sig", errors="replace", newline="") as handle:
        reader = csv.DictReader(handle, dialect=dialect)
        rows = list(reader)
        fieldnames = list(reader.fieldnames or [])

    return fieldnames, rows


def find_named_column(fieldnames: Iterable[str], explicit: str | None, candidates: set[str]) -> str | None:
    if explicit:
        return explicit

    for name in fieldnames:
        normalized = normalize_header(name)
        compact = compact_header(name)
        if normalized in candidates or compact in candidates:
            return name

    return None


def find_gps_pair_column(fieldnames: Iterable[str]) -> str | None:
    for name in fieldnames:
        normalized = normalize_header(name)
        compact = compact_header(name)
        if normalized == "gps" or compact == "gps":
            return name
    return None


def parse_lat_lon(
    row: dict[str, str],
    lat_column: str | None,
    lon_column: str | None,
    gps_pair_column: str | None,
) -> tuple[float, float] | None:
    if lat_column and lon_column:
        lat = parse_float(row.get(lat_column))
        lon = parse_float(row.get(lon_column))
        if lat is not None and lon is not None:
            return lat, lon

    if gps_pair_column:
        match = GPS_PAIR_RE.search(row.get(gps_pair_column, ""))
        if match:
            return float(match.group("lat")), float(match.group("lon"))

    return None


def add_distances(
    rows: list[dict[str, str]],
    base_lat: float,
    base_lon: float,
    lat_column: str | None,
    lon_column: str | None,
    gps_pair_column: str | None,
) -> tuple[list[dict[str, str]], list[float]]:
    analyzed_rows: list[dict[str, str]] = []
    distances: list[float] = []

    for row in rows:
        analyzed = dict(row)
        lat_lon = parse_lat_lon(row, lat_column, lon_column, gps_pair_column)

        if lat_lon is None:
            analyzed["distance_m"] = ""
        else:
            lat, lon = lat_lon
            distance = distance_m(base_lat, base_lon, lat, lon)
            analyzed["distance_m"] = f"{distance:.2f}"
            distances.append(distance)

        analyzed_rows.append(analyzed)

    return analyzed_rows, distances


def write_output(path: Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    output_fieldnames = list(fieldnames)
    if "distance_m" not in output_fieldnames:
        output_fieldnames.append("distance_m")

    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=output_fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path, help="EdgeTX CSV log path")
    parser.add_argument("--base-lat", type=float, required=True, help="Base/Pocket latitude")
    parser.add_argument("--base-lon", type=float, required=True, help="Base/Pocket longitude")
    parser.add_argument("--lat-column", help="Latitude column name if auto-detect fails")
    parser.add_argument("--lon-column", help="Longitude column name if auto-detect fails")
    parser.add_argument("--gps-column", help="Combined GPS coordinate column name if used")
    parser.add_argument("--output", type=Path, help="Write CSV with added distance_m column")
    args = parser.parse_args()

    fieldnames, rows = read_rows(args.csv_path)
    if not rows:
        raise SystemExit("No rows found in CSV log.")

    lat_column = find_named_column(fieldnames, args.lat_column, LAT_NAMES)
    lon_column = find_named_column(fieldnames, args.lon_column, LON_NAMES)
    gps_pair_column = args.gps_column or find_gps_pair_column(fieldnames)

    analyzed_rows, distances = add_distances(
        rows,
        args.base_lat,
        args.base_lon,
        lat_column,
        lon_column,
        gps_pair_column,
    )

    if args.output:
        write_output(args.output, analyzed_rows, fieldnames)

    print(f"Rows read: {len(rows)}")
    print(f"Rows with GPS: {len(distances)}")
    print(f"Latitude column: {lat_column or 'not found'}")
    print(f"Longitude column: {lon_column or 'not found'}")
    print(f"Combined GPS column: {gps_pair_column or 'not used'}")

    if distances:
        print(f"Max distance: {max(distances):.1f} m")
        print(f"Last distance: {distances[-1]:.1f} m")
    else:
        print("No GPS samples parsed. Pass --lat-column/--lon-column or --gps-column.")

    if args.output:
        print(f"Wrote: {args.output}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())


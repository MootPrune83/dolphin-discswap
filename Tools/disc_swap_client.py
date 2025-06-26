#!/usr/bin/env python3
"""Example script for swapping discs via the Dolphin HTTP API."""

import argparse
import http.client
import urllib.parse


def main() -> None:
    parser = argparse.ArgumentParser(description="Swap the currently inserted disc in Dolphin.")
    parser.add_argument(
        "path",
        help=(
            "Path to the new disc image. Example: "
            r"D:\\Dolphin-x64\\Games\\MyGame.iso"
        ),
    )
    parser.add_argument("--host", default="localhost", help="Server hostname (default: localhost)")
    parser.add_argument("--port", type=int, default=8394, help="Server port (default: 8394)")
    args = parser.parse_args()

    encoded = urllib.parse.quote(args.path)
    conn = http.client.HTTPConnection(args.host, args.port, timeout=5)
    try:
        conn.request("GET", f"/swap?path={encoded}")
        resp = conn.getresponse()
        body = resp.read().decode()
        print(f"Status: {resp.status} {resp.reason}")
        print(body)
    finally:
        conn.close()


if __name__ == "__main__":
    main()

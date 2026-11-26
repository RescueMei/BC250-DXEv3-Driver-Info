#!/usr/bin/env python3
"""Pretty-print the BC250 DXEv3 driver telemetry variable.

Usage:
  decode_driver_info.py [path]

Default path is the efivarfs entry. The file starts with the 4-byte attribute
mask; the JSON payload follows, normally NUL-terminated.
"""

import json
import sys
from pathlib import Path

DEFAULT = "/sys/firmware/efi/efivars/MeiMeiDXEv3DriverInfo-49cc168d-e8b0-4613-a807-16969986726f"


def main():
    path = Path(sys.argv[1] if len(sys.argv) > 1 else DEFAULT)
    raw = path.read_bytes()
    if len(raw) <= 4:
        raise SystemExit("variable is empty")
    payload = raw[4:].rstrip(b"\x00").decode("utf-8")
    print(json.dumps(json.loads(payload), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()

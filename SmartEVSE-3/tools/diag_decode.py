#!/usr/bin/env python3
"""
diag_decode.py - Decode SmartEVSE .diag binary files to JSON

Usage:
    python3 diag_decode.py capture.diag              # print JSON to stdout
    python3 diag_decode.py capture.diag -o out.json   # write to file
    python3 diag_decode.py capture.diag --summary      # short summary only
"""

import argparse
import json
import struct
import sys
import zlib

# File header: "EVSE" + version(u8) + profile(u8) + snapshot_size(u16) +
#              count(u16) + firmware_version(16s) + serial_nr(u32) + start_uptime(u32)
HEADER_FMT = "<4sBBHH16sII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)  # 34 bytes

# Snapshot: 64 bytes packed
SNAPSHOT_FMT = "<I" "BBBBB" "3h3hh" "Hhh" "HHH" "BBBB" "BBB" "BBH" "bBB" "BBBB" "bB" "9s"
SNAPSHOT_SIZE = 64

STATE_NAMES = {
    0: "STATE_A", 1: "STATE_B", 2: "STATE_C", 3: "STATE_D",
    4: "COMM_B", 5: "COMM_B_OK", 6: "COMM_C", 7: "COMM_C_OK",
    8: "ACTSTART", 9: "B1", 10: "C1", 11: "MODEM_REQ",
    12: "MODEM_WAIT", 13: "MODEM_DONE", 14: "MODEM_DENIED", 255: "NOSTATE",
}

MODE_NAMES = {0: "NORMAL", 1: "SMART", 2: "SOLAR"}

PROFILE_NAMES = {0: "OFF", 1: "GENERAL", 2: "SOLAR", 3: "LOADBAL", 4: "MODBUS", 5: "FAST"}

ERROR_FLAGS = {
    1: "LESS_6A", 2: "CT_NOCOMM", 4: "TEMP_HIGH", 8: "EV_NOCOMM",
    16: "RCM_TRIPPED", 32: "RCM_TEST", 64: "Test_IO", 128: "BL_FLASH",
}


def decode_errors(flags):
    if flags == 0:
        return "NONE"
    names = [name for bit, name in ERROR_FLAGS.items() if flags & bit]
    return "|".join(names) if names else f"0x{flags:02X}"


def decode_header(data):
    if len(data) < HEADER_SIZE:
        raise ValueError(f"File too small for header ({len(data)} < {HEADER_SIZE})")

    fields = struct.unpack_from(HEADER_FMT, data)
    magic = fields[0]
    if magic != b"EVSE":
        raise ValueError(f"Bad magic: {magic!r} (expected b'EVSE')")

    fw = fields[5].rstrip(b"\x00").decode("ascii", errors="replace")
    return {
        "magic": magic.decode("ascii"),
        "version": fields[1],
        "profile": PROFILE_NAMES.get(fields[2], str(fields[2])),
        "snapshot_size": fields[3],
        "count": fields[4],
        "firmware": fw,
        "serial_nr": fields[6],
        "start_uptime": fields[7],
    }


def decode_snapshot(data, offset):
    if offset + SNAPSHOT_SIZE > len(data):
        raise ValueError(f"Truncated snapshot at offset {offset}")

    raw = data[offset:offset + SNAPSHOT_SIZE]
    # Unpack field by field due to complex layout
    pos = 0

    def u8():
        nonlocal pos
        v = raw[pos]
        pos += 1
        return v

    def s8():
        nonlocal pos
        v = struct.unpack_from("b", raw, pos)[0]
        pos += 1
        return v

    def u16():
        nonlocal pos
        v = struct.unpack_from("<H", raw, pos)[0]
        pos += 2
        return v

    def s16():
        nonlocal pos
        v = struct.unpack_from("<h", raw, pos)[0]
        pos += 2
        return v

    def u32():
        nonlocal pos
        v = struct.unpack_from("<I", raw, pos)[0]
        pos += 4
        return v

    snap = {}
    snap["t"] = u32()
    snap["state"] = STATE_NAMES.get(u8(), "UNKNOWN")
    snap["error"] = decode_errors(u8())
    snap["charge_delay"] = u8()
    snap["access_status"] = u8()
    snap["mode"] = MODE_NAMES.get(u8(), "UNKNOWN")

    snap["mains_L1"] = s16()
    snap["mains_L2"] = s16()
    snap["mains_L3"] = s16()
    snap["ev_L1"] = s16()
    snap["ev_L2"] = s16()
    snap["ev_L3"] = s16()
    snap["isum"] = s16()

    snap["charge_current"] = u16()
    snap["iset_balanced"] = s16()
    snap["override_current"] = u16()

    snap["solar_stop_timer"] = u16()
    snap["import_current"] = u16()
    snap["start_current"] = u16()

    snap["state_timer"] = u8()
    snap["c1_timer"] = u8()
    snap["access_timer"] = u8()
    snap["no_current"] = u8()

    snap["nr_phases_charging"] = u8()
    snap["switching_c2"] = u8()
    snap["enable_c2"] = u8()

    snap["load_bl"] = u8()
    snap["balanced_state_0"] = u8()
    snap["balanced_0"] = u16()

    snap["temp"] = s8()
    snap["rc_mon"] = u8()
    snap["pilot_reading"] = u8()

    snap["mains_meter_timeout"] = u8()
    snap["ev_meter_timeout"] = u8()
    snap["mains_meter_type"] = u8()
    snap["ev_meter_type"] = u8()

    snap["wifi_rssi"] = s8()
    snap["mqtt_connected"] = u8()

    return snap


def verify_crc(data, header):
    count = header["count"]
    payload_len = HEADER_SIZE + count * SNAPSHOT_SIZE
    if len(data) < payload_len + 4:
        return None  # No CRC present

    stored_crc = struct.unpack_from("<I", data, payload_len)[0]
    computed_crc = zlib.crc32(data[:payload_len]) & 0xFFFFFFFF
    return stored_crc == computed_crc


def decode_file(data):
    header = decode_header(data)
    crc_ok = verify_crc(data, header)

    snapshots = []
    offset = HEADER_SIZE
    for i in range(header["count"]):
        snap = decode_snapshot(data, offset)
        snapshots.append(snap)
        offset += SNAPSHOT_SIZE

    return {
        "header": header,
        "crc_valid": crc_ok,
        "snapshots": snapshots,
    }


def print_summary(result):
    h = result["header"]
    snaps = result["snapshots"]
    print(f"SmartEVSE Diagnostic Capture")
    print(f"  Firmware:     {h['firmware']}")
    print(f"  Serial:       {h['serial_nr']}")
    print(f"  Profile:      {h['profile']}")
    print(f"  Snapshots:    {h['count']}")
    print(f"  CRC valid:    {result['crc_valid']}")
    if snaps:
        print(f"  Time range:   {snaps[0]['t']}s - {snaps[-1]['t']}s "
              f"({snaps[-1]['t'] - snaps[0]['t']}s span)")
        states = set(s["state"] for s in snaps)
        print(f"  States seen:  {', '.join(sorted(states))}")
        errors = set(s["error"] for s in snaps if s["error"] != "NONE")
        if errors:
            print(f"  Errors seen:  {', '.join(sorted(errors))}")


def main():
    parser = argparse.ArgumentParser(description="Decode SmartEVSE .diag files")
    parser.add_argument("file", help="Path to .diag file")
    parser.add_argument("-o", "--output", help="Output JSON file (default: stdout)")
    parser.add_argument("--summary", action="store_true", help="Print summary only")
    args = parser.parse_args()

    with open(args.file, "rb") as f:
        data = f.read()

    result = decode_file(data)

    if args.summary:
        print_summary(result)
        return

    output = json.dumps(result, indent=2)
    if args.output:
        with open(args.output, "w") as f:
            f.write(output + "\n")
        print(f"Written to {args.output}", file=sys.stderr)
    else:
        print(output)


if __name__ == "__main__":
    main()

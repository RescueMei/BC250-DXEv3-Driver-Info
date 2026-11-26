# BC250-DXEv3-Driver-Info

Telemetry driver for the BC-250 DXEv3 family. It enumerates the driver identity
records published by the other DXE drivers and writes a runtime-only JSON
variable that the OS can read through efivarfs.

It has no configuration, no UI, and does not touch the SMU.

## At a glance

| Item | Value |
| --- | --- |
| Package / module | `BC250DXEv3DriverInfoPkg` -> `MeiMeiDXEv3_Driver_Info.efi` |
| Depex | `gEfiVariableArchProtocolGuid` |
| Produces | no protocol of its own beyond its identity record |
| Consumes | `gMeiMeiDXEv3DriverInfoProtocolGuid`, variable services |
| Configuration | none |
| Compliance | [COMPLIANCE.md](COMPLIANCE.md) |

## Dependencies

Only variable services are required for dispatch. The driver does not depend on
any DXEv3 driver: it waits for `ReadyToBoot`, by which point every dispatched
DXE driver has installed its identity record.

## How it works

1. At entry the driver installs its own `MEIMEIDXEV3_DRIVER_INFO` record and
   registers a `ReadyToBoot` callback.
2. At `ReadyToBoot` it enumerates every handle that publishes
   `gMeiMeiDXEv3DriverInfoProtocolGuid`.
3. Each record is validated (`Revision`, `Size`), copied, and sorted by name.
   Malformed records are counted, not trusted.
4. The JSON is built in a 4 KB pool buffer with per-entry all-or-nothing
   appends. If the buffer fills, the remaining entries are dropped and the
   document is marked `"truncated": true`.
5. The variable is written once, including a trailing NUL, and the buffer is
   freed.

A failed `SetVariable` is logged and ignored; the absence of the variable is a
valid state.

## Driver identity protocol

Every DXEv3 driver installs
`gMeiMeiDXEv3DriverInfoProtocolGuid` from
`BC250-DXEv3-Shared/MeiMeiDXEv3SharedPkg/Include/Protocol/MeiMeiDXEv3DriverInfo.h`:

```c
typedef struct {
  UINT32    Revision;
  UINT32    Size;
  CHAR16    Name[24];
  UINT32    Version;        // 0xMMmmpp
  EFI_GUID  DriverGuid;
  UINT16    ConfigSchema;
  UINT16    PayloadVersion;
  UINT32    Capabilities;
} MEIMEIDXEV3_DRIVER_INFO;
```

The protocol has no methods: the interface pointer is a `static const` record.

## Telemetry variable

| Item | Value |
| --- | --- |
| Name | `MeiMeiDXEv3DriverInfo` |
| Vendor GUID | `49CC168D-E8B0-4613-A807-16969986726F` |
| Attributes | `BOOTSERVICE_ACCESS \| RUNTIME_ACCESS` (volatile) |
| Payload | UTF-8 JSON plus one trailing NUL |
| Maximum size | 4 KB |

Schema v1:

```json
{"schema":1,"drivers":[
  {"name":"ACPI","version":"1.0.0","guid":"a9981d14-6329-4698-ad20-173c4363dd30","config":1,"payload":0,"caps":1},
  {"name":"Cold-Boot","version":"1.0.0","guid":"6c98e4f3-9d8b-4e5a-b2e7-31a45b9c5d81","config":0,"payload":0,"caps":0}
],"count":2}
```

- `version` is `major.minor.patch`.
- `guid` is the module GUID from the driver's INF.
- `config` is the shared config schema, or 0 when the driver has no variable.
- `payload` is the payload interface version, or 0 when there is none.
- `caps` is a bitmask: 1 = has configuration, 2 = injects a payload.
- `truncated` and `invalid` appear only when they are non-zero.

OS read path:

```
/sys/firmware/efi/efivars/MeiMeiDXEv3DriverInfo-49cc168d-e8b0-4613-a807-16969986726f
```

The first four bytes are the attribute mask; the rest is the JSON plus NUL.

`tools/decode_driver_info.py` strips both and pretty-prints the document.

## Protocols

Produces no consumable protocol. It consumes
`gMeiMeiDXEv3DriverInfoProtocolGuid` by enumeration; there is no lookup order
dependency.

## Build

```bash
git submodule update --init --recursive
./build_ffs.sh                                  # container build
NO_CONTAINER=1 EDK2_DIR=/path/to/edk2 ./build_ffs.sh   # host build
```

Output:

- `Build/Output/MeiMeiDXEv3_Driver_Info.efi`
- `Build/Output/MeiMeiDXEv3_Driver_Info.ffs`

## Validation

2026-11-25, nine-driver stack: `DEPLOY_OK` at a 2100 MHz ceiling, actual clock
2100 MHz sustained, <= 1006 mV, 202 W peak, 82 C peak, 0 ring/C2P timeouts. The
variable was present with attributes `0x6` (BS+RT), schema 1, and 9 entries. A
negative run without CU Unlock produced 8 entries with no `CU-Unlock` entry.
See [COMPLIANCE.md](COMPLIANCE.md).

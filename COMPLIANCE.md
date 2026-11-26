# Driver Info Compliance

Driver: `BC250-DXEv3-Driver-Info`
Shared package pin: `2e2e921` (feature branch; merged pin recorded after merge).

Checklist and evidence:

- [x] Shared package version/pin recorded.
- [x] Configuration ABI comes from the shared package; the driver has no configuration.
- [x] Public protocol copies match the provider ABI; no copies are vendored.
- [x] Driver-owned variable name/layout documented (`MeiMeiDXEv3DriverInfo`, schema v1).
- [x] Depex is correct; public protocol is installed unconditionally.
- [x] Feature-disabled path performs no feature writes (no feature gate exists).
- [x] Missing/malformed config is rejected safely; boot continues (no config).
- [x] No cross-driver private headers or internal functions.
- [x] Individual driver build passed.
- [x] Nine-driver integrated build passed.
- [x] BC250 deployment reached `DEPLOY_OK` with the variable present.
- [x] The JSON parsed from the OS and matched the loaded driver set.
- [x] Negative test: a stack without CU Unlock produced 8 entries and no `CU-Unlock`.
- [x] No generated payload headers, build output, secrets, or temporary files staged.

## Configuration

None. The driver reads no menu variable and writes only the volatile telemetry
variable.

## Protocol and Depex

- Depex: `gEfiVariableArchProtocolGuid`.
- Consumes `gMeiMeiDXEv3DriverInfoProtocolGuid` by enumeration at `ReadyToBoot`.
- Installs its own identity record before registering the callback.

## Telemetry variable

- Name: `MeiMeiDXEv3DriverInfo`, vendor GUID `49CC168D-...`.
- Attributes: `BOOTSERVICE_ACCESS | RUNTIME_ACCESS`, no non-volatile bit.
- Payload: schema-v1 JSON plus one trailing NUL, capped at 4 KB.
- Overflow drops entries and sets `"truncated": true`; malformed records are
  counted in `"invalid"`.

## Validation record

- Date: 2026-11-25.
- Integrated build: nine FFS/EFI pairs.
- Deployment: `DEPLOY_OK ceiling=2100 cores=8`, nine-driver stack.
- Workload: 60 s Valley + 60 s Superposition, actual 2100 MHz, <= 1006 mV,
  202 W peak, 82 C peak, 0 ring timeouts, 0 C2P timeouts.
- Variable: 1091 bytes, attributes `0x6`, schema 1, `count` 9.
- Negative run: 8 entries, no `CU-Unlock`.

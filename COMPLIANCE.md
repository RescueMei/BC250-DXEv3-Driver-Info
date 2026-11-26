# Driver Info Compliance

Driver: `BC250-DXEv3-Driver-Info`

Use this checklist before merging. Do not use the README as the compliance record.

- [x] Shared package submodule/pin present.
- [x] Configuration ABI comes from the shared package; the driver has no configuration.
- [x] Public protocol copies match the provider ABI; no copies are vendored.
- [x] Driver-owned variable names/layouts documented (`MeiMeiDXEv3DriverInfo`, schema v1).
- [ ] Depex is correct; public protocol is installed unconditionally.
- [ ] Feature-disabled path performs no feature writes (no feature gate exists).
- [ ] Missing/malformed config is rejected safely; boot continues (no config).
- [x] No cross-driver private headers or internal functions.
- [x] No forced-VID/force-state from SMU tick context (no SMU access).
- [ ] SMU writes are read-back verified; reserved fields preserved (no SMU writes).
- [x] No generated payload headers, build output, secrets, or temporary files staged.
- [ ] Individual driver build passed.
- [ ] Nine-driver integrated build passed.
- [ ] BC250 deployment reached `DEPLOY_OK` with the variable present.
- [ ] The JSON parsed from the OS and matched the loaded driver set.
- [ ] Payload is 4-byte aligned, fits its lane, and was injection-tested (no payload).

## Notes

- The identity record is installed before the `ReadyToBoot` callback is
  registered, so this driver appears in its own output.
- The variable is volatile (boot service + runtime, no non-volatile bit), so it
  is recreated on every boot and never wears NVRAM.
- The record has no enablement field by design; the OS reads the feature
  variables separately if it needs them.

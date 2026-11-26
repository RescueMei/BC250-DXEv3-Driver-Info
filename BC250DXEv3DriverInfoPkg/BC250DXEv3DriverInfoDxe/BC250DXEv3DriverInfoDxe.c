/** @file
  BC250-DXEv3-Driver-Info

  Enumerates the driver identity records published by the DXEv3 drivers and
  writes a runtime-only JSON variable for OS telemetry:

    MeiMeiDXEv3DriverInfo

  The record is built at ReadyToBoot, after every dispatched DXE driver has
  installed its MEIMEIDXEV3_DRIVER_INFO protocol.

  Copyright (C) 2026 RescueMei
  SPDX-License-Identifier: MIT
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Config/MeiMeiDXEv3Config.h>
#include <Protocol/MeiMeiDXEv3DriverInfo.h>

#define DRIVER_INFO_VAR_NAME  L"MeiMeiDXEv3DriverInfo"
#define DRIVER_INFO_VAR_ATTR  (EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)
#define DRIVER_INFO_CAPACITY  4096U
#define DRIVER_INFO_MAX       16U
#define DRIVER_INFO_ENTRY     384U

//
// Driver identity for this driver itself.
//
STATIC EFI_HANDLE  mDriverInfoHandle = NULL;

STATIC CONST MEIMEIDXEV3_DRIVER_INFO  mDriverInfo = {
  MEIMEIDXEV3_DRIVER_INFO_REVISION,
  sizeof (MEIMEIDXEV3_DRIVER_INFO),
  L"Driver-Info",
  MEIMEIDXEV3_MAKE_VERSION (1, 0, 0),
  { 0xC4D1A7E2, 0x5B3F, 0x4A8C, { 0x9E, 0x21, 0x7F, 0x0D, 0x6B, 0x3A, 0x9C, 0x58 } },
  0,
  0,
  0
};

STATIC MEIMEIDXEV3_DRIVER_INFO  mDrivers[DRIVER_INFO_MAX];

typedef struct {
  CHAR8    *Buffer;
  UINTN     Capacity;
  UINTN     Length;
  BOOLEAN   Truncated;
} JSON_WRITER;

STATIC
VOID
JsonAppend (
  IN OUT JSON_WRITER  *Writer,
  IN     CONST CHAR8  *Text
  )
{
  UINTN  Length;

  Length = AsciiStrLen (Text);
  if (Writer->Length + Length >= Writer->Capacity) {
    Writer->Truncated = TRUE;
    return;
  }
  CopyMem (Writer->Buffer + Writer->Length, Text, Length);
  Writer->Length += Length;
  Writer->Buffer[Writer->Length] = '\0';
}

STATIC
VOID
JsonAppendString (
  IN OUT JSON_WRITER   *Writer,
  IN     CONST CHAR16  *Text
  )
{
  CHAR8  Escaped[8];

  JsonAppend (Writer, "\"");
  for (; *Text != L'\0'; Text++) {
    if ((*Text == L'"') || (*Text == L'\\')) {
      Escaped[0] = '\\';
      Escaped[1] = (CHAR8)*Text;
      Escaped[2] = '\0';
      JsonAppend (Writer, Escaped);
    } else if (*Text < 0x20U) {
      AsciiSPrint (Escaped, sizeof (Escaped), "\\u%04x", (UINTN)*Text);
      JsonAppend (Writer, Escaped);
    } else {
      Escaped[0] = (CHAR8)*Text;
      Escaped[1] = '\0';
      JsonAppend (Writer, Escaped);
    }
  }
  JsonAppend (Writer, "\"");
}

STATIC
VOID
JsonAppendVersion (
  IN OUT JSON_WRITER  *Writer,
  IN     UINT32        Version
  )
{
  CHAR8  Text[24];

  AsciiSPrint (
    Text,
    sizeof (Text),
    "\"%u.%u.%u\"",
    (UINTN)((Version >> 16) & 0xFFU),
    (UINTN)((Version >> 8) & 0xFFU),
    (UINTN)(Version & 0xFFU)
    );
  JsonAppend (Writer, Text);
}

STATIC
BOOLEAN
FormatEntry (
  OUT CHAR8                            *Buffer,
  IN  UINTN                             Capacity,
  IN  CONST MEIMEIDXEV3_DRIVER_INFO     *Info
  )
{
  JSON_WRITER  Writer;
  CHAR8        Guid[40];
  CHAR8        Number[48];

  Writer.Buffer    = Buffer;
  Writer.Capacity  = Capacity;
  Writer.Length    = 0;
  Writer.Truncated = FALSE;
  Buffer[0] = '\0';

  JsonAppend (&Writer, "{\"name\":");
  JsonAppendString (&Writer, Info->Name);
  JsonAppend (&Writer, ",\"version\":");
  JsonAppendVersion (&Writer, Info->Version);

  AsciiSPrint (
    Guid,
    sizeof (Guid),
    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    (UINTN)Info->DriverGuid.Data1,
    (UINTN)Info->DriverGuid.Data2,
    (UINTN)Info->DriverGuid.Data3,
    (UINTN)Info->DriverGuid.Data4[0],
    (UINTN)Info->DriverGuid.Data4[1],
    (UINTN)Info->DriverGuid.Data4[2],
    (UINTN)Info->DriverGuid.Data4[3],
    (UINTN)Info->DriverGuid.Data4[4],
    (UINTN)Info->DriverGuid.Data4[5],
    (UINTN)Info->DriverGuid.Data4[6],
    (UINTN)Info->DriverGuid.Data4[7]
    );
  JsonAppend (&Writer, ",\"guid\":\"");
  JsonAppend (&Writer, Guid);
  JsonAppend (&Writer, "\"");

  AsciiSPrint (Number, sizeof (Number), ",\"config\":%u", (UINTN)Info->ConfigSchema);
  JsonAppend (&Writer, Number);
  AsciiSPrint (Number, sizeof (Number), ",\"payload\":%u", (UINTN)Info->PayloadVersion);
  JsonAppend (&Writer, Number);
  AsciiSPrint (Number, sizeof (Number), ",\"caps\":%u", (UINTN)Info->Capabilities);
  JsonAppend (&Writer, Number);
  JsonAppend (&Writer, "}");

  return !Writer.Truncated;
}

STATIC
VOID
SortDrivers (
  IN OUT MEIMEIDXEV3_DRIVER_INFO  *List,
  IN     UINTN                    Count
  )
{
  UINTN                    Index;
  UINTN                    Next;
  MEIMEIDXEV3_DRIVER_INFO  Temp;

  for (Index = 1; Index < Count; Index++) {
    CopyMem (&Temp, &List[Index], sizeof (Temp));
    Next = Index;
    while ((Next > 0) && (StrCmp (List[Next - 1].Name, Temp.Name) > 0)) {
      CopyMem (&List[Next], &List[Next - 1], sizeof (Temp));
      Next--;
    }
    CopyMem (&List[Next], &Temp, sizeof (Temp));
  }
}

STATIC
UINTN
CollectDrivers (
  IN  MEIMEIDXEV3_DRIVER_INFO  *List,
  OUT UINTN                    *Invalid,
  OUT BOOLEAN                  *Overflow
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE               *Handles;
  UINTN                     HandleCount;
  UINTN                     Index;
  UINTN                     Count;
  MEIMEIDXEV3_DRIVER_INFO  *Info;

  *Invalid  = 0;
  *Overflow = FALSE;
  Count     = 0;
  Handles   = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gMeiMeiDXEv3DriverInfoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (Status == EFI_NOT_FOUND) {
    return 0;
  }
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BC250DXEv3DriverInfo: handle enumeration failed: %r\n", Status));
    return 0;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gMeiMeiDXEv3DriverInfoProtocolGuid,
                    (VOID **)&Info
                    );
    if (EFI_ERROR (Status) || (Info == NULL) ||
        (Info->Revision != MEIMEIDXEV3_DRIVER_INFO_REVISION) ||
        (Info->Size < sizeof (MEIMEIDXEV3_DRIVER_INFO)))
    {
      (*Invalid)++;
      continue;
    }
    if (Count >= DRIVER_INFO_MAX) {
      *Overflow = TRUE;
      break;
    }
    CopyMem (&List[Count], Info, sizeof (MEIMEIDXEV3_DRIVER_INFO));
    List[Count].Name[MEIMEIDXEV3_DRIVER_NAME_MAX - 1U] = L'\0';
    Count++;
  }

  FreePool (Handles);
  return Count;
}

STATIC
VOID
EFIAPI
DriverInfoReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    VariableGuid = MEIMEIDXEV3_CONFIG_VAR_GUID;
  CHAR8      *Buffer;
  CHAR8       Entry[DRIVER_INFO_ENTRY];
  CHAR8       Number[48];
  JSON_WRITER Writer;
  UINTN       Count;
  UINTN       Written;
  UINTN       Index;
  UINTN       Invalid;
  BOOLEAN     Overflow;

  (VOID)Event;
  (VOID)Context;

  Count = CollectDrivers (mDrivers, &Invalid, &Overflow);
  SortDrivers (mDrivers, Count);

  Buffer = AllocatePool (DRIVER_INFO_CAPACITY);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "BC250DXEv3DriverInfo: JSON buffer allocation failed\n"));
    return;
  }

  Writer.Buffer    = Buffer;
  Writer.Capacity  = DRIVER_INFO_CAPACITY;
  Writer.Length    = 0;
  Writer.Truncated = FALSE;
  Buffer[0] = '\0';

  JsonAppend (&Writer, "{\"schema\":1,\"drivers\":[");
  Written = 0;
  for (Index = 0; Index < Count; Index++) {
    if (!FormatEntry (Entry, sizeof (Entry), &mDrivers[Index])) {
      Writer.Truncated = TRUE;
      break;
    }
    if (Writer.Length + AsciiStrLen (Entry) + 128U >= Writer.Capacity) {
      Writer.Truncated = TRUE;
      break;
    }
    if (Written != 0) {
      JsonAppend (&Writer, ",");
    }
    JsonAppend (&Writer, Entry);
    Written++;
  }

  JsonAppend (&Writer, "]");
  AsciiSPrint (Number, sizeof (Number), ",\"count\":%u", (UINTN)Written);
  JsonAppend (&Writer, Number);
  if (Written != Count) {
    Writer.Truncated = TRUE;
  }
  if (Overflow) {
    Writer.Truncated = TRUE;
  }
  if (Invalid != 0) {
    AsciiSPrint (Number, sizeof (Number), ",\"invalid\":%u", (UINTN)Invalid);
    JsonAppend (&Writer, Number);
  }
  if (Writer.Truncated) {
    JsonAppend (&Writer, ",\"truncated\":true");
  }
  JsonAppend (&Writer, "}\n");

  Status = gRT->SetVariable (
                  DRIVER_INFO_VAR_NAME,
                  &VariableGuid,
                  DRIVER_INFO_VAR_ATTR,
                  Writer.Length + 1U,
                  Buffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BC250DXEv3DriverInfo: SetVariable failed: %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "BC250DXEv3DriverInfo: published %u drivers (%u bytes)\n",
      (UINT32)Written, (UINT32)Writer.Length));
  }

  FreePool (Buffer);
}

EFI_STATUS
EFIAPI
BC250DXEv3DriverInfoEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  //
  // Publish this driver's own identity record first, so it also appears in the
  // telemetry output.
  //
  if (mDriverInfoHandle == NULL) {
    Status = gBS->InstallProtocolInterface (
                    &mDriverInfoHandle,
                    &gMeiMeiDXEv3DriverInfoProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    (VOID *)&mDriverInfo
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "BC250DXEv3DriverInfo: info install failed: %r\n", Status));
    }
  }

  Status = EfiCreateEventReadyToBootEx (
             TPL_CALLBACK,
             DriverInfoReadyToBoot,
             NULL,
             &Event
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "BC250DXEv3DriverInfo: ReadyToBoot event failed: %r\n", Status));
  }

  return EFI_SUCCESS;
}

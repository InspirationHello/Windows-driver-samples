#ifndef _MSVAD_ADAPTER_H_
#define _MSVAD_ADAPTER_H_

#include <msvad.h>

extern "C" NTSTATUS
AudioDeviceDriverEntry
(
    IN  PDRIVER_OBJECT          DriverObject,
    IN  PUNICODE_STRING         RegistryPathName
);

#endif // !_MSVAD_ADAPTER_H_

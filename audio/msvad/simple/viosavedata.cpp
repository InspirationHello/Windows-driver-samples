#pragma warning (disable : 4127)
#pragma warning (disable : 26165)

#include <ntifs.h>

#include <msvad.h>
#include "savedata.h"
#include <ntstrsafe.h>   // This is for using RtlStringcbPrintf
#include <wdm.h>
#include "viosavedata.h"

#include <initguid.h>

DEFINE_GUID(GUID_DEVINTERFACE_VioAudio,
    0xc45687db, 0xd100, 0x4da4, 0xb7, 0x60, 0x31, 0x59, 0x2e, 0x61, 0x06, 0xaa);
// {c45687db-d100-4da4-b760-31592e6106aa}

enum VIRTIO_AUDIO_DEVICE_EVENT {
    VIRTIO_AUDIO_DEVICE_READY = 0,
    VIRTIO_AUDIO_DEVICE_OPEN,
    VIRTIO_AUDIO_DEVICE_CLOSE,
    VIRTIO_AUDIO_DEVICE_ENABLE,
    VIRTIO_AUDIO_DEVICE_DISABLE,
    VIRTIO_AUDIO_DEVICE_SET_FORMAT,
    VIRTIO_AUDIO_DEVICE_SET_STATE,
    VIRTIO_AUDIO_DEVICE_SET_VOLUME,
    VIRTIO_AUDIO_DEVICE_SET_MUTE,
    VIRTIO_AUDIO_DEVICE_GET_MUTE,
    NR_VIRTIO_AUDIO_DEVICE_EVENT
};

typedef struct _VIRTIO_AUDIO_FORMAT {
    UINT16 channels;
    UINT16 bits_per_sample;
    UINT32 samples_per_sec;
    UINT32 avg_bytes_per_sec;
}VIRTIO_AUDIO_FORMAT, *PVIRTIO_AUDIO_FORMAT;

#define IOCTL_VIOAUDIO_DEVICE_OPEN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_DEVICE_CLOSE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_SEND_DATA   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_GET_DATA   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_SET_FORMAT  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_GET_FORMAT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_SET_DISABLE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_SET_STATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_SET_VOLUME CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_SET_MUTE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VIOAUDIO_GET_MUTE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)

CVioSaveData::CVioSaveData() :
    m_DevObj(NULL)
{
}

CVioSaveData::~CVioSaveData()
{
    if (m_FileHandle) {
        // FileClose();
    }

    if (m_DevObj) {
        ObDereferenceObject(m_DevObj);
    }
}

NTSTATUS CVioSaveData::Initialize(ULONG StreamId)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PWSTR    symbolicLinkList;

    UNREFERENCED_PARAMETER(StreamId);

    ntStatus = IoGetDeviceInterfaces(
        &GUID_DEVINTERFACE_VioAudio,
        NULL,
        0,
        &symbolicLinkList
    );

    if (NT_SUCCESS(ntStatus) && NULL != symbolicLinkList) {
        PFILE_OBJECT   fileObj;
        PDEVICE_OBJECT devObj;
        UNICODE_STRING objName;

        IO_STATUS_BLOCK ioStatus = { 0 };

        for (PWSTR symbolicLink = symbolicLinkList;
            symbolicLink[0] != NULL && symbolicLink[1] != NULL;
            symbolicLink += wcslen(symbolicLink) + 1) {

            RtlInitUnicodeString(&objName, symbolicLink);

            DPF_ENTER(("VioAudioSaveData: Get object name: %wZ\n", &objName));

            ntStatus = IoGetDeviceObjectPointer(
                &objName,
                FILE_ALL_ACCESS,
                &fileObj,
                &devObj
            );

            if (NT_SUCCESS(ntStatus)) {
                
                ntStatus = ObReferenceObjectByPointer(devObj, FILE_ALL_ACCESS, NULL, KernelMode);
                if (!NT_SUCCESS(ntStatus)) {
                    DPF_ENTER(("VioAudioSaveData: ObReferenceObjectByPointer Successed with status 0x%08x\n", ntStatus));
                    continue;
                }

                m_FileName.Length = 0;
                m_FileName.MaximumLength = (USHORT)((objName.Length * sizeof(WCHAR)) + sizeof(WCHAR));//convert to wchar and add room for NULL
                m_FileName.Buffer = (PWSTR)
                    ExAllocatePoolWithTag
                    (
                        PagedPool,
                        m_FileName.MaximumLength,
                        MSVAD_POOLTAG
                    );
                if (!m_FileName.Buffer) {
                    ObDereferenceObject(devObj);
                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                    DPF(D_TERSE, ("[Could not allocate memory for FileName]"));

                    break;
                }

                RtlUnicodeStringCopy(&m_FileName, &objName);

                m_DevObj = devObj;

                DPF_ENTER(("VioAudioSaveData: IoGetDeviceObjectPointer Successed with status 0x%08x\n", ntStatus));
                break;
            }
            else {
                DPF_ENTER(("VioAudioSaveData: IoGetDeviceObjectPointer Failed with status 0x%08x\n", ntStatus));
            }
        }
    }

    ExFreePool(symbolicLinkList);

    if (NT_SUCCESS(ntStatus)) {
        // Create data file.
        InitializeObjectAttributes
        (
            &m_objectAttributes,
            &m_FileName,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,
            NULL
        );
    }

    DPF_ENTER(("VioAudioSaveData: FileOpen Final status 0x%08x\n", ntStatus));

    return ntStatus;
}

void CVioSaveData::Disable
(
    BOOL fDisable
)
{
    if (!m_DevObj) {
        return;
    }

    IoCtrlTo(IOCTL_VIOAUDIO_SET_DISABLE, &fDisable, sizeof(fDisable));
}

DWORD CVioSaveData::IoCtrlTo(DWORD CtrlCode, PVOID InBuffer, DWORD InSize)
{
    IO_STATUS_BLOCK ioStatus = { 0 };
    KEVENT event;
    PIRP irp;

    if (!m_DevObj) {
        DPF_ENTER(("VioAudioSaveData: the device object is null\n"));
        return 0;
    }

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
        CtrlCode,
        m_DevObj,
        InBuffer, InSize,
        NULL, 0,
        FALSE,
        &event,
        &ioStatus
    );

    if (irp) {
        if (IoCallDriver(m_DevObj, irp) == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        }

        if (ioStatus.Status == 0) {
            return ioStatus.Information;
        } else {
            DPF_ENTER(("VioAudioSaveData: IRP for RELEASE_MMAP failed with status %u\n", ioStatus.Status));
        }
    }
    else {
        DPF_ENTER(("VioAudioSaveData: Failed to get the IRP for %u\n", CtrlCode));
    }

    return 0;
}

DWORD CVioSaveData::IoCtrlFrom(DWORD CtrlCode, PVOID OutBuffer, DWORD OutSize)
{
    IO_STATUS_BLOCK ioStatus = { 0 };
    KEVENT event;
    PIRP irp;

    if (!m_DevObj) {
        DPF_ENTER(("VioAudioSaveData: the device object is null\n"));
        return 0;
    }

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
        CtrlCode,
        m_DevObj,
        NULL, 0,
        &OutBuffer, OutSize,
        FALSE,
        &event,
        &ioStatus
    );

    if (irp) {
        if (IoCallDriver(m_DevObj, irp) == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        }

        if (ioStatus.Status == 0) {
            return ioStatus.Information;
        }
        else {
            DPF_ENTER(("VioAudioSaveData: IRP for %u failed with status 0x%08x\n", CtrlCode, ioStatus.Status));
        }
    }
    else {
        DPF_ENTER(("VioAudioSaveData: Failed to get the IRP for %u\n", CtrlCode));
    }

    return 0;
}

NTSTATUS CVioSaveData::SetDataFormat(IN  PWAVEFORMATEX pWaveFormat)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    VIRTIO_AUDIO_FORMAT audioFormat = { 0 };

    if (!pWaveFormat || !m_DevObj) {
        return STATUS_SUCCESS;
    }

    audioFormat.channels = pWaveFormat->nChannels;
    audioFormat.bits_per_sample = (UINT16)pWaveFormat->wBitsPerSample;
    audioFormat.samples_per_sec = (UINT32)pWaveFormat->nSamplesPerSec;
    audioFormat.avg_bytes_per_sec = (UINT32)pWaveFormat->nAvgBytesPerSec; 

    IoCtrlTo(IOCTL_VIOAUDIO_SET_FORMAT, &audioFormat, sizeof(audioFormat));

    return ntStatus;
}

NTSTATUS CVioSaveData::SetState
(
    _In_  KSSTATE           NewState
)
{
    LONG iState = (LONG)NewState;

    if (!m_DevObj) {
        return STATUS_SUCCESS;
    }

    IoCtrlTo(IOCTL_VIOAUDIO_SET_STATE, &iState, sizeof(iState));

    return STATUS_SUCCESS;
}

void CVioSaveData::SetVolume
(
    _In_  LONG              Channel,
    _In_  LONG              Value
)
{
    UNREFERENCED_PARAMETER(Channel);

    if (!m_DevObj) {
        return;
    }

    IoCtrlTo(IOCTL_VIOAUDIO_SET_VOLUME, &Value, sizeof(Value));
}

NTSTATUS CVioSaveData::FileClose(void)
{
    return CSaveBackend::FileClose();
}

NTSTATUS CVioSaveData::FileOpen(IN BOOL fOverWrite)
{
#if 0
    PWSTR symbolicLinkList;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(fOverWrite);

    ntStatus = IoGetDeviceInterfaces(
        &GUID_DEVINTERFACE_VioAudio,
        NULL,
        0,
        &symbolicLinkList
    );

    if (NT_SUCCESS(ntStatus) && NULL != symbolicLinkList) {
        PFILE_OBJECT   fileObj;
        PDEVICE_OBJECT devObj;
        UNICODE_STRING objName;

        IO_STATUS_BLOCK ioStatus = { 0 };

        for (PWSTR symbolicLink = symbolicLinkList;
            symbolicLink[0] != NULL && symbolicLink[1] != NULL;
            symbolicLink += wcslen(symbolicLink) + 1) {

            RtlInitUnicodeString(&objName, symbolicLink);

            DPF_ENTER(("VioAudioSaveData: Get object name: %wZ\n", &objName));

            ntStatus = IoGetDeviceObjectPointer(
                &objName,
                FILE_ALL_ACCESS,
                &fileObj,
                &devObj
            );

            if (NT_SUCCESS(ntStatus)) {
                ntStatus = ObOpenObjectByPointer(fileObj, OBJ_KERNEL_HANDLE, NULL, GENERIC_ALL,
                    NULL, KernelMode, &m_FileHandle);

                ObDereferenceObject(fileObj);

                if (!NT_SUCCESS(ntStatus)) {
                    DPF_ENTER(("VioAudioSaveData: ObOpenObjectByPointer Failed with status 0x%08x\n", ntStatus));
                    continue;
                }

                ntStatus = ObReferenceObjectByPointer(devObj, FILE_ALL_ACCESS, NULL, KernelMode);
                if (!NT_SUCCESS(ntStatus)) {
                    DPF_ENTER(("VioAudioSaveData: ObReferenceObjectByPointer Successed with status 0x%08x\n", ntStatus));
                    continue;
                }

                // if failed at this scope: do ObDereferenceObject(devObj);

                m_DevObj = devObj;

                DPF_ENTER(("VioAudioSaveData: IoGetDeviceObjectPointer Successed with status 0x%08x\n", ntStatus));
                break;
            }
            else {
                DPF_ENTER(("VioAudioSaveData: IoGetDeviceObjectPointer Failed with status 0x%08x\n", ntStatus));
            }
        }
    }

    ExFreePool(symbolicLinkList);

    DPF_ENTER(("VioAudioSaveData: FileOpen Final status 0x%08x\n", ntStatus));

    return ntStatus; // CSaveBackend::FileOpen(fOverWrite);
#else
    return CSaveBackend::FileOpen(fOverWrite);
#endif
}

NTSTATUS CVioSaveData::FileWrite(PBYTE pData, ULONG ulDataSize)
{
    return CSaveBackend::FileWrite(pData, ulDataSize);
}

NTSTATUS CVioSaveData::FileWriteHeader(void)
{
    return CSaveBackend::FileWriteHeader();
}

void
CVioSaveData::SetMute
(
    _In_  BOOL              Value
)
{
    if (!m_DevObj) {
        return;
    }

    IoCtrlTo(IOCTL_VIOAUDIO_SET_MUTE, &Value, sizeof(Value));
}

BOOL
CVioSaveData::GetMute
(
)
{
    return FALSE;
}


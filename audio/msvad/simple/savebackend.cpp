/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    savedata.cpp

Abstract:

    Implementation of MSVAD data saving class.

    To save the playback data to disk, this class maintains a circular data
    buffer, associated frame structures and worker items to save frames to
    disk.
    Each frame structure represents a portion of buffer. When that portion
    of frame is full, a workitem is scheduled to save it to disk.



--*/
#pragma warning (disable : 4127)
#pragma warning (disable : 26165)

#include <msvad.h>
#include "savebackend.h"
#include <ntstrsafe.h>   // This is for using RtlStringcbPrintf

#define DEFAULT_FILE_NAME           L"\\DosDevices\\C:\\VioAudio"

#pragma code_seg("PAGE")
//=============================================================================
// CSaveBackend
//=============================================================================

//=============================================================================
CSaveBackend::CSaveBackend() :
    m_FileHandle(NULL),
    m_waveFormat(NULL),
    m_pFilePtr(NULL)
{

    PAGED_CODE();

    RtlZeroMemory(&m_objectAttributes, sizeof(m_objectAttributes));

    DPF_ENTER(("[CSaveBackend::CSaveBackend]"));
} // CSaveBackend

//=============================================================================
CSaveBackend::~CSaveBackend()
{
    PAGED_CODE();

    DPF_ENTER(("[CSaveBackend::~CSaveBackend]"));

    if (m_FileName.Buffer) {
        ExFreePoolWithTag(m_FileName.Buffer, MSVAD_POOLTAG);
    }
} // CSaveBackend

//=============================================================================

//=============================================================================
void
CSaveBackend::Disable
(
    BOOL                        fDisable
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(fDisable);

    DPF_ENTER(("[CSaveBackend::Disable]"));
} // Disable

//=============================================================================
NTSTATUS
CSaveBackend::FileClose(void)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_SUCCESS;

    DPF_ENTER(("[CSaveBackend::FileClose]"));

    if (m_FileHandle) {
        ntStatus = ZwClose(m_FileHandle);
        m_FileHandle = NULL;
    }

    return ntStatus;
} // FileClose

//=============================================================================
NTSTATUS
CSaveBackend::FileOpen
(
    IN  BOOL                    fOverWrite
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    IO_STATUS_BLOCK             ioStatusBlock;

    DPF_ENTER(("[CSaveBackend::FileOpen]"));

    if (!m_FileHandle) {
        ntStatus =
            ZwCreateFile
            (
                &m_FileHandle,
                GENERIC_WRITE | SYNCHRONIZE | FILE_APPEND_DATA,
                &m_objectAttributes,
                &ioStatusBlock,
                NULL,
                FILE_ATTRIBUTE_NORMAL,
                0,
                fOverWrite ? FILE_OVERWRITE_IF : FILE_OPEN_IF,
                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                NULL,
                0
            );
        if (!NT_SUCCESS(ntStatus)) {
            DPF(D_TERSE, ("[CSaveData::FileOpen : Error opening data file]"));
        }
    }

    return ntStatus;
} // FileOpen

//=============================================================================
NTSTATUS
CSaveBackend::FileWrite
(
    _In_reads_bytes_(ulDataSize)    PBYTE   pData,
    _In_                            ULONG   ulDataSize
)
{
    PAGED_CODE();

    ASSERT(pData);

    NTSTATUS                    ntStatus = STATUS_INVALID_HANDLE;

    DPF_ENTER(("[CSaveBackend::FileWrite ulDataSize=%lu]", ulDataSize));

    if (m_FileHandle) {
        IO_STATUS_BLOCK         ioStatusBlock;

        ntStatus = ZwWriteFile(m_FileHandle,
            NULL,
            NULL,
            NULL,
            &ioStatusBlock,
            pData,
            ulDataSize,
            NULL,
            NULL);

        if (NT_SUCCESS(ntStatus)) {
            ASSERT(ioStatusBlock.Information == ulDataSize);

            // m_pFilePtr->QuadPart += ulDataSize;
        } else {
            DPF(D_TERSE, ("[CSaveData::FileWrite : WriteFile Error]"));
        }
    } else {
        DPF(D_TERSE, ("[CSaveData::FileWrite : File not open]"));
        ntStatus = STATUS_INVALID_HANDLE;
    }

    return ntStatus;
} // FileWrite

//=============================================================================
NTSTATUS
CSaveBackend::FileWriteHeader(void)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_SUCCESS;

    DPF_ENTER(("[CSaveBackend::FileWriteHeader]"));
    
    return ntStatus;
} // FileWriteHeader

#pragma code_seg()

#pragma code_seg("PAGE")

//=============================================================================
NTSTATUS
CSaveBackend::Initialize
(
    ULONG StreamId
)
{
    size_t      cLen;
    WCHAR       szTemp[MAX_PATH];
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    PAGED_CODE();

    DPF_ENTER(("[CSaveBackend::Initialize]"));

    RtlStringCchPrintfW(szTemp, ARRAYSIZE(szTemp), L"%s_%d.wav", DEFAULT_FILE_NAME, StreamId);
    m_FileName.Length = 0;
    ntStatus = RtlStringCchLengthW (szTemp, sizeof(szTemp)/sizeof(szTemp[0]), &cLen);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_TERSE, ("[Could not RtlStringCchLengthW for 0x%08x]", ntStatus));
        return ntStatus;
    }

    m_FileName.MaximumLength = (USHORT)((cLen * sizeof(WCHAR)) + sizeof(WCHAR));//convert to wchar and add room for NULL
    m_FileName.Buffer = (PWSTR)
        ExAllocatePoolWithTag
        (
            PagedPool,
            m_FileName.MaximumLength,
            MSVAD_POOLTAG
        );
    if (!m_FileName.Buffer) {
        DPF(D_TERSE, ("[Could not allocate memory for FileName]"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlUnicodeStringCopyString(&m_FileName, szTemp);

    // Create data file.
    InitializeObjectAttributes
    (
        &m_objectAttributes,
        &m_FileName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );

    return ntStatus;
} // Initialize

//=============================================================================
NTSTATUS
CSaveBackend::SetDataFormat
(
    IN  PWAVEFORMATEX       pWaveFormat
)
{
    PAGED_CODE();
    NTSTATUS ntStatus = STATUS_SUCCESS;
 
    DPF_ENTER(("[CSaveBackend::SetDataFormat]"));

    ASSERT(pWaveFormat);

    // !!! TODO !!! deep copy
    m_waveFormat = pWaveFormat;

    return ntStatus;
} // SetDataFormat

//=============================================================================
NTSTATUS 
CSaveBackend::SetState
(
    _In_  KSSTATE           NewState
)
{
    UNREFERENCED_PARAMETER(NewState);

    return STATUS_SUCCESS;
} // SetDataFormat

//=============================================================================
void 
CSaveBackend::SetVolume
(
    _In_  LONG              Channel,
    _In_  LONG              Value
)
{
    UNREFERENCED_PARAMETER(Channel);
    UNREFERENCED_PARAMETER(Value);
} // SetDataFormat

//=============================================================================
void 
CSaveBackend::SetMute
(
    _In_  BOOL              Value
)
{
    UNREFERENCED_PARAMETER(Value);
} // SetMute

//=============================================================================
BOOL 
CSaveBackend::GetMute
(
)
{
    return FALSE;
} // GetMute


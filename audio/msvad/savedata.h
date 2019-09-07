/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    savedata.h

Abstract:

    Declaration of MSVAD data saving class. This class supplies services
to save data to disk.


--*/

#ifndef _MSVAD_SAVEDATA_H
#define _MSVAD_SAVEDATA_H

#include "savebackend.h"

//-----------------------------------------------------------------------------
//  Forward declaration
//-----------------------------------------------------------------------------
class CSaveData;
typedef CSaveData *PCSaveData;


#define uint8_t UINT8
#define uint32_t UINT32
#define int32_t INT32
#define roundup_pow_of_two RoundupPowOfTwo
#define rbuf_lock_t KMUTEX // KSPIN_LOCK

typedef struct ring_buffer_ {
    uint8_t           *data;
    uint32_t           size, size_mask;
    uint32_t           rpos;
    uint32_t           wpos;

    rbuf_lock_t        lock;
}ring_buffer_t;


//-----------------------------------------------------------------------------
//  Structs
//-----------------------------------------------------------------------------

// Parameter to workitem.
#include <pshpack1.h>
typedef struct _SAVEWORKER_PARAM {
    PIO_WORKITEM     WorkItem;
    ULONG            ulFrameNo;
    ULONG            ulDataSize;
    PBYTE            pData;
    PCSaveData       pSaveData;
    KEVENT           EventDone;
} SAVEWORKER_PARAM;
typedef SAVEWORKER_PARAM *PSAVEWORKER_PARAM;
#include <poppack.h>

// wave file header.
#include <pshpack1.h>
typedef struct _OUTPUT_FILE_HEADER
{
    DWORD           dwRiff;
    DWORD           dwFileSize;
    DWORD           dwWave;
    DWORD           dwFormat;
    DWORD           dwFormatLength;
} OUTPUT_FILE_HEADER;
typedef OUTPUT_FILE_HEADER *POUTPUT_FILE_HEADER;

typedef struct _OUTPUT_DATA_HEADER
{
    DWORD           dwData;
    DWORD           dwDataLength;
} OUTPUT_DATA_HEADER;
typedef OUTPUT_DATA_HEADER *POUTPUT_DATA_HEADER;

#include <poppack.h>

#define MAX_NR_SAVE_BACKEND (4)
// #define WORTK_THREAD_MODE

//-----------------------------------------------------------------------------
//  Classes
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// CSaveData
//   Saves the wave data to disk.
//
IO_WORKITEM_ROUTINE SaveFrameWorkerCallback;

class CSaveData
{
protected:
    UNICODE_STRING              m_FileName;         // DataFile name.
    HANDLE                      m_FileHandle;       // DataFile handle.
    PBYTE                       m_pDataBuffer;      // Data buffer.
    ULONG                       m_ulBufferSize;     // Total buffer size. must be pow of two
    ULONG                       m_ulBufferSizeMask; // Buffer size mask.

    ULONG                       m_ulFramePtr;       // Current Frame.
    ULONG                       m_ulFrameCount;     // Frame count.
    ULONG                       m_ulFrameSize;
    ULONG                       m_ulBufferPtr;      // Pointer in buffer.
    PBOOL                       m_fFrameUsed;       // Frame usage table.
    KSPIN_LOCK                  m_FrameInUseSpinLock; // Spinlock for synch.
    KMUTEX                      m_FileSync;         // Synchronizes file access

    OBJECT_ATTRIBUTES           m_objectAttributes; // Used for opening file.

    OUTPUT_FILE_HEADER          m_FileHeader;
    PWAVEFORMATEX               m_waveFormat;
    OUTPUT_DATA_HEADER          m_DataHeader;
    PLARGE_INTEGER              m_pFilePtr;

    static PDEVICE_OBJECT       m_pDeviceObject;
    static ULONG                m_ulStreamId;
    static PSAVEWORKER_PARAM    m_pWorkItems;

    BOOL                        m_fWriteDisabled;

    BOOL                        m_bInitialized;

    INT64                       m_i64Wpos, m_i64Rpos;
    volatile ULONG              m_ulTransferChunkSize;

    ring_buffer_t               m_rbuf;

    // for batch read or write: recommend heap value, such as new or malloc.
    static PCSaveBackend        m_SaveBackends[MAX_NR_SAVE_BACKEND];
    static INT                  m_SaveBackendsLength;

#ifdef WORTK_THREAD_MODE
    KEVENT                      m_WakeUpWorker;
    PKTHREAD                    m_Worker;
    BOOL                        m_bShutDownWorker;

    friend NTSTATUS
        CreateWorkerThread(
            IN PCSaveData  Device
        );
    friend VOID
        SendPcmRoutine(
            IN PVOID pContext
        );
#endif // WORTK_THREAD_MODE


public:
    CSaveData();
    ~CSaveData();

    static BOOLEAN              AddSaveBackend
    (
        PCSaveBackend           SaveBackend
    );

    static BOOLEAN              RemoveAllSaveBackend
    (
        void
    );

    NTSTATUS                    SetState
    (
        _In_  KSSTATE           NewState
    );

    static void                 SetVolume
    (
        _In_  ULONG             Index,
        _In_  LONG              Channel,
        _In_  LONG              Value
    );

    static LONG                 GetVolume
    (
        _In_  ULONG             Index,
        _In_  LONG              Channel
    );

    static void                 SetMute
    (
        _In_  ULONG             Index,
        _In_  BOOL              Value
    );

    static BOOL                GetMute
    (
        _In_  ULONG             Index
    );

    static void                 DestroyWorkItems
    (
        void
    );
    void                        Disable
    (
        BOOL                    fDisable
    );
    static PSAVEWORKER_PARAM    GetNewWorkItem
    (
        void
    );
    NTSTATUS                    Initialize
    (
        void
    );
	static NTSTATUS SetDeviceObject
	(
	    IN  PDEVICE_OBJECT          DeviceObject
	);
	static PDEVICE_OBJECT GetDeviceObject
	(
	    void
	);
    void                        ReadData
    (
        _Inout_updates_bytes_all_(ulByteCount)  PBYTE   pBuffer,
        _In_                                    ULONG   ulByteCount
    );
    NTSTATUS                    SetDataFormat
    (
        IN  PKSDATAFORMAT       pDataFormat
    );
    void                        WaitAllWorkItems
    (
        void
    );
    void                        WriteData
    (
        _In_reads_bytes_(ulByteCount)   PBYTE   pBuffer,
        _In_                            ULONG   ulByteCount
    );
    void                       SendData();

private:
    static NTSTATUS             InitializeWorkItems
    (
        IN  PDEVICE_OBJECT      DeviceObject
    );

    NTSTATUS                    FileClose
    (
        void
    );
    NTSTATUS                    FileOpen
    (
        IN  BOOL                fOverWrite
    );
    NTSTATUS                    FileWrite
    (
        _In_reads_bytes_(ulDataSize)    PBYTE   pData,
        _In_                            ULONG   ulDataSize
    );
    NTSTATUS                    FileWriteHeader
    (
        void
    );

    void                        SaveFrame
    (
        IN  ULONG               ulFrameNo,
        IN  ULONG               ulDataSize
    );

    UINT32                      BufferAvailSize();
    UINT32                      BufferFreeSize();

    friend VOID                 SaveFrameWorkerCallback
    (
     PDEVICE_OBJECT pDeviceObject,
     IN  PVOID  Context
    );


    friend int consume_rbuf_by_vio(uint8_t* buf, size_t len, void *other);
};
typedef CSaveData *PCSaveData;

#endif

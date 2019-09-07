#ifndef _MSVAD_VIO_SAVEDATA_H
#define _MSVAD_VIO_SAVEDATA_H

#include "savebackend.h"

//-----------------------------------------------------------------------------
//  Forward declaration
//-----------------------------------------------------------------------------
class CVioSaveData;
typedef CVioSaveData *PCVioSaveData;

class CVioSaveData : public CSaveBackend {
protected:
    PDEVICE_OBJECT              m_DevObj;
public:
    CVioSaveData();
    ~CVioSaveData();

    virtual NTSTATUS            Initialize
    (
        ULONG StreamId
    );
    virtual void                Disable
    (
        BOOL                    fDisable
    );
    virtual NTSTATUS            SetDataFormat
    (
        IN  PWAVEFORMATEX       pWaveFormat
    );
    virtual NTSTATUS            SetState
    (
        _In_  KSSTATE           NewState
    );
    virtual void                SetVolume
    (
        _In_  LONG              Channel,
        _In_  LONG              Value
    );
    virtual void                SetMute
    (
        _In_  BOOL              Value
    );
    virtual BOOL                GetMute
    (
    );

    virtual NTSTATUS            FileClose
    (
        void
    );
    virtual NTSTATUS            FileOpen
    (
        IN  BOOL                fOverWrite
    );
    virtual NTSTATUS            FileWrite
    (
        _In_reads_bytes_(ulDataSize)    PBYTE   pData,
        _In_                            ULONG   ulDataSize
    );
    virtual NTSTATUS            FileWriteHeader
    (
        void
    );

protected:
    DWORD                       IoCtrlTo(
        DWORD CtrlCode, PVOID InBuffer, DWORD InSize
    );
    DWORD                       IoCtrlFrom(
        DWORD CtrlCode, PVOID OutBuffer, DWORD OutSize
    );
};

#endif
#include "stdafx.h"
#include "windows.h"
#include "commctrl.h"
#include "FixList.h"

#include <winioctl.h>

// HEAPHeap
PHEAPBLOCK currentBlock = nullptr;

enum { CLUSTERSPERREAD = 1024 };

template <class T1, class T2> inline
T1* Padd(T1* p, T2 n) { return (T1*)((char *)p + n); }


ULONGLONG AttributeLength(PATTRIBUTE attr) 
{ 
    return attr->Nonresident
        ? PNONRESIDENT_ATTRIBUTE(attr)->DataSize
        : PRESIDENT_ATTRIBUTE(attr)->ValueLength;
}

ULONGLONG AttributeLengthAllocated(PATTRIBUTE attr)
{
    return attr->Nonresident ? PNONRESIDENT_ATTRIBUTE(attr)->AllocatedSize : PRESIDENT_ATTRIBUTE(attr)->ValueLength;
}

VOID ReadSector(PDISKHANDLE disk, ULONGLONG sector, ULONG count, PVOID buffer)
{
    ULARGE_INTEGER offset;
    OVERLAPPED overlap = { 0 };
    ULONG n;

    offset.QuadPart = sector * disk->NTFS.bootSector.BytesPerSector;
    overlap.Offset = offset.LowPart; 
    overlap.OffsetHigh = offset.HighPart;

    ReadFile(disk->fileHandle, buffer, count * disk->NTFS.bootSector.BytesPerSector, &n, &overlap);
}

VOID ReadLCN(PDISKHANDLE disk, ULONGLONG lcn, ULONG count, PVOID buffer)
{
    ReadSector(disk, lcn * disk->NTFS.bootSector.SectorsPerCluster,
        count * disk->NTFS.bootSector.SectorsPerCluster, buffer);
}

VOID ReadExternalAttribute(PDISKHANDLE disk, PNONRESIDENT_ATTRIBUTE attr,
    ULONGLONG vcn, ULONG count, PVOID buffer)
{
    ULONGLONG lcn, runcount;
    ULONG readcount, left;
    PUCHAR bytes = PUCHAR(buffer);

    for (left = count; left > 0; left -= readcount) {
        FindRun(attr, vcn, &lcn, &runcount);

        readcount = ULONG(min(runcount, left));

        ULONG n = readcount * disk->NTFS.BytesPerCluster;

        if (lcn == 0)
            memset(bytes, 0, n);
        else
            ReadLCN(disk, lcn, readcount, bytes);

        vcn += readcount;
        bytes += n;
    }
}

void ReadAttribute(PDISKHANDLE disk, PATTRIBUTE attr, PVOID buffer)
{
    if (attr->Nonresident == FALSE) {
        PRESIDENT_ATTRIBUTE rattr = PRESIDENT_ATTRIBUTE(attr);
        memcpy(buffer, Padd(rattr, rattr->ValueOffset), rattr->ValueLength);
    }
    else {
        PNONRESIDENT_ATTRIBUTE nattr = PNONRESIDENT_ATTRIBUTE(attr);
        ReadExternalAttribute(disk, nattr, 0, ULONG(nattr->HighVcn) + 1, buffer);
    }
}


PDISKHANDLE OpenDisk(WCHAR DosDevice)
{
    WCHAR path[8];
    path[0] = L'\\';
    path[1] = L'\\';
    path[2] = L'.';
    path[3] = L'\\';
    path[4] = DosDevice;
    path[5] = L':';
    path[6] = L'\0';
    PDISKHANDLE disk;
    disk = OpenDisk(path);
    if (disk != nullptr)
    {
        disk->DosDevice = DosDevice;
        return disk;
    }
    return nullptr;
}

PDISKHANDLE OpenDisk(LPCTSTR disk)
{
    PDISKHANDLE tmpDisk;
    DWORD read;
    tmpDisk = new DISKHANDLE;
    memset(tmpDisk, 0, sizeof(DISKHANDLE));
    tmpDisk->fileHandle = CreateFile(disk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (tmpDisk->fileHandle != INVALID_HANDLE_VALUE)
    {
        ReadFile(tmpDisk->fileHandle, &tmpDisk->NTFS.bootSector, sizeof(BOOT_BLOCK), &read, nullptr);
        if (read == sizeof(BOOT_BLOCK))
        {
            if (strncmp("NTFS", reinterpret_cast<const char*>(&tmpDisk->NTFS.bootSector.Format), 4) == 0)
            {
                tmpDisk->type = NTFSDISK;
                tmpDisk->NTFS.BytesPerCluster = tmpDisk->NTFS.bootSector.BytesPerSector * tmpDisk->NTFS.bootSector.SectorsPerCluster;
                tmpDisk->NTFS.BytesPerFileRecord = tmpDisk->NTFS.bootSector.ClustersPerFileRecord < 0x80 ? tmpDisk->NTFS.bootSector.ClustersPerFileRecord * tmpDisk->NTFS.BytesPerCluster : 1 << (0x100 - tmpDisk->NTFS.bootSector.ClustersPerFileRecord);

                tmpDisk->NTFS.complete = FALSE;
                tmpDisk->NTFS.MFTLocation.QuadPart = tmpDisk->NTFS.bootSector.MftStartLcn * tmpDisk->NTFS.BytesPerCluster;
                tmpDisk->NTFS.MFT = nullptr;
                tmpDisk->heapBlock = nullptr;
                tmpDisk->IsLong = 0;
                tmpDisk->NTFS.sizeMFT = 0;
            }
            else
            {
                tmpDisk->type = UNKNOWN;
                tmpDisk->fFiles = nullptr;
            }
        }
        return tmpDisk;
    }

    delete tmpDisk;
    return nullptr;
};

BOOL CloseDisk(PDISKHANDLE disk)
{
    if (disk != nullptr)
    {
        if (disk->fileHandle > INVALID_HANDLE_VALUE) {
            CloseHandle(disk->fileHandle);
        }
        if (disk->type == NTFSDISK)
        {
            {
                delete disk->NTFS.MFT;
            }
            disk->NTFS.MFT = nullptr;
            {
                delete disk->NTFS.Bitmap;
            }
            disk->NTFS.Bitmap = nullptr;
        }
        if (disk->heapBlock != nullptr)
        {
            FreeHeap(disk->heapBlock);
            disk->heapBlock = nullptr;
        }

        delete[] disk->fFiles;

        delete disk;
        return TRUE;
    }
    return FALSE;
};


ULONGLONG LoadMFT(PDISKHANDLE disk, BOOL complete)
{
    DWORD read;
    ULARGE_INTEGER offset;
    UCHAR *buf;

    if (disk == nullptr) {
        return 0;
    }

    if (disk->type == NTFSDISK)
    {
        offset = disk->NTFS.MFTLocation;

        SetFilePointer(disk->fileHandle, offset.LowPart, reinterpret_cast<PLONG>(&offset.HighPart), FILE_BEGIN);
        buf = new UCHAR[disk->NTFS.BytesPerCluster];
        ReadFile(disk->fileHandle, buf, disk->NTFS.BytesPerCluster, &read, nullptr);

        auto file = reinterpret_cast<PFILE_RECORD_HEADER>(buf);

        FixFileRecord(file);

        PNONRESIDENT_ATTRIBUTE nattr(nullptr);
        if (file->Ntfs.Type == 'ELIF')
        {
            //PFILENAME_ATTRIBUTE fn;
            // ???
            //PLONGFILEINFO data = (PLONGFILEINFO) buf;
            PNONRESIDENT_ATTRIBUTE nattr2(nullptr);
            auto attr = reinterpret_cast<PATTRIBUTE>(reinterpret_cast<PUCHAR>(file) + file->AttributesOffset);
            int stop = min(8, file->NextAttributeNumber);

            for (int i = 0; i < stop; i++)
            {
                if (attr->AttributeType < 0 || attr->AttributeType>0x100) {
                    break;
                }

                switch (attr->AttributeType)
                {
                case AttributeList:
                    // now it gets tricky
                    // we have to rebuild the data attribute

                    // wake down the list to find all runarrays
                    // use ReadAttribute to get the list
                    // I think, the right order is important

                    // find out how to walk down the list !!!!

                    // the only solution for now
                    return 3;
                    break;
                case Data:
                    nattr = reinterpret_cast<PNONRESIDENT_ATTRIBUTE>(attr);
                    break;
                case Bitmap:
                    nattr2 = reinterpret_cast<PNONRESIDENT_ATTRIBUTE>(attr);
                default:
                    break;
                };


                if (attr->Length > 0 && attr->Length < file->BytesInUse) {
                    attr = PATTRIBUTE(PUCHAR(attr) + attr->Length);
                }
                else
                    if (attr->Nonresident == TRUE) {
                        attr = PATTRIBUTE(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));
                    }

            }
            if (nattr == nullptr) {
                return 0;
            }
            if (nattr2 == nullptr) {
                return 0;
            }
        }
        disk->NTFS.sizeMFT = static_cast<DWORD>(nattr->DataSize);
        disk->NTFS.MFT = buf;

        disk->NTFS.entryCount = disk->NTFS.sizeMFT / disk->NTFS.BytesPerFileRecord;
        return nattr->DataSize;
    }
    return 0;
};

PATTRIBUTE FindAttribute(PFILE_RECORD_HEADER file, ATTRIBUTE_TYPE type)
{
    auto attr = reinterpret_cast<PATTRIBUTE>(reinterpret_cast<PUCHAR>(file) + file->AttributesOffset);

    for (int i = 1; i < file->NextAttributeNumber; i++)
    {
        if (attr->AttributeType == type) {
            return attr;
        }

        if (attr->AttributeType < 1 || attr->AttributeType>0x100) {
            break;
        }
        if (attr->Length > 0 && attr->Length < file->BytesInUse) {
            attr = PATTRIBUTE(PUCHAR(attr) + attr->Length);
        }
        else
            if (attr->Nonresident == TRUE) {
                attr = PATTRIBUTE(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));
            }
    }
    return nullptr;
}

DWORD ParseMFT(PDISKHANDLE disk, UINT option, PSTATUSINFO info)
{
    if (disk == nullptr) {
        return 0;
    }

    if (disk->type == NTFSDISK)
    {

        CreateFixList();

        auto fh = PFILE_RECORD_HEADER(disk->NTFS.MFT);
        FixFileRecord(fh);

        disk->IsLong = 1;//sizeof(SEARCHFILEINFO);

        if (disk->heapBlock == nullptr) {
            disk->heapBlock = CreateHeap(0x100000);
        }
        auto nattr = reinterpret_cast<PNONRESIDENT_ATTRIBUTE>(FindAttribute(fh, Data));
        if (nattr != nullptr)
        {
            auto buffer = new UCHAR[CLUSTERSPERREAD*disk->NTFS.BytesPerCluster];
            ReadMFTParse(disk, nattr, 0, ULONG(nattr->HighVcn) + 1, buffer, nullptr, info);
            delete[] buffer;
        }

        ProcessFixList(disk);
    }

    return 0;
}

DWORD ReadMFTParse(PDISKHANDLE disk, PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, ULONG count, PVOID buffer, FETCHPROC fetch, PSTATUSINFO info)
{
    ULONGLONG lcn, runcount;
    ULONG readcount, left;
    DWORD ret = 0;
    auto bytes = PUCHAR(buffer);
    //PUCHAR data;

    int x = (disk->NTFS.entryCount + 16);//*sizeof(SEARCHFILEINFO);
    //data = new UCHAR[x];
    //memset(data, 0, x);
    //disk->fFiles = (PSEARCHFILEINFO)data;
    disk->fFiles = new SEARCHFILEINFO[x];
    memset(disk->fFiles, 0, x * sizeof(SEARCHFILEINFO));

    for (left = count; left > 0; left -= readcount)
    {
        FindRun(attr, vcn, &lcn, &runcount);
        readcount = ULONG(min(runcount, left));
        ULONG n = readcount * disk->NTFS.BytesPerCluster;
        if (lcn == 0)
        {
            // spares file?
            memset(bytes, 0, n);
        }
        else
        {
            ret += ReadMFTLCN(disk, lcn, readcount, buffer, fetch, info);
        }
        vcn += readcount;
        bytes += n;
    }
    return ret;
}

ULONG RunLength(const PUCHAR run)
{
    // i guess it must be this way
    return (*run & 0xf) + ((*run >> 4) & 0xf) + 1;
}

LONGLONG RunLCN(const PUCHAR run)
{
    UCHAR n1 = *run & 0xf;
    UCHAR n2 = (*run >> 4) & 0xf;
    LONGLONG lcn = n2 == 0 ? 0 : CHAR(run[n1 + n2]);

    for (LONG i = n1 + n2 - 1; i > n1; i--) {
        lcn = (lcn << 8) + run[i];
    }
    return lcn;
}

ULONGLONG RunCount(const PUCHAR run)
{
    // count the runs we have to process
    UCHAR k = *run & 0xf;
    ULONGLONG count = 0;

    for (ULONG i = k; i > 0; i--) {
        count = (count << 8) + run[i];
    }

    return count;
}

BOOL FindRun(PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count)
{
    if (vcn < attr->LowVcn || vcn > attr->HighVcn) {
        return FALSE;
    }
    *lcn = 0;

    ULONGLONG base = attr->LowVcn;

    for (auto run = PUCHAR(PUCHAR(attr) + attr->RunArrayOffset); *run != 0; run += RunLength(run))
    {
        *lcn += RunLCN(run);
        *count = RunCount(run);
        if (base <= vcn && vcn < base + *count)
        {
            *lcn = RunLCN(run) == 0 ? 0 : *lcn + vcn - base;
            *count -= ULONG(vcn - base);
            return TRUE;
        }


        base += *count;


    }

    return FALSE;
}

DWORD ReadMFTLCN(PDISKHANDLE disk, ULONGLONG lcn, ULONG count, PVOID buffer, FETCHPROC fetch, PSTATUSINFO info)
{
    LARGE_INTEGER offset;
    DWORD read = 0;
    //DWORD ret=0;
    DWORD cnt = 0, c = 0, pos = 0;

    offset.QuadPart = lcn*disk->NTFS.BytesPerCluster;
    //SetFilePointer(disk->fileHandle, offset.LowPart, &offset.HighPart, FILE_BEGIN);

    OVERLAPPED overlap = { 0 };
    overlap.Offset = offset.LowPart;
    overlap.OffsetHigh = offset.HighPart;


    cnt = count / CLUSTERSPERREAD;

    for (int i = 1; i <= cnt; i++)
    {

        ReadFile(disk->fileHandle, buffer, CLUSTERSPERREAD*disk->NTFS.BytesPerCluster, &read, &overlap);
        c += CLUSTERSPERREAD;
        pos += read;

        offset.QuadPart += read;
        overlap.Offset = offset.LowPart;
        overlap.OffsetHigh = offset.HighPart;

        ProcessBuffer(disk, static_cast<PUCHAR>(buffer), read, fetch);
        CallMe(info, disk->filesSize);

    }

    ReadFile(disk->fileHandle, buffer, (count - c)*disk->NTFS.BytesPerCluster, &read, &overlap);
    ProcessBuffer(disk, static_cast<PUCHAR>(buffer), read, fetch);
    CallMe(info, disk->filesSize);

    pos += read;
    return pos;
}

DWORD inline ProcessBuffer(PDISKHANDLE disk, PUCHAR buffer, DWORD size, FETCHPROC fetch)
{
    auto end = PUCHAR(buffer) + size;
    SEARCHFILEINFO* data = disk->fFiles + disk->filesSize;

    while (buffer < end)
    {
        auto fh = PFILE_RECORD_HEADER(buffer);
        FixFileRecord(fh);
        if (FetchSearchInfo(disk, fh, data)) {
            disk->realFiles++;
        }
        buffer += disk->NTFS.BytesPerFileRecord;
        //data += sizeof(SEARCHFILEINFO);
        ++data;
        disk->filesSize++;
    }
    return 0;
}

LPWSTR GetPath(PDISKHANDLE disk, int id)
{
    int a = id;
    //int i;
    DWORD pt;
    //PUCHAR ptr = (PUCHAR)disk->sFiles;
    DWORD PathStack[64];
    int PathStackPos = 0;
    static WCHAR glPath[0xffff];
    int CurrentPos = 0;

    PathStackPos = 0;
    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;
        pt = a*disk->IsLong;
        //a = PSEARCHFILEINFO(ptr+pt)->ParentId.LowPart;
        a = disk->fFiles[pt].ParentId.LowPart;

        if (a == 0 || a == 5) {
            break;
        }
    }
    if (disk->DosDevice != NULL)
    {
        glPath[0] = disk->DosDevice;
        glPath[1] = L':';
        CurrentPos = 2;
    }
    else {
        glPath[0] = L'\0';
    }
    for (int i = PathStackPos - 1; i > 0; i--)
    {
        pt = PathStack[i] * disk->IsLong;
        glPath[CurrentPos++] = L'\\';
        //memcpy(&glPath[CurrentPos], PSEARCHFILEINFO(ptr+pt)->FileName, PSEARCHFILEINFO(ptr+pt)->FileNameLength*2);
        //CurrentPos+=PSEARCHFILEINFO(ptr+pt)->FileNameLength;
        memcpy(&glPath[CurrentPos], disk->fFiles[pt].FileName, disk->fFiles[pt].FileNameLength * 2);
        CurrentPos += disk->fFiles[pt].FileNameLength;
    }
    glPath[CurrentPos] = L'\\';
    glPath[CurrentPos + 1] = L'\0';
    return glPath;
}

LPWSTR GetCompletePath(PDISKHANDLE disk, int id)
{
    int a = id;
    //int i;
    DWORD pt;
    //PUCHAR ptr = (PUCHAR)disk->sFiles;
    DWORD PathStack[64];
    int PathStackPos = 0;
    static WCHAR glPath[0xffff];
    int CurrentPos = 0;

    PathStackPos = 0;
    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;
        pt = a*disk->IsLong;
        //a = PSEARCHFILEINFO(ptr+pt)->ParentId.LowPart;
        a = disk->fFiles[pt].ParentId.LowPart;

        if (a == 0 || a == 5) {
            break;
        }
    }
    if (disk->DosDevice != NULL)
    {
        glPath[0] = disk->DosDevice;
        glPath[1] = L':';
        CurrentPos = 2;
    }
    else {
        glPath[0] = L'\0';
    }
    for (int i = PathStackPos - 1; i >= 0; i--)
    {
        pt = PathStack[i] * disk->IsLong;
        glPath[CurrentPos++] = L'\\';
        //memcpy(&glPath[CurrentPos], PSEARCHFILEINFO(ptr+pt)->FileName, PSEARCHFILEINFO(ptr+pt)->FileNameLength*2);
        //CurrentPos+=PSEARCHFILEINFO(ptr+pt)->FileNameLength;
        memcpy(&glPath[CurrentPos], disk->fFiles[pt].FileName, disk->fFiles[pt].FileNameLength * 2);
        CurrentPos += disk->fFiles[pt].FileNameLength;
    }
    glPath[CurrentPos] = L'\0';
    return glPath;
}

VOID inline CallMe(PSTATUSINFO info, DWORD value)
{
    if (info != nullptr) {
        SendMessage(info->hWnd, PBM_SETPOS, value, 0);
    }

}


BOOL inline FetchSearchInfo(PDISKHANDLE disk, PFILE_RECORD_HEADER file, SEARCHFILEINFO* data)
{
    PFILENAME_ATTRIBUTE fn;
    auto attr = reinterpret_cast<PATTRIBUTE>(reinterpret_cast<PUCHAR>(file) + file->AttributesOffset);
    int stop = min(8, file->NextAttributeNumber);

    bool fileNameFound = false;
    bool fileSizeFound = false;
    bool dataFound = false;

    if (file->Ntfs.Type == 'ELIF')
    {
        data->Flags = file->Flags;

        for (int i = 0; i < stop; i++)
        {
            if (attr->AttributeType < 0 || attr->AttributeType>0x100) {
                break;
            }

            switch (attr->AttributeType)
            {
            case FileName:
                fn = PFILENAME_ATTRIBUTE(PUCHAR(attr) + PRESIDENT_ATTRIBUTE(attr)->ValueOffset);
                if (fn->DataSize || fn->AllocatedSize)
                {
                    data->DataSize = fn->DataSize;
                    data->AllocatedSize = fn->AllocatedSize;
                }
                if (((fn->NameType & WIN32_NAME) != 0) || fn->NameType == 0)
                {
                    if (!memcmp(fn->Name, L"InstallationLog.txt", 38))
                    {
                        DebugBreak();
                    }
                    //fn->Name[fn->NameLength] = L'\0';
                    data->FileName = AllocAndCopyString(disk->heapBlock, fn->Name, fn->NameLength);
                    data->FileNameLength = min(fn->NameLength, wcslen(data->FileName));
                    data->ParentId.QuadPart = fn->DirectoryFileReferenceNumber;
                    data->ParentId.HighPart &= 0x0000ffff;

                    if (file->BaseFileRecord.LowPart != 0)// && file->BaseFileRecord.HighPart !=0x10000)
                    {
                        AddToFixList(file->BaseFileRecord.LowPart, disk->filesSize);
                    }

                    fileNameFound = true;

                    if (dataFound && fileSizeFound) {
                        return TRUE;
                    }
                }
                break;
            case AttributeList:
                {
                    //break;

                    UCHAR attrbuf[64 * 1024];//4096];
                    ReadAttribute(disk, attr, &attrbuf[0]);


                    PATTRIBUTE_LIST attrlist = (PATTRIBUTE_LIST)&attrbuf[0];
                    while (attrlist->Attribute != -1)
                    {
                        //if (attrlist->Attribute == FileName)
                        //    return (DWORD)attrlist->FileReferenceNumber;
                        //else if (attrlist->Attribute == AttributeList)
                        //{
                        //    UCHAR buf[4096];

                        //}
                        if (attrlist->Attribute == Data)
                        {
                            //DebugBreak();

                            //*

                            NTFS_FILE_RECORD_INPUT_BUFFER mftRecordInput{};
                            mftRecordInput.FileReferenceNumber.QuadPart = 0xffffffffffff & attrlist->FileReferenceNumber;
                            DWORD dwRet = 0;
                            enum { dwFileRecSize = 64 * 1024 };
                            char buf[dwFileRecSize];
                            PNTFS_FILE_RECORD_OUTPUT_BUFFER pMftRecord = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)buf;
                            PFILE_RECORD_HEADER pfileRecordheader = (PFILE_RECORD_HEADER)pMftRecord->FileRecordBuffer;
                            //NTFS_FILE_RECORD_OUTPUT_BUFFER
                            if (DeviceIoControl(disk->fileHandle, FSCTL_GET_NTFS_FILE_RECORD
                                , &mftRecordInput, sizeof(mftRecordInput)
                                , pMftRecord, dwFileRecSize, &dwRet, NULL))
                            {
                                volatile auto pAttribute = (PATTRIBUTE)((PBYTE)pfileRecordheader + pfileRecordheader->AttributesOffset);
                                while (pAttribute->AttributeType != -1)
                                {
                                    if (pAttribute->AttributeType == Data && pAttribute->Nonresident)
                                    {
                                        //data->DataSize = max(data->DataSize, AttributeLength(pAttribute));
                                        //data->AllocatedSize = max(data->AllocatedSize, AttributeLengthAllocated(pAttribute));
                                        data->DataSize += AttributeLength(pAttribute);
                                        data->AllocatedSize += AttributeLengthAllocated(pAttribute);
                                        //break;
                                    }
                                    //if (pAttribute->Nonresident) {
                                    //    *pOutSize = PNONRESIDENT_ATTRIBUTE(pAttribute)->DataSize;
                                    //}
                                    //else {
                                    //    *pOutSize = PRESIDENT_ATTRIBUTE(pAttribute)->ValueLength;
                                    //}
                                    if (!pAttribute->Length)
                                        break;
                                    pAttribute = (PATTRIBUTE)(PUCHAR(pAttribute) + pAttribute->Length);
                                }
                            }

                            //*/
                        }
                        if (!attrlist->Length)
                            break;
                        attrlist = (PATTRIBUTE_LIST)(PUCHAR(attrlist) + attrlist->Length);
                    }
                }
                break;
            case Data:
                if (!attr->Nonresident && PRESIDENT_ATTRIBUTE(attr)->ValueLength > 0)
                {
                    memcpy(data->data,
                        PUCHAR(attr) + PRESIDENT_ATTRIBUTE(attr)->ValueOffset,
                        min(sizeof(data->data), PRESIDENT_ATTRIBUTE(attr)->ValueLength));

                    if (fileNameFound && fileSizeFound) {
                        return TRUE;
                    }
                }
            case ZeroValue: // falls through
                if (AttributeLength(attr) > 0 || AttributeLengthAllocated(attr) > 0)
                {
                    data->DataSize = max(data->DataSize, AttributeLength(attr));
                    data->AllocatedSize = max(data->AllocatedSize, AttributeLengthAllocated(attr));
                    if (fileNameFound && dataFound) {
                        return TRUE;
                    }
                }
            break;
            default:
                break;
            };


            if (attr->Length > 0 && attr->Length < file->BytesInUse) {
                attr = PATTRIBUTE(PUCHAR(attr) + attr->Length);
            }
            else
                if (attr->Nonresident == TRUE) {
                    attr = PATTRIBUTE(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));
                }

        }
    }
    return fileNameFound;
}



BOOL FixFileRecord(PFILE_RECORD_HEADER file)
{
    //int sec = 2048;
    auto usa = PUSHORT(PUCHAR(file) + file->Ntfs.UsaOffset);
    auto sector = PUSHORT(file);

    if (file->Ntfs.UsaCount > 4) {
        return FALSE;
    }
    for (ULONG i = 1; i < file->Ntfs.UsaCount; i++)
    {
        sector[255] = usa[i];
        sector += 256;
    }

    return TRUE;
}

BOOL ReparseDisk(PDISKHANDLE disk, UINT option, PSTATUSINFO info)
{
    if (disk != nullptr)
    {
        if (disk->type == NTFSDISK)
        {
            {
                delete disk->NTFS.MFT;
            }
            disk->NTFS.MFT = nullptr;
            {
                delete disk->NTFS.Bitmap;
            }
            disk->NTFS.Bitmap = nullptr;
        }
        if (disk->heapBlock != nullptr)
        {
            ReUseBlocks(disk->heapBlock, FALSE);
        }
        delete[] disk->fFiles;
        disk->fFiles = nullptr;

        disk->filesSize = 0;
        disk->realFiles = 0;

        if (LoadMFT(disk, FALSE) != 0) {
            ParseMFT(disk, option, info);
        }
        return TRUE;
    }
    return FALSE;
};

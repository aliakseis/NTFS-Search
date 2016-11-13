#include "stdafx.h"
#include "windows.h"
#include "commctrl.h"
#include "FixList.h"

// HEAPHeap
PHEAPBLOCK currentBlock=NULL;

//NONRESIDENT_ATTRIBUTE ERROR_ATTRIBUTE = {1,2,3,4,5};
#define CLUSTERSPERREAD 1024
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
	if (disk!=NULL)
	{
		disk->DosDevice = DosDevice;
		return disk;
	}
	return NULL;
}

PDISKHANDLE OpenDisk(LPCTSTR disk)
{
	PDISKHANDLE tmpDisk;
	DWORD read;
	tmpDisk = new DISKHANDLE;
	memset(tmpDisk, 0, sizeof(DISKHANDLE));
	tmpDisk->fileHandle = CreateFile(disk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (tmpDisk->fileHandle != INVALID_HANDLE_VALUE)
	{
		ReadFile(tmpDisk->fileHandle, &tmpDisk->NTFS.bootSector, sizeof(BOOT_BLOCK), &read, NULL);
		if (read==sizeof(BOOT_BLOCK))
		{
			if (strncmp("NTFS",(const char*) &tmpDisk->NTFS.bootSector.Format, 4)==0)
			{
				tmpDisk->type = NTFSDISK;
				tmpDisk->NTFS.BytesPerCluster = tmpDisk->NTFS.bootSector.BytesPerSector * tmpDisk->NTFS.bootSector.SectorsPerCluster;
				tmpDisk->NTFS.BytesPerFileRecord = tmpDisk->NTFS.bootSector.ClustersPerFileRecord < 0x80 ? tmpDisk->NTFS.bootSector.ClustersPerFileRecord * tmpDisk->NTFS.BytesPerCluster: 1 <<(0x100 - tmpDisk->NTFS.bootSector.ClustersPerFileRecord);
				
				tmpDisk->NTFS.complete = FALSE;
				tmpDisk->NTFS.MFTLocation.QuadPart = tmpDisk->NTFS.bootSector.MftStartLcn * tmpDisk->NTFS.BytesPerCluster;
				tmpDisk->NTFS.MFT = NULL;
				tmpDisk->heapBlock = NULL;
				tmpDisk->IsLong = 0;
				tmpDisk->NTFS.sizeMFT = 0;
			}
			else
			{
				tmpDisk->type = UNKNOWN;
				tmpDisk->fFiles = NULL;
			}
		}
		return tmpDisk;
	}

	delete tmpDisk;
	return NULL;
};

BOOL CloseDisk(PDISKHANDLE disk)
{
	if (disk!=NULL)
	{
		if (disk->fileHandle>INVALID_HANDLE_VALUE) CloseHandle(disk->fileHandle);
		if (disk->type == NTFSDISK)
		{
			if (disk->NTFS.MFT!=NULL)
				delete disk->NTFS.MFT;
			disk->NTFS.MFT = NULL;
			if (disk->NTFS.Bitmap !=NULL)
				delete disk->NTFS.Bitmap;
			disk->NTFS.Bitmap = NULL;
		}
		if (disk->heapBlock!=NULL)
		{
			FreeHeap(disk->heapBlock);
			disk->heapBlock = NULL;
		}
        /*
		if (disk->IsLong)
		{
			if (disk->lFiles!=NULL)
				delete disk->lFiles;
			disk->lFiles = NULL;
		}
		else
		{
			if (disk->sFiles!=NULL)
				delete disk->sFiles;
			disk->sFiles = NULL;
		}
        */

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
	PFILE_RECORD_HEADER file;
	PNONRESIDENT_ATTRIBUTE nattr(NULL), nattr2(NULL);

	if (disk==NULL)
		return 0;

	if (disk->type==NTFSDISK)
	{
		offset = disk->NTFS.MFTLocation;

		SetFilePointer(disk->fileHandle, offset.LowPart,(PLONG) &offset.HighPart, FILE_BEGIN);
		buf = new UCHAR[disk->NTFS.BytesPerCluster];
		ReadFile(disk->fileHandle, buf, disk->NTFS.BytesPerCluster, &read, NULL);

		file = (PFILE_RECORD_HEADER)(buf);
		
		FixFileRecord(file);

		if (file->Ntfs.Type == 'ELIF')
		{
			//PFILENAME_ATTRIBUTE fn;
            // ???
			//PLONGFILEINFO data = (PLONGFILEINFO) buf;
			PATTRIBUTE attr = (PATTRIBUTE)((PUCHAR)(file) + file->AttributesOffset);
			int stop = min(8,file->NextAttributeNumber);
	
			//data->Flags = file->Flags;
	
			for (int i=0;i<stop;i++)
			{
				if (attr->AttributeType<0 || attr->AttributeType>0x100) break;
				
				switch(attr->AttributeType)
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
						nattr = (PNONRESIDENT_ATTRIBUTE) attr;
						break;
					case Bitmap:
						nattr2 = (PNONRESIDENT_ATTRIBUTE) attr;
					default:
						break;
				};


				if (attr->Length>0 && attr->Length < file->BytesInUse)
					attr = PATTRIBUTE(PUCHAR(attr) + attr->Length);
				else
					if (attr->Nonresident == TRUE)
						attr = PATTRIBUTE(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));	
				
			}
			if (nattr==NULL)
				return 0;
			if (nattr2==NULL)
				return 0;
		}
		disk->NTFS.sizeMFT = (DWORD) nattr->DataSize;
		disk->NTFS.MFT = buf;

		disk->NTFS.entryCount = disk->NTFS.sizeMFT / disk->NTFS.BytesPerFileRecord;
		return nattr->DataSize;
	}
	return 0;
};

PATTRIBUTE FindAttribute(PFILE_RECORD_HEADER file, ATTRIBUTE_TYPE type)
{
	PATTRIBUTE attr = (PATTRIBUTE)((PUCHAR)(file) + file->AttributesOffset);

	for (int i=1;i<file->NextAttributeNumber;i++)
	{
		if (attr->AttributeType==type)
			return attr;
		
		if (attr->AttributeType<1 || attr->AttributeType>0x100) break;
		if (attr->Length>0 && attr->Length < file->BytesInUse)
			attr = PATTRIBUTE(PUCHAR(attr) + attr->Length);
		else
			if (attr->Nonresident == TRUE)
				attr = PATTRIBUTE(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));	
	}
	return NULL;
}

DWORD ParseMFT(PDISKHANDLE disk,UINT option, PSTATUSINFO info)
{
	PUCHAR buffer;
	PFILE_RECORD_HEADER fh;
	//PRESIDENT_ATTRIBUTE attr;
	PNONRESIDENT_ATTRIBUTE nattr;
	//LONGFILEINFO *data;
	//DWORD index=0;

	//FETCHPROC fetch;
	//fetch = FetchFileInfo;

	if (disk==NULL)
		return 0;

	if (disk->type==NTFSDISK)
	{

		CreateFixList();

		fh = PFILE_RECORD_HEADER(disk->NTFS.MFT);
		FixFileRecord(fh);

		disk->IsLong = 1;//sizeof(SEARCHFILEINFO);

		if (disk->heapBlock==NULL)
			disk->heapBlock = CreateHeap(0x100000);
		nattr = (PNONRESIDENT_ATTRIBUTE) FindAttribute(fh, Data); 
		if (nattr!=NULL)
		{
			buffer = new UCHAR[CLUSTERSPERREAD*disk->NTFS.BytesPerCluster];
			ReadMFTParse(disk, nattr, 0, ULONG(nattr->HighVcn)+1, buffer, NULL, info);
			delete buffer;
		}	

		ProcessFixList(disk);
	}

	return 0;
}

DWORD ReadMFTParse(PDISKHANDLE disk, PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, ULONG count, PVOID buffer, FETCHPROC fetch, PSTATUSINFO info)
{
	ULONGLONG lcn, runcount;
	ULONG readcount, left;
	DWORD ret=0;
	PUCHAR bytes=PUCHAR(buffer);
	//PUCHAR data;

	int x = (disk->NTFS.entryCount+16);//*sizeof(SEARCHFILEINFO);
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
		if (lcn==0)
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

ULONG RunLength(PUCHAR run)
{
	// i guess it must be this way
	return (*run & 0xf) + ((*run >> 4) & 0xf)+1;
}

LONGLONG RunLCN(PUCHAR run)
{
	UCHAR n1 = *run & 0xf;
	UCHAR n2 = (*run >> 4) & 0xf;
	LONGLONG lcn = n2 == 0 ? 0 : CHAR(run[n1+n2]);

	for (LONG i = n1 + n2 - 1;i>n1; i--)
		lcn = (lcn << 8) + run[i];
	return lcn;
}

ULONGLONG RunCount(PUCHAR run)
{
	// count the runs we have to process
	UCHAR k = *run & 0xf;
	ULONGLONG count = 0;

	for (ULONG i=k;i > 0; i--)
		count = (count << 8) + run[i];

	return count;
}

BOOL FindRun(PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count)
{
	if (vcn < attr->LowVcn || vcn > attr->HighVcn)
		return FALSE;
	*lcn = 0;
	
	ULONGLONG base=attr->LowVcn;

	for (PUCHAR run = PUCHAR(PUCHAR(attr) +  attr->RunArrayOffset);*run!=0;run+=RunLength(run))
	{
		*lcn += RunLCN(run);
		*count = RunCount(run);
		if (base <= vcn && vcn < base + *count)
		{
			*lcn = RunLCN(run) == 0 ? 0 : *lcn + vcn - base;
			*count -= ULONG(vcn - base);
			return TRUE;
		}
		else
		{
			base += *count;
		}

	}

	return FALSE;
}

DWORD ReadMFTLCN(PDISKHANDLE disk, ULONGLONG lcn, ULONG count, PVOID buffer, FETCHPROC fetch, PSTATUSINFO info)
{
	LARGE_INTEGER offset;
	DWORD read=0;
	//DWORD ret=0;
	DWORD cnt=0, c=0, pos=0;
	
	offset.QuadPart = lcn*disk->NTFS.BytesPerCluster;
	SetFilePointer(disk->fileHandle, offset.LowPart, &offset.HighPart, FILE_BEGIN);

	cnt = count / CLUSTERSPERREAD;

	for (int i=1;i<=cnt;i++)
	{

		ReadFile(disk->fileHandle, buffer, CLUSTERSPERREAD*disk->NTFS.BytesPerCluster, &read, NULL);
		c += CLUSTERSPERREAD;
		pos+=read;

		ProcessBuffer(disk, (PUCHAR)buffer, read, fetch );
		CallMe(info, disk->filesSize);

	}

	ReadFile(disk->fileHandle, buffer, (count-c)*disk->NTFS.BytesPerCluster, &read, NULL);
	ProcessBuffer(disk, (PUCHAR)buffer, read, fetch );
	CallMe(info, disk->filesSize);
	
	pos+=read;
	return pos;
}

DWORD inline ProcessBuffer(PDISKHANDLE disk, PUCHAR buffer, DWORD size, FETCHPROC fetch)
{
	PUCHAR end;
	//PUCHAR data;
	//DWORD count=0;
	PFILE_RECORD_HEADER fh;
	end	= PUCHAR(buffer) + size;
	//data = PUCHAR(disk->fFiles);
	//data += sizeof(SEARCHFILEINFO) * disk->filesSize;
    SEARCHFILEINFO* data = disk->fFiles + disk->filesSize;

	while (buffer<end)
	{
		fh = PFILE_RECORD_HEADER(buffer);
		FixFileRecord(fh);
		if (FetchSearchInfo(disk, fh, data))
			disk->realFiles++;
		buffer+= disk->NTFS.BytesPerFileRecord;
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
	int PathStackPos=0;
	static WCHAR glPath[0xffff];
	int CurrentPos=0;
	
	PathStackPos = 0;
	for (int i=0;i<64;i++)
	{
		PathStack[PathStackPos++] = a;
		pt = a*disk->IsLong;
		//a = PSEARCHFILEINFO(ptr+pt)->ParentId.LowPart;
        a = disk->fFiles[pt].ParentId.LowPart;
		
		if (a==0 || a==5) break; 
	}
	if (disk->DosDevice !=NULL)
	{
		glPath[0] = disk->DosDevice;
		glPath[1] = L':';
		CurrentPos = 2;
	}
	else
		glPath[0]=L'\0';
	for (int i=PathStackPos-1;i>0;i--)
	{
		pt = PathStack[i]*disk->IsLong;
		glPath[CurrentPos++] = L'\\';
		//memcpy(&glPath[CurrentPos], PSEARCHFILEINFO(ptr+pt)->FileName, PSEARCHFILEINFO(ptr+pt)->FileNameLength*2);
		//CurrentPos+=PSEARCHFILEINFO(ptr+pt)->FileNameLength;
        memcpy(&glPath[CurrentPos], disk->fFiles[pt].FileName, disk->fFiles[pt].FileNameLength*2);
        CurrentPos+=disk->fFiles[pt].FileNameLength;
	}
	glPath[CurrentPos]=L'\\';
	glPath[CurrentPos+1]=L'\0';
	return glPath;
}

LPWSTR GetCompletePath(PDISKHANDLE disk, int id)
{
	int a = id;
	//int i;
	DWORD pt;
	//PUCHAR ptr = (PUCHAR)disk->sFiles;
	DWORD PathStack[64];
	int PathStackPos=0;
	static WCHAR glPath[0xffff];
	int CurrentPos=0;
	
	PathStackPos = 0;
	for (int i=0;i<64;i++)
	{
		PathStack[PathStackPos++] = a;
		pt = a*disk->IsLong;
		//a = PSEARCHFILEINFO(ptr+pt)->ParentId.LowPart;
        a = disk->fFiles[pt].ParentId.LowPart;
		
		if (a==0 || a==5) break; 
	}
	if (disk->DosDevice !=NULL)
	{
		glPath[0] = disk->DosDevice;
		glPath[1] = L':';
		CurrentPos = 2;
	}
	else
		glPath[0]=L'\0';
	for (int i=PathStackPos-1;i>=0;i--)
	{
		pt = PathStack[i]*disk->IsLong;
		glPath[CurrentPos++] = L'\\';
		//memcpy(&glPath[CurrentPos], PSEARCHFILEINFO(ptr+pt)->FileName, PSEARCHFILEINFO(ptr+pt)->FileNameLength*2);
		//CurrentPos+=PSEARCHFILEINFO(ptr+pt)->FileNameLength;
        memcpy(&glPath[CurrentPos], disk->fFiles[pt].FileName, disk->fFiles[pt].FileNameLength*2);
        CurrentPos+=disk->fFiles[pt].FileNameLength;
	}
	glPath[CurrentPos]=L'\0';
	return glPath;
}

VOID inline CallMe(PSTATUSINFO info, DWORD value)
{
	if (info!=NULL)
		SendMessage(info->hWnd, PBM_SETPOS, value, 0);
	
}


BOOL inline FetchSearchInfo(PDISKHANDLE disk, PFILE_RECORD_HEADER file, SEARCHFILEINFO* data)
{
	//int i;
	
	PFILENAME_ATTRIBUTE fn;
	//PLONGFILEINFO data = (PLONGFILEINFO) buf;
	PATTRIBUTE attr = (PATTRIBUTE)((PUCHAR)(file) + file->AttributesOffset);
	//PATTRIBUTE attrlist = NULL;
	int stop = min(8,file->NextAttributeNumber);

    BOOL fileNameFound = FALSE;
    BOOL dataFound = FALSE;
	
	if (file->Ntfs.Type =='ELIF')
	{
		data->Flags = file->Flags;
	
		for (int i=0;i<stop;i++)
		{
			if (attr->AttributeType<0 || attr->AttributeType>0x100) break;
			
			switch(attr->AttributeType)
			{
				case FileName:
					fn = PFILENAME_ATTRIBUTE(PUCHAR(attr) + PRESIDENT_ATTRIBUTE(attr)->ValueOffset);
					if (fn->NameType & WIN32_NAME || fn->NameType == 0)
					{
						fn->Name[fn->NameLength] = L'\0';
						data->FileName = AllocAndCopyString(disk->heapBlock, fn->Name,fn->NameLength);
						data->FileNameLength = min(fn->NameLength, wcslen(data->FileName));
						data->ParentId.QuadPart = fn->DirectoryFileReferenceNumber;
						data->ParentId.HighPart &= 0x0000ffff;
						if (file->BaseFileRecord.LowPart!=0)// && file->BaseFileRecord.HighPart !=0x10000)
						{
							AddToFixList( file->BaseFileRecord.LowPart, disk->filesSize);
						}

                        if (dataFound)
    						return TRUE;
					}
					break;
                case Data:
                    {
                        if (PRESIDENT_ATTRIBUTE(attr)->ValueLength > 0)
                        {
                            memcpy(data->data, 
                                PUCHAR(attr) + PRESIDENT_ATTRIBUTE(attr)->ValueOffset,
                                min(sizeof(data->data), PRESIDENT_ATTRIBUTE(attr)->ValueLength));

                            if (fileNameFound)
                                return TRUE;
                        }
                    }
                    break;
				default:
					break;
			};


			if (attr->Length>0 && attr->Length < file->BytesInUse)
				attr = PATTRIBUTE(PUCHAR(attr) + attr->Length);
			else
				if (attr->Nonresident == TRUE)
					attr = PATTRIBUTE(PUCHAR(attr) + sizeof(NONRESIDENT_ATTRIBUTE));	
			
		}
	}
	return fileNameFound;
}



BOOL FixFileRecord(PFILE_RECORD_HEADER file)
{
	//int sec = 2048;
	PUSHORT usa = PUSHORT(PUCHAR(file) + file->Ntfs.UsaOffset);
	PUSHORT sector = PUSHORT(file);
	
	if (file->Ntfs.UsaCount>4) 
		return FALSE;
	for (ULONG i=1;i<file->Ntfs.UsaCount;i++)
	{
		sector[255] = usa[i];
		sector+= 256;
	}
	
	return TRUE;
}

BOOL ReparseDisk(PDISKHANDLE disk, UINT option, PSTATUSINFO info)
{
	if (disk!=NULL)
	{
		if (disk->type == NTFSDISK)
		{
			if (disk->NTFS.MFT!=NULL)
				delete disk->NTFS.MFT;
			disk->NTFS.MFT = NULL;
			if (disk->NTFS.Bitmap !=NULL)
				delete disk->NTFS.Bitmap;
			disk->NTFS.Bitmap = NULL;
		}
		if (disk->heapBlock!=NULL)
		{
			ReUseBlocks(disk->heapBlock, FALSE);
		}
		//if (disk->sFiles!=NULL)
		//	delete disk->sFiles;
		//disk->sFiles = NULL;
        delete [] disk->fFiles;
        disk->fFiles = NULL;

		disk->filesSize = 0;
		disk->realFiles = 0;

		if (LoadMFT(disk, FALSE)!=0)
			ParseMFT(disk, option, info);
		return TRUE;
	}
	return FALSE;
};

/*PUCHAR AllocData(PHEAPBLOCK block, DWORD size)
{
	PHEAPBLOCK tmp, back;
	PUCHAR ret=NULL;
	int t;

	tmp = block->end;
	if(tmp!=NULL)
	{
		t = tmp->size - tmp->current;
		if (t > (int)size)
		{
			ret = &tmp->data[tmp->current];
			tmp->current += size;
			return ret;
		}
		back = tmp;
	}

	if (tmp->next !=NULL)
	{
		tmp = tmp->next;
		if(tmp!=NULL)
		{
			t = tmp->size - tmp->current;
			if (t > (int)size)
			{
				ret = &tmp->data[tmp->current];
				tmp->current += size;
				return ret;
			}
			back = tmp;
			//goto here;
		}

	}
	else
	{
here:	tmp = (PHEAPBLOCK) malloc(sizeof(HEAPBLOCK));
		tmp->data =	(PUCHAR) malloc(block->size);
		if (tmp->data!=NULL)
		{	
			tmp->current = size;
			tmp->size = block->size;
			tmp->next = NULL;

			back = block->end;
			tmp->end = block;
			back->next = tmp;
			block->end = tmp;

			return tmp->data;
		}
		else
			free(tmp);
	}
	return NULL;

}
*/


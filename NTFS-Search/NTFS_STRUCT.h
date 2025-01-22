// NTFS Structures

#include "Heap.h"

#include <string>

/* BOOT_BLOCK

*/
#pragma pack(push,1)

typedef struct {
	UCHAR Jump[3];
	UCHAR Format[8];
	USHORT BytesPerSector;
	UCHAR SectorsPerCluster;
	USHORT BootSectors;
	UCHAR Mbz1;
	USHORT Mbz2;
	USHORT Reserved1;
	UCHAR MediaType;
	USHORT Mbz3;
	USHORT SectorsPerTrack;
	USHORT NumberOfHeads;
	ULONG PartitionOffset;
	ULONG Rserved2[2];
	ULONGLONG TotalSectors;
	ULONGLONG MftStartLcn;
	ULONGLONG Mft2StartLcn;
	ULONG ClustersPerFileRecord;
	ULONG ClustersPerIndexBlock;
	ULONGLONG VolumeSerialNumber;
	UCHAR Code[0x1AE];
	USHORT BootSignature;
}BOOT_BLOCK, *PBOOT_BLOCK;

#pragma pack(pop)


/* NTFS_RECORD_HEADER
	type - 'FILE' 'INDX' 'BAAD' 'HOLE' *CHKD'

*/
typedef struct{
	ULONG Type;
	USHORT UsaOffset;
	USHORT UsaCount;
	USN Usn;
}NTFS_RECORD_HEADER, *PNTFS_RECORD_HEADER;

/* FILE_RECORD_HEADER

*/
typedef struct{
	NTFS_RECORD_HEADER Ntfs;
	USHORT SequenceNumber;
	USHORT LinkCount;
	USHORT AttributesOffset;
	USHORT Flags; // 0x0001 InUse; 0x0002 Directory
	ULONG BytesInUse;
	ULONG BytesAllocated;
	ULARGE_INTEGER BaseFileRecord;
	USHORT NextAttributeNumber;
} FILE_RECORD_HEADER, *PFILE_RECORD_HEADER;

/* ATTRIBUTE_TYPE enumeration

*/

typedef enum {
	ZeroValue = 0,
	StandardInformation = 0x10,
	AttributeList = 0x20,
	FileName = 0x30,
	ObjectId = 0x40,
	SecurityDescripter = 0x50,
	VolumeName = 0x60,
	VolumeInformation = 0x70,
	Data = 0x80,
	IndexRoot = 0x90,
	IndexAllocation = 0xA0,
	Bitmap = 0xB0,
	ReparsePoint = 0xC0,
	EAInformation = 0xD0,
	EA = 0xE0,
	PropertySet = 0xF0,
	LoggedUtilityStream = 0x100
} ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

/* ATTRIBUTE Structure

*/
typedef struct{
	ATTRIBUTE_TYPE AttributeType;
	ULONG Length;
	BOOLEAN Nonresident;
	UCHAR NameLength; 
	USHORT NameOffset; // Starts form the Attribute Offset
	USHORT Flags; // 0x001 = Compressed
	USHORT AttributeNumber;
} ATTRIBUTE, *PATTRIBUTE;

/* ATTRIBUTE resident

*/
typedef struct {
	ATTRIBUTE Attribute;
	ULONG ValueLength;
	USHORT ValueOffset; //Starts from the Attribute
	USHORT Flags; //0x0001 Indexed
} RESIDENT_ATTRIBUTE, *PRESIDENT_ATTRIBUTE;

/* ATTRIBUTE nonresident

*/
typedef struct {
	ATTRIBUTE Attribute;
	ULONGLONG LowVcn;
	ULONGLONG HighVcn;
	USHORT RunArrayOffset;
	UCHAR CompressionUnit;
	UCHAR AligmentOrReserved[5];
	ULONGLONG AllocatedSize;
	ULONGLONG DataSize;
	ULONGLONG InitializedSize;
	ULONGLONG CompressedSize; //Only when compressed
} NONRESIDENT_ATTRIBUTE, *PNONRESIDENT_ATTRIBUTE;

/* 
	VolumeName - just a Unicode String
	Data = just data
	SecurityDescriptor - rarely found
	Bitmap - array of bits, which indicate the use of entries
*/

/* STANDARD_INFORMATION
	FILE_ATTRIBUTES_* like in windows.h
	and is always resident
*/
typedef struct {
	FILETIME CreationTime;
	FILETIME ChangeTime;
	FILETIME LastWriteTime;
	FILETIME LastAccessTime;
	ULONG FileAttributes;
	ULONG AligmentOrReservedOrUnknown[3];
	ULONG QuotaId; //NTFS 3.0 or higher
	ULONG SecurityID; //NTFS 3.0 or higher
	ULONGLONG QuotaCharge; //NTFS 3.0 or higher
	USN Usn; //NTFS 3.0 or higher
} STANDARD_INFORMATION, *PSTANDARD_INFORMATION;

/* ATTRIBUTE_LIST 
	is always nonresident and consists of an array of ATTRIBUTE_LIST
*/
typedef struct {
	ATTRIBUTE_TYPE Attribute;
	USHORT Length;
	UCHAR NameLength;
	USHORT NameOffset; // starts at structure begin
	ULONGLONG LowVcn;
	ULONGLONG FileReferenceNumber;
	USHORT AttributeNumber;
	USHORT AligmentOrReserved[3];
}ATTRIBUTE_LIST, *PATTRIBUTE_LIST;

/* FILENAME_ATTRIBUTE
	is always resident
	ULONGLONG informations only updated, if name changes
*/
typedef struct {
	ULONGLONG DirectoryFileReferenceNumber; //points to a MFT Index of a directory
	FILETIME CreationTime; //saved on creation, changed when filename changes
	FILETIME ChangeTime;
	FILETIME LastWriteTime;
	FILETIME LastAccessTime;
	ULONGLONG AllocatedSize;
	ULONGLONG DataSize; 
	ULONG FileAttributes; // ditto
	ULONG AligmentOrReserved;
	UCHAR NameLength;
	UCHAR NameType; // 0x01 Long 0x02 Short 0x00 Posix?
	WCHAR Name[1];
}FILENAME_ATTRIBUTE, *PFILENAME_ATTRIBUTE;

#define POSIX_NAME 0
#define WIN32_NAME 1
#define DOS_NAME 2
#define WIN32DOS_NAME 3


/* MYSTRUCTS


*/
#define NTFSDISK  1

// not supported
#define FAT32DISK 2
#define FATDISK   4
#define EXT2	  8

#define UNKNOWN 0xff99ff99


struct SEARCHFILEINFO {
    LPCWSTR FileName;
    USHORT FileNameLength;
    USHORT Flags;
    ULARGE_INTEGER ParentId;

    ULONGLONG DataSize;
    ULONGLONG AllocatedSize;

    char data[64];
};


typedef struct {
	HANDLE fileHandle;
	DWORD type;
	DWORD IsLong;
	DWORD filesSize;
	DWORD realFiles;
	WCHAR DosDevice;
	PHEAPBLOCK heapBlock;
	union
	{
		struct
		{
			BOOT_BLOCK bootSector;
			DWORD BytesPerFileRecord;
			DWORD BytesPerCluster;
			BOOL complete;
			DWORD sizeMFT;
			DWORD entryCount;
			ULARGE_INTEGER MFTLocation;
			UCHAR *MFT;
			UCHAR *Bitmap;
		} NTFS;
		struct
		{
			DWORD FAT;
		} FAT;


        SEARCHFILEINFO *fFiles;
	};
}DISKHANDLE, *PDISKHANDLE;

typedef struct
{
	HWND hWnd;
	DWORD Value;
}STATUSINFO, *PSTATUSINFO;

VOID CallMe(PSTATUSINFO info, DWORD value);

/* MY FUNCTIONS

*/

#define LONGINFO		1
#define SHORTINFO		2
#define SEARCHINFO		3
#define EXTRALONGINFO	4

typedef DWORD(__cdecl *FETCHPROC)(PDISKHANDLE, PFILE_RECORD_HEADER, PUCHAR);


PDISKHANDLE OpenDisk(LPCTSTR disk);
PDISKHANDLE OpenDisk(WCHAR DosDevice);

BOOL CloseDisk(PDISKHANDLE disk);
ULONGLONG LoadMFT(PDISKHANDLE disk, BOOL complete);
BOOL FixFileRecord(PFILE_RECORD_HEADER file);

DWORD ReadNextCluster(PDISKHANDLE disk, PVOID buffer); 
PUCHAR FindAttribute(PDISKHANDLE disk, ATTRIBUTE_TYPE type);
PATTRIBUTE FindAttribute(PFILE_RECORD_HEADER file, ATTRIBUTE_TYPE type);
PATTRIBUTE FindNextAttribute(PATTRIBUTE att, ATTRIBUTE_TYPE type);
LPCWSTR GetFileName(PFILE_RECORD_HEADER file);
PFILENAME_ATTRIBUTE FindFileName(PFILE_RECORD_HEADER file, USHORT type);

DWORD ParseMFT(PDISKHANDLE disk, UINT option, PSTATUSINFO info);


std::wstring GetPath(const PDISKHANDLE disk, int id);
//LPWSTR GetCompletePath(PDISKHANDLE disk, int id);
BOOL FetchSearchInfo(PDISKHANDLE disk, PFILE_RECORD_HEADER file, SEARCHFILEINFO* data);

ULONG RunLength(PUCHAR run);
LONGLONG RunLCN(PUCHAR run);
ULONGLONG RunCount(PUCHAR run);
BOOL FindRun(PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, PULONGLONG lcn, PULONGLONG count);

DWORD ReadDataLCN(PDISKHANDLE disk, ULONGLONG lcn, ULONG count, PUCHAR buffer);
DWORD ReadExternalData(PDISKHANDLE disk, PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, ULONG count, PUCHAR buffer);
DWORD ReadData(PDISKHANDLE disk, PATTRIBUTE attr, PUCHAR buffer);
DWORD ReadMFTParse(PDISKHANDLE disk, PNONRESIDENT_ATTRIBUTE attr, ULONGLONG vcn, ULONG count, PVOID buffer, FETCHPROC fetch, PSTATUSINFO info);
DWORD ReadMFTLCN(PDISKHANDLE disk, ULONGLONG lcn, ULONG count, PVOID buffer, FETCHPROC fetch, PSTATUSINFO info);

DWORD ProcessBuffer(PDISKHANDLE disk, PUCHAR buffer, DWORD size, FETCHPROC fetch);

BOOL ReparseDisk(PDISKHANDLE disk, UINT option, PSTATUSINFO info);

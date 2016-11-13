// Code to fix the structures but only for filenames

#include "stdafx.h"
#include "FixList.h"

LINKITEM *fixlist=NULL;
LINKITEM *curfix=NULL;


void AddToFixList(int entry, int data)
{
	curfix->entry = entry;
	curfix->data = data;
	curfix->next = new LINKITEM;
	curfix = curfix->next;
	curfix->next = NULL;
}

void CreateFixList()
{
	fixlist = new LINKITEM;
	fixlist->next = NULL;
	curfix = fixlist;
}

void ProcessFixList(PDISKHANDLE disk)
{
	SEARCHFILEINFO *info, *src;
	while (fixlist->next!=NULL)
	{
		info = &disk->fFiles[fixlist->entry];
		src = &disk->fFiles[fixlist->data];
		info->FileName = src->FileName;
		info->FileNameLength = src->FileNameLength;
		
		info->ParentId = src->ParentId;
		// hide all that we used for cleanup
		src->ParentId.QuadPart = 0;
		LINKITEM *item;
		item = fixlist;
		fixlist = fixlist->next;
		delete item;
	}
	fixlist = NULL;
	curfix = NULL;
}


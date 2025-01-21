// Heaps

#include "stdafx.h"
#include "Heap.h"

PHEAPBLOCK CreateHeap(DWORD size)
{
    auto tmp = static_cast<PHEAPBLOCK>(malloc(sizeof(HEAPBLOCK)));
    tmp->current = 0;
    tmp->size = size;
    tmp->next = nullptr;
    tmp->data = static_cast<PUCHAR>(malloc(size));
    if (tmp->data != nullptr)
    {
        //currentBlock = tmp;
        tmp->end = tmp;
        return tmp;
    }


    free(tmp);
    return nullptr;

}

BOOL FreeHeap(PHEAPBLOCK block)
{
    if (block != nullptr)
    {
        FreeAllBlocks(block);
        return TRUE;
    }
    return FALSE;
}

LPTSTR AllocAndCopyString(PHEAPBLOCK block, LPCTSTR string, DWORD size)
{
    PHEAPBLOCK back = nullptr;
    PUCHAR ret = nullptr;
    DWORD rsize = (size + 1) * sizeof(TCHAR);
    int asize = ((rsize) & 0xfffffff8) + 8;

    if (asize <= rsize) {
        DebugBreak();
    }

    auto tmp = block->end;

    if (tmp != nullptr)
    {
        auto t = tmp->size - tmp->current;
        if (t > asize) {
            goto copy;
        }
        back = tmp;


        tmp = tmp->next;
        if (tmp != nullptr)
        {
            t = tmp->size - tmp->current;
            if (t > asize)
            {
                block->end = tmp;
                goto copy;

            }
            back = tmp;
            tmp = tmp->next;
        }
    }
    tmp = static_cast<PHEAPBLOCK>(malloc(sizeof(HEAPBLOCK)));
    memset(tmp, 0, sizeof(HEAPBLOCK));
    tmp->data = static_cast<PUCHAR>(malloc(block->size));
    if (tmp->data != nullptr)
    {
        tmp->size = block->size;
        tmp->next = nullptr;

        if (back == nullptr) {
            back = block->end;
        }
        tmp->end = block;
        back->next = tmp;
        block->end = tmp;
        goto copy;
    }
    else
    {
        DebugBreak();
        free(tmp);
        return nullptr;
    }

copy:
    ret = &tmp->data[tmp->current];
    memcpy(ret, string, rsize);
    ret[rsize] = 0;
    ret[rsize + 1] = 0;
    tmp->current += asize;
    return reinterpret_cast<LPTSTR>(ret);

}

BOOL FreeAllBlocks(PHEAPBLOCK block)
{
    PHEAPBLOCK tmp = block;

    while (tmp != nullptr)
    {
        free(tmp->data);
        auto back = tmp;
        tmp = tmp->next;
        free(back);
    }

    return TRUE;
}

BOOL ReUseBlocks(PHEAPBLOCK block, BOOL clear)
{
    if (block != nullptr)
    {
        PHEAPBLOCK tmp = block;
        while (tmp != nullptr)
        {
            tmp->current = 0;
            tmp = tmp->next;
            if (clear != 0) {
                memset(tmp->data, 0, tmp->size * sizeof(TCHAR));
            }
        }
        block->end = block;
    }
    return TRUE;
}

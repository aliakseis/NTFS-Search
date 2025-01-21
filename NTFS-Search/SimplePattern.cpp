// Code to support wildchars

#include "stdafx.h"
#include "SimplePattern.h"

static int wcsnrcmp(
    const wchar_t* first,
    const wchar_t* last,
    size_t count
)
{
    if (count == 0u) {
        return(0);
    }

    while ((--count != 0u) && *first == *last)
    {
        first--;
        last--;
    }

    return ((*first - *last));
}
// Prepares the searchpattern struct
SEARCHP StartSearch(wchar_t* string, int len)
{
    if (len <= 1)
        return { -1 };

    SEARCHP sp;
    sp.mode = 0;
    if (string[len - 1] == L'*')
    {
        sp.mode += 2;
        string[len - 1] = 0;
        len--;
    }
    if (string[0] == L'*')
    {
        sp.mode += 1;
        string++;
        len--;
    }

    sp.string = string;
    sp.len = len;
    sp.totallen = sp.len;
    if (sp.mode == 0)
    {
        auto res = wcschr(string, L'*');
        if (res != nullptr)
        {
            sp.mode = 42;
            *res = L'\0';
            sp.len = res - string;
            sp.extra = &res[1];
            sp.extralen = len - sp.len - 1;
            sp.totallen = sp.len + sp.extralen;
        }
    }
    return sp;
}

// does the actual search
bool SearchStr(SEARCHP& pattern, wchar_t* string, int len)
{
    if (pattern.totallen > len) {
        return false;
    }
    switch (pattern.mode)
    {
    case 0:
        if (wcscmp(string, pattern.string) == 0) {
            return true;
        }
        break;
    case 1:
        if (wcsnrcmp(string + len, pattern.string + pattern.len, pattern.len + 1) == 0) {
            return true;
        }
        break;
    case 2:
        if (wcsncmp(string, pattern.string, pattern.len) == 0) {
            return true;
        }
        break;
    case 3:
        if (wcsstr(string, pattern.string) != nullptr) {
            return true;
        }
        break;
    case 42:
        if (wcsnrcmp(string + len, pattern.extra + pattern.extralen, pattern.extralen + 1) == 0) {
            if (wcsncmp(string, pattern.string, pattern.len) == 0) {
                return true;
            }
        }
        break;
    }
    return false;
}

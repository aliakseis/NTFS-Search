// Simple- really

struct SEARCHP
{
	int mode;
	wchar_t* string;
	int len;
	wchar_t* extra;
	int extralen;
	int totallen;
    operator bool() const { return mode != -1; }
};

SEARCHP StartSearch(wchar_t* string, int len);
bool SearchStr(SEARCHP& pattern, wchar_t* string, int len);

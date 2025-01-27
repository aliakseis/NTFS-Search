// NTFSearch.cpp : Defines the entry point for the application.
//

/* BugFixes
    - limit Results // more or less done 0xfff0
    - extend searchstrings
    - include sort // nearly done
    - include typeinformation // done
    - progressbar completion / fixed by cheating
*/



#include "stdafx.h"
#include "commdlg.h"
#include "NTFS-Search.h"
#include "commctrl.h"
#include "shellapi.h"
#include "NTFS_STRUCT.h"
#include "objidl.h"
#include "shlobj.h"
#include "shlwapi.h"
#include "SimplePattern.h"
#include "process.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <cassert>
#include <sstream>

enum { MAX_LOADSTRING = 100 };

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define PACKVERSION(major,minor) MAKELONG(minor,major)


PHEAPBLOCK FileStrings;
PHEAPBLOCK PathStrings;

typedef struct SearchResult
{
    int icon;
    LPTSTR extra;
    LPTSTR filename;
    LPTSTR path;

    ULONGLONG dataSize;
    ULONGLONG allocatedSize;

}*PSearchResult;

//SearchResult results[0xffff];
//int results_cnt = 0;

std::vector<SearchResult> results;

const auto MAX_RESULTS_NUMBER = 2000000;

int glSensitive = FALSE;
BOOL glHelp = FALSE;

struct ThreadInfo
{
    HWND hWnd;
    PDISKHANDLE disk;
};

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR szLS[MAX_LOADSTRING];
TCHAR szNoS[MAX_LOADSTRING];
TCHAR szTotal[MAX_LOADSTRING];
TCHAR szFound[MAX_LOADSTRING];
TCHAR szSearch[MAX_LOADSTRING];
TCHAR szLoading[MAX_LOADSTRING];
TCHAR szResults[MAX_LOADSTRING];
TCHAR szFiles[] = TEXT("Textfile\0*.txt\0All files\0*.*\0\0");
TCHAR szSaveRes[MAX_LOADSTRING];
TCHAR szSaveResErr[MAX_LOADSTRING];
TCHAR szTooMany[MAX_LOADSTRING];
TCHAR szDeletedFile[MAX_PATH];
TCHAR szStrange[MAX_LOADSTRING];
TCHAR szAccessDenied[MAX_LOADSTRING];
TCHAR szFNF[MAX_PATH];
TCHAR szDelete[MAX_PATH];
TCHAR szDeleteOnReboot[MAX_PATH];
TCHAR szDiskError[MAX_LOADSTRING];
TCHAR szWarning[MAX_LOADSTRING];

HIMAGELIST list, list2, list3;
HMENU popup, hm;

PDISKHANDLE disks[32];
VOID ReleaseAllDisks();

//Controls
HWND hEdit, hListView, hCheck1, hCheck2, hGroup, hStatus, hCombo;
HWND glDlg = nullptr;
// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE /*hInstance*/, int /*nCmdShow*/);
LRESULT CALLBACK	WndProc(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/);
INT_PTR CALLBACK	About(HWND /*hDlg*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/);
INT_PTR CALLBACK	Help(HWND /*hDlg*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/);
INT_PTR CALLBACK	Waiting(HWND /*hDlg*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/);

LRESULT CALLBACK	SearchDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void StartLoading(PDISKHANDLE disk, HWND hWnd);
int ProcessLoading(HWND hWnd, HWND hCombo, int reload);

DWORD WINAPI LoadSearchInfo(LPVOID lParam);
int SearchFiles(HWND hWnd, PDISKHANDLE disk, TCHAR *filename, bool deleted, SEARCHP& pat);
int Search(HWND hWnd, int disk, TCHAR *filename, bool deleted);
UINT ExecuteFile(HWND hWnd, LPWSTR str, USHORT flags);
UINT ExecuteFileEx(HWND hWnd, const LPTSTR command, LPWSTR str, LPCTSTR dir, UINT show, USHORT flags);

BOOL UnloadDisk(HWND hWnd, int index);
BOOL SearchString(LPWSTR pattern, int length, LPWSTR string, int len);

int filecompare(const void *arg1, const void *arg2);
int pathcompare(const void *arg1, const void *arg2);
int extcompare(const void *arg1, const void *arg2);

BOOL ProcessPopupMenu(HWND hWnd, int index, DWORD item);
BOOL SaveResults(LPWSTR filename);
#define FILES 0
#define FILENAMES 1
#define DELETENOW 1
#define DELETEONREBOOT 42

void PrepareCopy(HWND hWnd, UINT flags);
void DeleteFiles(HWND hWnd, UINT flags);

DWORD GetDllVersion(LPCTSTR lpszDllName);
DWORD ShowError();


int APIENTRY _tWinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR    lpCmdLine,
    int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO(Aliaksei Sanko): Place code here.
    MSG msg;
    HACCEL hAccelTable;


    INITCOMMONCONTROLSEX init;
    init.dwSize = sizeof(init);
    init.dwICC = ICC_WIN95_CLASSES | ICC_USEREX_CLASSES;//ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS |ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&init);


    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_NTFSEARCH, szWindowClass, MAX_LOADSTRING);
    LoadString(hInstance, IDS_LOADED, szLS, MAX_LOADSTRING);
    LoadString(hInstance, IDS_UNSUPPORTED, szNoS, MAX_LOADSTRING);
    LoadString(hInstance, IDS_TOTAL, szTotal, MAX_LOADSTRING);
    LoadString(hInstance, IDS_FOUND, szFound, MAX_LOADSTRING);
    LoadString(hInstance, IDS_SEARCHING, szSearch, MAX_LOADSTRING);
    LoadString(hInstance, IDS_LOADING, szLoading, MAX_LOADSTRING);
    LoadString(hInstance, IDS_RESULTS, szResults, MAX_LOADSTRING);
    //LoadString(hInstance, IDS_FILES, szSaveRes, MAX_LOADSTRING);
    LoadString(hInstance, IDS_SAVERES, szSaveRes, MAX_LOADSTRING);
    LoadString(hInstance, IDS_SAVERESERR, szSaveResErr, MAX_LOADSTRING);
    LoadString(hInstance, IDS_TOOMANY, szTooMany, MAX_LOADSTRING);
    LoadString(hInstance, IDS_DELETEDFILE, szDeletedFile, MAX_PATH);
    LoadString(hInstance, IDS_FILENOTFOUND, szFNF, MAX_PATH);
    LoadString(hInstance, IDS_LOADSTRANGE, szStrange, MAX_LOADSTRING);
    LoadString(hInstance, IDS_ACCESSDENIED, szAccessDenied, MAX_LOADSTRING);
    LoadString(hInstance, IDS_DELETE, szDelete, MAX_PATH);
    LoadString(hInstance, IDS_DELETEONREBOOT, szDeleteOnReboot, MAX_PATH);
    LoadString(hInstance, IDS_DISKERROR, szDiskError, MAX_LOADSTRING);
    LoadString(hInstance, IDS_WARNING, szWarning, MAX_LOADSTRING);

    MyRegisterClass(hInstance);

    //SearchString(TEXT("*a*W*"),5, TEXT("Hallo Welt"),10);

    FileStrings = CreateHeap(0xffff * sizeof(SearchResult));
    PathStrings = CreateHeap(0xfff * MAX_PATH);

    // Perform application initialization:
    if (InitInstance(hInstance, nCmdShow) == 0)
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NTFSEARCH));

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        //IsDialogMessage(msg.hwnd, &msg);
        if (!TranslateAccelerator(glDlg, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    FreeHeap(PathStrings);
    FreeHeap(FileStrings);

    //Beep(2500,150);
    return static_cast<int>(msg.wParam);
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NTFSEARCH));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_NTFSEARCH);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;

    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

    if (hWnd == nullptr)
    {
        return FALSE;
    }


    HICON icon;
    HINSTANCE shell;

    shell = LoadLibrary(TEXT("shell32.dll"));

    list = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_MASK | ILC_COLOR32, 8, 8);
    icon = LoadIcon(shell, MAKEINTRESOURCE(8));
    ImageList_AddIcon(list, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(9));
    ImageList_AddIcon(list, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(10));
    ImageList_AddIcon(list, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(5));
    ImageList_AddIcon(list, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(11));
    ImageList_AddIcon(list, icon);
    DestroyIcon(icon);

    // List2
    list2 = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_MASK | ILC_COLOR32, 8, 8);
    icon = LoadIcon(shell, MAKEINTRESOURCE(1));
    ImageList_AddIcon(list2, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(2));
    ImageList_AddIcon(list2, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(42));
    ImageList_AddIcon(list2, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(5));
    ImageList_AddIcon(list2, icon);
    DestroyIcon(icon);
    icon = LoadIcon(shell, MAKEINTRESOURCE(10));
    ImageList_AddIcon(list2, icon);
    DestroyIcon(icon);

    FreeLibrary(shell);


    popup = LoadMenu(hInst, (LPWSTR)IDR_MENU1);

    hm = GetSubMenu(popup, 0);
    SetMenuDefaultItem(hm, 0, TRUE);

    //CreateDialogParam(hInst, (LPCTSTR)IDD_SEARCH, NULL, (DLGPROC)SearchDlg,0);
    DialogBox(hInst, MAKEINTRESOURCE(IDD_SEARCH), nullptr, (DLGPROC)SearchDlg);

    DestroyWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_COMMAND:
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case EN_CHANGE:

            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // TODO(Aliaksei Sanko): Add any drawing code here...
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        ImageList_Destroy(list);
        ImageList_Destroy(list2);
        DestroyMenu(popup);
        ReleaseAllDisks();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return static_cast<INT_PTR>(TRUE);

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return static_cast<INT_PTR>(TRUE);
        }
        break;
    }
    return static_cast<INT_PTR>(FALSE);
}

// Help handler
INT_PTR CALLBACK Help(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT("Help - HELP - Help"));
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT("You can use * as a wildcard"));
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT("*AnyText* - *.*"));
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT("*AnyText - *.html"));
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT("AnyText* - Data.*"));
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT("Any*Text - Use*Data"));
        SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)TEXT(""));
        glHelp = TRUE;
        return static_cast<INT_PTR>(TRUE);
    case WM_MEASUREITEM:
    {
        LPMEASUREITEMSTRUCT item;
        item = (LPMEASUREITEMSTRUCT)lParam;
        item->itemHeight = 50;
    }
    break;
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT item;
        item = (LPDRAWITEMSTRUCT)lParam;
        if ((item->itemAction & ODA_DRAWENTIRE) != 0u)
        {
            DrawText(item->hDC, (TCHAR*)item->itemData, wcslen((TCHAR*)item->itemData), &item->rcItem, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        }
    }
    break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return static_cast<INT_PTR>(TRUE);
        }
        break;
    case WM_DESTROY:
        glHelp = FALSE;
        break;
    }
    return static_cast<INT_PTR>(FALSE);
}


LRESULT CALLBACK	SearchDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int id, msg;
    int index;
    TCHAR data;
    DWORD loaded;

    static TCHAR tmp[MAX_PATH];
    static TCHAR path[0xffff];
    COMBOBOXEXITEM item;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        TCHAR buf[MAX_PATH];
        //glDlg = hWnd;
        //hStatus = CreateWindowEx(0, STATUSCLASSNAME , NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP , 20, 20, 100, 100, hWnd, NULL, hInst, NULL);
        hListView = GetDlgItem(hWnd, IDC_RESULT);
        hCombo = GetDlgItem(hWnd, IDC_DRIVES);

        if (GetDllVersion(TEXT("comctl32.dll")) >= PACKVERSION(6, 0))
        {
            //Proceed.
            ListView_SetExtendedListViewStyle(hListView, LVS_EX_DOUBLEBUFFER);
            hStatus = CreateWindowEx(WS_EX_COMPOSITED, STATUSCLASSNAME, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 20, 20, 100, 100, hWnd, nullptr, hInst, nullptr);

        }
        else
        {
            // Use an alternate approach for older DLL versions.
            hStatus = CreateWindowEx(0, STATUSCLASSNAME, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 20, 20, 100, 100, hWnd, nullptr, hInst, nullptr);

        }

        LVCOLUMN col;

        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;// | LVCF_IMAGE | LVCF_FMT;
        col.fmt = HDF_BITMAP_ON_RIGHT;
        LoadString(hInst, IDS_FILENAME, &buf[0], MAX_PATH);
        col.pszText = buf;
        //col.iImage = 5;
        col.cx = 220;
        col.iSubItem = 0;

        ListView_InsertColumn(hListView, 0, &col);

        LoadString(hInst, IDS_PATH, &buf[0], MAX_PATH);

        col.pszText = buf;
        col.cx = 320;
        col.iSubItem = 2;

        ListView_InsertColumn(hListView, 2, &col);

        LoadString(hInst, IDS_EXT, &buf[0], MAX_PATH);

        col.pszText = buf;
        col.cx = 60;
        col.iSubItem = 1;

        ListView_InsertColumn(hListView, 1, &col);

        // TODO strings in resources

        col.pszText = _T("Size");
        col.cx = 60;
        col.iSubItem = 3;

        ListView_InsertColumn(hListView, 3, &col);

        col.pszText = _T("Allocated Size");
        col.cx = 60;
        col.iSubItem = 4;

        ListView_InsertColumn(hListView, 4, &col);


        //ComboBox_SetMinVisible(hCombo, 5);
        ListView_SetImageList(hListView, list2, LVSIL_SMALL);

        DWORD drives;
        drives = GetLogicalDrives();


        memset(&item, 0, sizeof(item));
        item.iItem = -1;
        item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_LPARAM | CBEIF_SELECTEDIMAGE;
        item.iImage = 0;
        item.iSelectedImage = 1;
        SendMessage(hCombo, CBEM_SETIMAGELIST, 0, (LPARAM)list);

        LoadString(hInst, IDS_USELOADED, &buf[0], MAX_PATH);

        item.pszText = buf;
        item.lParam = 0xff;
        SendMessage(hCombo, CBEM_INSERTITEM, 0, (LPARAM)&item);


        for (int i = 0; i < 32; i++)
        {
            if (((drives >> (i)) & 0x1) != 0u)
            {

                TCHAR str[5];
                UINT type;

                wsprintf(str, TEXT("%C:\\"), 0x41 + i);
                item.pszText = str;
                item.lParam = i;
                type = GetDriveType(str);
                if (type == DRIVE_FIXED)
                {
                    SendMessage(hCombo, CBEM_INSERTITEM, 0, (LPARAM)&item);
                }
            }

        }
        LoadString(hInst, IDS_LOADALL, &buf[0], MAX_PATH);

        item.pszText = buf;
        item.lParam = 0xfe;
        SendMessage(hCombo, CBEM_INSERTITEM, 0, (LPARAM)&item);


        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
        int parts[2] = { 250,-1 };
        SendMessage(hStatus, SB_SETPARTS, 2, (LPARAM)&parts);

        SendDlgItemMessage(hWnd, IDC_DELETED, BM_SETCHECK, BST_CHECKED, 0);

        return TRUE;
    }
    case WM_SIZE:
    {
        int width, height;
        RECT rt, rt2;
        width = LOWORD(lParam);
        height = HIWORD(lParam);

        //MoveWindow(hEdit, 20, 20, width - 40, 20, TRUE);
        //MoveWindow(hCombo, 20, 50, width - 40, 120, TRUE);
        MoveWindow(hStatus, 0, height, width, 20, TRUE);
        GetWindowRect(hStatus, &rt);
        GetWindowRect(hCombo, &rt2);
        rt2.bottom += 12;
        MapWindowPoints(nullptr, hWnd, reinterpret_cast<LPPOINT>(&rt2), 2);
        MoveWindow(hListView, 0, (rt2.bottom) + 20, width, height - (rt.bottom - rt.top) - (rt2.bottom + 20), TRUE);


        break;
    }
    case WM_NOTIFY:
        LPNMHDR hdr;
        LPNMLISTVIEW listitem;
        hdr = (LPNMHDR)lParam;
        if (hdr->hwndFrom == hListView)
        {
            //int len;
            switch (hdr->code)
            {
            case NM_DBLCLK:

                index = SendMessage(hdr->hwndFrom, LVM_GETSELECTIONMARK, 0, 0);
                if (index >= 0)
                {
                    ProcessPopupMenu(hWnd, index, IDM_OPEN);
                }
                break;
            case NM_RCLICK:

                break;
            case LVN_DELETEALLITEMS:
                SetLastError(0);
                SetWindowLongPtr(hWnd, DWLP_MSGRESULT, TRUE);
                return TRUE;
            case LVN_DELETEITEM:
                Beep(2500, 5);
                break;
            case NM_RETURN:
                Beep(2500, 50);
                break;
            case LVN_ODFINDITEM:
                NMLVFINDITEM *finditem;
                LONG_PTR pt;


                finditem = (NMLVFINDITEM*)lParam;
                if ((finditem->lvfi.flags & LVFI_STRING) != 0u)
                {
                    TCHAR stmp[2], stmp2[2];
                    stmp[0] = finditem->lvfi.psz[0];
                    stmp[1] = 0;
                    stmp2[1] = 0;
                    CharLower(stmp);
                    int j = 0;
                    for (int i = finditem->iStart; i != finditem->iStart - 1; i++)
                    {
                        if (j >= results.size()) {
                            break;
                        }
                        if (i >= results.size()) {
                            i = 0;
                        }
                        stmp2[0] = results[i].filename[0];
                        CharLower(stmp2);
                        if (stmp[0] == stmp2[0])
                        {
                            pt = i;
                            SetWindowLongPtr(hWnd, DWLP_MSGRESULT, pt);
                            return TRUE;
                        }
                        j++;
                    }
                }
                pt = -1;
                SetWindowLongPtr(hWnd, DWLP_MSGRESULT, pt);
                return TRUE;
                break;
            case LVN_GETDISPINFO:
                NMLVDISPINFO* info;
                info = (NMLVDISPINFO*)lParam;

                if (info->item.iItem > -1 && info->item.iItem < results.size())
                {
                    auto res = &results[info->item.iItem];

                    if ((info->item.mask & LVIF_TEXT) != 0u)
                    {
                        switch (info->item.iSubItem)
                        {
                        case 0: info->item.pszText = res->filename; break;
                        case 1: info->item.pszText = res->extra; break;
                        case 2: info->item.pszText = res->path; break;
                            //case 3: _stprintf_s(info->item.pszText, info->item.cchTextMax, _T("%llu"), res->dataSize); break;
                            //case 4: _stprintf_s(info->item.pszText, info->item.cchTextMax, _T("%llu"), res->allocatedSize); break;
                        case 3: case 4:
                        {
                            // Create a stringstream and imbue it with the locale
                            std::basic_stringstream<TCHAR> ss;
                            ss.imbue(std::locale(""));

                            // Use the numpunct facet to format the number with thousands separators
                            ss << std::fixed << ((info->item.iSubItem == 3) ? res->dataSize : res->allocatedSize);
                            _tcscpy_s(info->item.pszText, info->item.cchTextMax, ss.str().c_str());
                        }
                        break;
                        }
                    }

                    if ((info->item.mask & LVIF_IMAGE) != 0u)
                    {
                        info->item.iImage = res->icon;
                    }
                }

                break;
            case LVN_BEGINDRAG:
                listitem = (LPNMLISTVIEW)lParam;
                // Begin drag here
                //Beep(800,50);
                break;
            case LVN_COLUMNCLICK:
                listitem = (LPNMLISTVIEW)lParam;
                switch (listitem->iSubItem)
                {
                case 0: qsort((void*)&results[0], results.size(), sizeof(SearchResult), filecompare); break;
                case 1: qsort((void*)&results[0], results.size(), sizeof(SearchResult), extcompare); break;
                case 2: qsort((void*)&results[0], results.size(), sizeof(SearchResult), pathcompare); break;
                case 3: std::sort(results.data(), results.data() + results.size(),
                        [](const SearchResult& left, const SearchResult& right) { return left.dataSize > right.dataSize; });
                    break;
                case 4: std::sort(results.data(), results.data() + results.size(),
                        [](const SearchResult& left, const SearchResult& right) { return left.allocatedSize > right.allocatedSize; });
                    break;
                }
                InvalidateRect(listitem->hdr.hwndFrom, nullptr, TRUE);
                break;
            }
        }
        break;
    case WM_USER + 2:
        int count;
        count = SendMessage(hCombo, CB_GETCOUNT, 0, 0);

        memset(&item, 0, sizeof(item));

        item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
        item.iImage = 2;
        item.iSelectedImage = 2;
        loaded = 0;
        for (int i = 0; i < count; i++)
        {
            data = SendMessage(hCombo, CB_GETITEMDATA, i, 0);
            if (data > 0 && data < 32)
            {
                if (disks[data] != nullptr)
                {
                    if (disks[data]->filesSize != 0)
                    {
                        wsprintf(tmp, &szLS[0]/*TEXT("%C:\\ {loaded with %d entries}")*/, disks[data]->DosDevice, disks[data]->filesSize);
                        item.iItem = i;
                        item.pszText = tmp;
                        SendMessage(hCombo, CBEM_SETITEM, 0, (LPARAM)&item);
                    }
                    else
                    {
                        wsprintf(tmp, &szNoS[0]/*TEXT("%C:\\ {UNSUPPORTED}")*/, disks[data]->DosDevice, disks[data]->filesSize);
                        item.iItem = i;
                        item.iImage = 4;
                        item.iSelectedImage = 4;
                        item.pszText = tmp;
                        SendMessage(hCombo, CBEM_SETITEM, 0, (LPARAM)&item);
                        item.iImage = 2;
                        item.iSelectedImage = 2;
                    }
                    loaded += disks[data]->realFiles;
                }
                else
                {
                    wsprintf(tmp, TEXT("%C:\\"), 0x41 + data);
                    item.iItem = i;
                    item.pszText = tmp;
                    item.iImage = 0;
                    item.iSelectedImage = 1;
                    SendMessage(hCombo, CBEM_SETITEM, 0, (LPARAM)&item);
                    item.iImage = 2;
                    item.iSelectedImage = 2;
                }
            }
        }
        InvalidateRect(hCombo, nullptr, TRUE);
        wsprintf(tmp, &szTotal[0]/*TEXT("%d entries total")*/, loaded);
        SendMessage(hStatus, SB_SETTEXT, 1, (LPARAM)tmp);
        break;
    case WM_CONTEXTMENU:
        if (hListView == (HWND)wParam)
        {
            index = SendMessage(hListView, LVM_GETSELECTIONMARK, 0, 0);
            if (index >= 0)
            {
                POINTS pt;
                //POINT pt2;
                RECT rt, rt2;
                DWORD dw;

                pt = MAKEPOINTS(lParam);
                if (pt.x == -1 && pt.y == -1)
                {
                    ListView_GetItemRect(hListView, index, &rt, LVIR_SELECTBOUNDS);
                    GetWindowRect(hListView, &rt2);
                    dw = ListView_GetTopIndex(hListView);
                    dw = index - dw;

                    dw = TrackPopupMenu(hm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, rt2.left + rt.right, rt2.top + (rt.bottom - rt.top)*(dw + 1), 0, hWnd, nullptr);
                }
                else {
                    dw = TrackPopupMenu(hm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, nullptr);
                }
                ProcessPopupMenu(hWnd, index, dw);
            }
        }
        break;
    case WM_COMMAND:
        id = LOWORD(wParam);
        msg = HIWORD(wParam);

        if (msg == EN_SETFOCUS)
        {
            EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
            break;
        }
        if (id == IDM_CLEAR)
        {
            ListView_DeleteAllItems(hListView);
            EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
        }
        else if (id == IDM_ABOUT)
        {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        }
        else if (id == IDM_SAVE)
        {
            OPENFILENAME of;
            wsprintf(tmp, &szResults[0]/*TEXT("results_%d.txt")*/, results.size());
            memset(&of, 0, sizeof(OPENFILENAME));
            of.lStructSize = sizeof(OPENFILENAME);
            of.lpstrFile = tmp;
            of.nMaxFile = MAX_PATH;
            of.lpstrFilter = szFiles;//TEXT("Textfile\0*.txt\0All files\0*.*\0\0");
            of.nFileExtension = 1;
            of.hwndOwner = hWnd;
            of.Flags = OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;
            of.lpstrTitle = szSaveRes;//TEXT("Save results");
            if (GetSaveFileName(&of) != 0)
            {
                if (SaveResults(of.lpstrFile) == FALSE) {
                    MessageBox(hWnd, szSaveResErr /*TEXT("The results couldn't be saved.")*/, nullptr, MB_ICONERROR);
                }
            }
        }
        else if (id == ID_HELP)
        {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_HELP), hWnd, Help);
        }
        if (id == IDOK || id == IDOK2)
        {

            if (hListView == GetFocus())
            {
                index = SendMessage(hListView, LVM_GETSELECTIONMARK, 0, 0);
                if (index >= 0)
                {
                    ProcessPopupMenu(hWnd, index, IDM_OPEN);
                }
                //Beep(2500,50);
            }
            else
            {

                wsprintf(tmp, &szSearch[0]/*TEXT("Searching ...")*/);
                SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)tmp);

                index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                data = SendMessage(hCombo, CB_GETITEMDATA, index, 0);
                GetDlgItemText(hWnd, IDC_EDIT, tmp, MAX_PATH);

                int b = SendDlgItemMessage(hWnd, IDC_DELETED, BM_GETCHECK, 0, 0);
                DWORD res = Search(hWnd, data, tmp, b != BST_CHECKED);

                wsprintf(tmp, &szFound[0]/* TEXT("%d files found.")*/, res);
                SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)tmp);
                //SendDlgItemMessage(hWnd, IDOK, WM_ENABLE, FALSE, 0);
                //EnableWindow((HWND)lParam, FALSE);
                b = SendDlgItemMessage(hWnd, IDC_LIVE, BM_GETCHECK, 0, 0);

                if (res > 0 && b != BST_CHECKED)
                {
                    SetFocus(hListView);
                    ListView_SetSelectionMark(hListView, 0);
                }
                //}
            }
            break;
        }
        if (id == IDC_UNLOAD && msg == BN_CLICKED)
        {
            index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            data = SendMessage(hCombo, CB_GETITEMDATA, index, 0);
            if (data > 0 && data < 32)
            {
                ListView_DeleteAllItems(hListView);
                ReUseBlocks(PathStrings, FALSE);
                UnloadDisk(hWnd, data);
            }
            else
            {
                ListView_DeleteAllItems(hListView);
                ReUseBlocks(PathStrings, FALSE);

                for (int i = 0; i < 32; i++)
                {
                    UnloadDisk(hWnd, i);
                }
            }
        }
        else if (id == IDC_REFRESH)
        {
            ProcessLoading(hWnd, hCombo, TRUE);
        }
        else if (id == IDC_CASE)
        {
            int b = SendDlgItemMessage(hWnd, IDC_CASE, BM_GETCHECK, 0, 0);
            glSensitive = (b == BST_CHECKED);
        }
        else if (msg == EN_CHANGE)
        {
            int len;
            len = SendDlgItemMessage(hWnd, id, WM_GETTEXTLENGTH, 0, 0);
            int b = SendDlgItemMessage(hWnd, IDC_LIVE, BM_GETCHECK, 0, 0);
            EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);

            if (len > 2 && b == BST_CHECKED)
            {
                SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
            }
        }
        else if (msg == CBN_SELCHANGE)
        {
            ProcessLoading(hWnd, (HWND)lParam, FALSE);
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hWnd, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_HELP:
        if (glHelp == 0) {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_HELP), hWnd, Help);
        }

        break;
    }
    return FALSE;
}

DWORD WINAPI LoadSearchInfo(LPVOID lParam)
{
    STATUSINFO status;
    ThreadInfo* info;
    info = static_cast<ThreadInfo*>(lParam);
    status.Value = PBM_SETPOS;
    status.hWnd = GetDlgItem(info->hWnd, IDC_PROGRESS);
    if (info->disk->filesSize == 0)
    {
        DWORD res;
        if (res = static_cast<DWORD>(static_cast<unsigned int>(LoadMFT(info->disk, FALSE) != 0) != 0u) != 0u)
        {
            SendDlgItemMessage(info->hWnd, IDC_PROGRESS, PBM_SETRANGE32, 0, info->disk->NTFS.entryCount);
            ParseMFT(info->disk, SEARCHINFO, &status);
        }
        else if (res == 3)
        {
            MessageBox(info->hWnd, szStrange, nullptr, MB_ICONERROR);
        }
    }
    else
    {
        SendDlgItemMessage(info->hWnd, IDC_PROGRESS, PBM_SETRANGE32, 0, info->disk->NTFS.entryCount);
        ReparseDisk(info->disk, SEARCHINFO, &status);
    }
    Sleep(800);
    SendMessage(info->hWnd, WM_USER + 1, 0, 0);
    return 0;
}

void StartLoading(PDISKHANDLE disk, HWND hWnd)
{
    DialogBoxParam(hInst, (LPCTSTR)IDD_WAIT, hWnd, static_cast<DLGPROC>(Waiting), (LPARAM)disk);
}

INT_PTR CALLBACK Waiting(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        if (lParam != NULL)
        {
            DWORD threadId;
            HANDLE handle;
            TCHAR tmp[256];
            static ThreadInfo info;
            info.disk = (PDISKHANDLE)lParam;
            info.hWnd = hDlg;
            //DWORD range;
            wsprintf(tmp, &szLoading[0]/*TEXT("Loading %C:\\ ... - Please wait")*/, info.disk->DosDevice);
            SetWindowText(hDlg, tmp);
            //range = SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_GETRANGE, TRUE, NULL);
            //handle = CreateThread(NULL, 0, LoadSearchInfo, (LPVOID) &info, 0, &threadId); 
            handle = (HANDLE)_beginthreadex(nullptr, 0, reinterpret_cast<unsigned int(__stdcall*)(void*)>(LoadSearchInfo), (LPVOID)&info, 0, reinterpret_cast<unsigned int*>(&threadId));

            CloseHandle(handle);
        }
        return static_cast<INT_PTR>(TRUE);

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return static_cast<INT_PTR>(TRUE);
        }
        break;
    case WM_USER + 1:
        HWND hWnd;
        hWnd = GetParent(hDlg);
        //DWORD pos;
        //pos = SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_GETPOS, 0, 0);
        if (hWnd != nullptr)
        {
            PostMessage(hWnd, WM_USER + 2, 0, 0);
        }
        EndDialog(hDlg, 0);
        return TRUE;
        break;
    }
    return static_cast<INT_PTR>(FALSE);
}

VOID ReleaseAllDisks()
{
    for (auto & disk : disks)
    {
        if (disk != nullptr) {
            CloseDisk(disk);
        }
    }
}

int Search(HWND hWnd, int disk, TCHAR *filename, bool deleted)
{
    DWORD ret = 0;


    SendMessage(hListView, WM_SETREDRAW, FALSE, 0);


    ListView_DeleteAllItems(hListView);
    ReUseBlocks(PathStrings, FALSE);
    ReUseBlocks(FileStrings, FALSE);
    //results_cnt = 0;
    results.clear();

    auto pat = StartSearch(filename, wcslen(filename));
    if (!pat) {
        return 0;
    }


    if (disk > 0 && disk < 32)
    {
        if (disks[disk] != nullptr)
        {
            ret = SearchFiles(hWnd, disks[disk], filename, deleted, pat);
        }
    }
    else
    {
        for (auto & disk : disks)
        {
            if (disk != nullptr && results.size() < MAX_RESULTS_NUMBER)
            {
                ret += SearchFiles(hWnd, disk, filename, deleted, pat);
            }
        }
    }
    if (ret != results.size()) {
        DebugBreak();
    }
    //results_cnt = ret;
    ListView_SetItemCountEx(hListView, results.size(), 0);
    SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
    //EndSearch(pat);

    return ret;
}

int SearchFiles(HWND hWnd, PDISKHANDLE disk, TCHAR *filename, bool deleted, SEARCHP& pat)
{
    int hit = 0;
    //LVITEM item;
    //PUCHAR data;
    WCHAR tmp[0xffff];
    if (glSensitive == 0)
    {
        _wcslwr(filename);
    }
    const SEARCHFILEINFO* info = disk->fFiles;

    //memset(&item, 0, sizeof(item));
    LVITEM item{};
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;

    for (int i = 0; i < disk->filesSize; i++)
    {
        if (deleted || ((info[i].Flags & 0x1) != 0))
        {
            if (info[i].FileName != nullptr)
            {
                bool ok;
                if (glSensitive == 0)
                {
                    //MessageBox(0,PSEARCHFILEINFO(data)->FileName,0,0);
                    //wcscpy_s(tmp,info->FileName, info->FileNameLength);
                    memcpy(tmp, info[i].FileName, info[i].FileNameLength * sizeof(TCHAR) + 2);
                    _wcslwr(tmp);
                    ok = SearchStr(pat, (wchar_t*)tmp, info[i].FileNameLength);
                }
                else
                {
                    ok = SearchStr(pat, const_cast<wchar_t*>(info[i].FileName), info[i].FileNameLength);
                }
                if (ok)
                    //if (wcsstr(tmp, filename)!=NULL)
                    //if (SearchString(tmp, PSEARCHFILEINFO(data)->FileNameLength, filename, len)==TRUE)
                    //if (StringRegCompare(tmp, PSEARCHFILEINFO(data)->FileNameLength, filename, len)==TRUE)
                {
                    //SearchResult* res = &results[results_cnt++];
                    const auto filename = const_cast<LPTSTR>(info[i].FileName);
                    const auto t = GetPath(disk, i);
                    //const auto s = wcslen(t);
                    const auto path = AllocAndCopyString(PathStrings, t.c_str(), t.length());
                    const auto icon = info[i].Flags;

                    const auto dataSize = info[i].DataSize;
                    const auto allocatedSize = info[i].AllocatedSize;

                    //LPTSTR ret;
                    LPTSTR extra;
                    if ((info[i].Flags & 0x002) == 0)
                    {
                        auto ret = wcsrchr(filename, L'.');
                        if (ret != nullptr) {
                            extra = ret + 1;
                        }
                        else {
                            extra = TEXT(" ");
                        }
                    }
                    else
                    {
                        extra = TEXT(" ");
                    }

                    results.push_back({ icon, extra, filename, path, dataSize, allocatedSize });

                    /*item.pszText = LPSTR_TEXTCALLBACK;//(LPWSTR)PSEARCHFILEINFO(data)->FileName;
                    item.iItem	 = i;
                    item.iImage = PSEARCHFILEINFO(data)->Flags;
                    item.lParam = (LPARAM)res;
                    */
                    auto last = ListView_InsertItem(hListView, &item);

                    //swprintf(tmp,L"%u",i);
                //	ListView_SetItemText(hListView,last,1, LPSTR_TEXTCALLBACK);

                    hit++;
                    if (results.size() >= MAX_RESULTS_NUMBER)
                    {
                        //int res;
                        //res = MessageBox(0, TEXT("Your search produces too many results!\nContinue your search?"), 0, MB_ICONINFORMATION | MB_TASKMODAL | MB_YESNO);
                        MessageBox(hWnd, szTooMany/*TEXT("Your search produces too many results!")*/, nullptr, MB_ICONWARNING | MB_OK);
                        //if (res!=IDYES)
                        //{
                            //SendMessage(hListView, WM_SETREDRAW,TRUE,0);						
                        break;
                        //}
                    }
                }
            }

        }
        //data+=disk->IsLong;
    }

    //qsort(result, hit, sizeof(SearchResult),

    //SendMessage(hListView, WM_SETREDRAW,TRUE,0);
    return hit;
}
UINT ExecuteFile(HWND hWnd, LPWSTR str, USHORT flags)
{
    UINT res;
    SHELLEXECUTEINFO shell;

    memset(&shell, 0, sizeof(SHELLEXECUTEINFO));
    shell.cbSize = sizeof(SHELLEXECUTEINFO);
    shell.lpFile = str;
    shell.fMask = SEE_MASK_INVOKEIDLIST;
    shell.lpVerb = nullptr;
    shell.nShow = SW_SHOWDEFAULT;


    if ((flags & 0x001) == 0)
    {
        MessageBox(hWnd, szDeletedFile /*TEXT("The file is deleted and cannot be accessed throught the filesystem driver.\nUse a recover program to get access to the stored data.")*/, nullptr, MB_ICONWARNING);
        return 0;
    }

    //res = (UINT) ShellExecute(0,NULL, str, NULL, NULL, SW_SHOWDEFAULT);
    ShellExecuteEx(&shell);
    res = (UINT)shell.hInstApp;
    switch (res)
    {
    case SE_ERR_NOASSOC:
        ShellExecute(nullptr, TEXT("openas"), str, nullptr, nullptr, SW_SHOWDEFAULT);
        break;
    case SE_ERR_ASSOCINCOMPLETE:

        break;
    case SE_ERR_ACCESSDENIED:
        MessageBox(hWnd, szAccessDenied, nullptr, MB_ICONERROR);
        break;
    case ERROR_PATH_NOT_FOUND:
        //MessageBox(hWnd, TEXT("The path coulnd't be found.\nPropably the path is hidden."), 0, MB_ICONWARNING);
        break;
    case ERROR_FILE_NOT_FOUND:
        //MessageBox(hWnd, szFNF/*TEXT("The file coulnd't be found.\nThe file is propably hidden or a metafile.")*/, 0, MB_ICONERROR);
        break;
    case ERROR_BAD_FORMAT:
        //MessageBox(hWnd, TEXT("This is not a valid Win32 Executable File."), 0, MB_ICONINFORMATION);
        break;
    case SE_ERR_DLLNOTFOUND:

        break;
    default:
        //if (res>32)

        break;
    }

    return res;
}

BOOL UnloadDisk(HWND hWnd, int index)
{
    //PUCHAR data;

    if (disks[index] != nullptr)
    {
        /*data = (PUCHAR) disks[index]->sFiles;
        for (int i=0;i<disks[index]->filesSize;i++)
        {
            if (PSEARCHFILEINFO(data)->FileName !=NULL)
                delete PSEARCHFILEINFO(data)->FileName;
            data +=disks[index]->IsLong;
        }*/
        CloseDisk(disks[index]);
        disks[index] = nullptr;
        SendMessage(hWnd, WM_USER + 2, 0, 0);
        return TRUE;
    }
    return FALSE;
}

UINT ExecuteFileEx(HWND hWnd, const LPTSTR command, LPWSTR str, LPCTSTR dir, UINT show, USHORT flags)
{
    UINT res;
    SHELLEXECUTEINFO shell;

    memset(&shell, 0, sizeof(SHELLEXECUTEINFO));
    shell.cbSize = sizeof(SHELLEXECUTEINFO);
    shell.lpFile = str;
    shell.fMask = SEE_MASK_INVOKEIDLIST;
    shell.lpVerb = command;
    shell.nShow = show;
    shell.lpDirectory = dir;

    if ((flags & 0x001) == 0)
    {
        MessageBox(hWnd, szDeletedFile/*TEXT("The file is deleted and cannot be accessed throught the filesystem driver.\nUse a recover program to get access to the stored data.")*/, nullptr, MB_ICONWARNING);
        return 0;
    }

    //res = (UINT) ShellExecute(0,NULL, str, NULL, NULL, SW_SHOWDEFAULT);
    ShellExecuteEx(&shell);
    res = (UINT)shell.hInstApp;
    switch (res)
    {
    case SE_ERR_NOASSOC:
        ShellExecute(nullptr, TEXT("openas"), str, nullptr, nullptr, SW_SHOWDEFAULT);
        break;
    case SE_ERR_ASSOCINCOMPLETE:

        break;
    case SE_ERR_ACCESSDENIED:
        MessageBox(hWnd, szAccessDenied, nullptr, MB_ICONERROR);
        break;
    case ERROR_PATH_NOT_FOUND:
        //MessageBox(hWnd, TEXT("The path coulnd't be found.\nPropably the path is hidden."), 0, MB_ICONWARNING);
        break;
    case ERROR_FILE_NOT_FOUND:
        //MessageBox(hWnd, TEXT("The file coulnd't be found.\nThe file is propably hidden or a metafile."), 0, MB_ICONERROR);
        break;
    case ERROR_BAD_FORMAT:
        //MessageBox(hWnd, TEXT("This is not a valid Win32 Executable File."), 0, MB_ICONINFORMATION);
        break;
    case SE_ERR_DLLNOTFOUND:

        break;
    default:
        //if (res>32)

        break;
    }

    return res;
}

/*BOOL SearchString(LPWSTR string, int length, LPWSTR pattern, int len)
{
    int p=0, s=0;
    if (length < len)
        return FALSE;

    return TRUE;
}
*/
int filecompare(const void *arg1, const void *arg2)
{
    /* Compare all of both strings: */
    return _wcsicmp(PSearchResult(arg1)->filename, PSearchResult(arg2)->filename);
}

int pathcompare(const void *arg1, const void *arg2)
{
    /* Compare all of both strings: */
    return _wcsicmp(PSearchResult(arg1)->path, PSearchResult(arg2)->path);
}

int extcompare(const void *arg1, const void *arg2)
{
    /* Compare all of both strings: */
    return _wcsicmp(PSearchResult(arg1)->extra, PSearchResult(arg2)->extra);
}

BOOL SaveResults(LPWSTR filename)
{
    HANDLE file;
    bool error = false;
    DWORD written;
    TCHAR buff[0xffff];
    file = CreateFile(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    for (int i = 0; i < results.size(); i++)
    {
        wcscpy(buff, results[i].path);
        wcscat(buff, results[i].filename);
        wcscat(buff, TEXT("\r\n"));
        if (WriteFile(file, buff, wcslen(buff) * sizeof(TCHAR), &written, nullptr) != TRUE) {
            error = true;
        }
    }

    CloseHandle(file);
    if (error) {
        return FALSE;
    }
    return TRUE;
}

BOOL ProcessPopupMenu(HWND hWnd, int index, DWORD item)
{
    TCHAR path[0xffff];
    int len;
    wcscpy(path, results[index].path);
    len = wcslen(path);
    path[len] = 0;

    switch (item)
    {
    case IDM_OPEN:
        wcscpy(&path[len], results[index].filename);
        len += wcslen(path);
        path[len] = 0;
        ExecuteFile(hWnd, path, results[index].icon);
        break;
    case IDM_OPENMIN:
        wcscpy(&path[len], results[index].filename);
        len += wcslen(path);
        path[len] = 0;
        ExecuteFileEx(hWnd, nullptr, path, results[index].path, SW_MINIMIZE, results[index].icon);

        break;
    case IDM_OPENWITH:
        wcscpy(&path[len], results[index].filename);
        len += wcslen(path);
        path[len] = 0;
        ExecuteFileEx(hWnd, TEXT("openas"), path, results[index].path, SW_SHOWDEFAULT, results[index].icon);

        break;
    case IDM_OPENDIR:
        ExecuteFileEx(hWnd, TEXT("explore"), path, results[index].path, SW_SHOWDEFAULT, results[index].icon);
        break;
    case ID_CMDPROMPT:

        ExecuteFileEx(hWnd, TEXT("open"), TEXT("cmd.exe"), results[index].path, SW_SHOWDEFAULT, results[index].icon);
        break;
    case IDM_PROPERTIES:
        wcscpy(&path[len], results[index].filename);
        len += wcslen(path);
        path[len] = 0;
        ExecuteFileEx(hWnd, TEXT("properties"), path, results[index].path, SW_SHOWDEFAULT, results[index].icon);

        break;
    case ID_COPY:
        PrepareCopy(hWnd, FILES);
        break;
    case ID_COPY_NAMES:
        PrepareCopy(hWnd, FILENAMES);
        break;
    case ID_DELETE:
        DeleteFiles(hWnd, DELETENOW);
        break;
    case ID_DELETE_ON_REBOOT:
        DeleteFiles(hWnd, DELETEONREBOOT);
        break;
    default:

        break;
    }

    return TRUE;
}

void PrepareCopy(HWND hWnd, UINT flags)
{
    DWORD datasize = 0;
    //STGMEDIUM stg;
    DROPFILES files;
    int structsize;

    switch (flags)
    {
    case FILES:
        structsize = sizeof(files);
        break;
    case FILENAMES:
    default:
        structsize = 0;
        break;
    }

    /*stg.tymed = TYMED_HGLOBAL;
    stg.hGlobal = NULL;

*/
    files.fNC = TRUE;
    files.pt.x = 0;
    files.pt.y = 0;
    files.pFiles = sizeof(files);
    files.fWide = TRUE;

    auto buff = std::make_unique<TCHAR[]>(0x80000);
    for (int i = 0; i < results.size(); i++)
    {
        UINT mask = ListView_GetItemState(hListView, i, LVIS_SELECTED);
        if (((mask & LVIS_SELECTED) != 0u) && ((results[i].icon & 0x001) != 0))
        {
            wcscpy(&buff[datasize], results[i].path);
            int len = wcslen(&buff[datasize]);
            wcscpy(&buff[datasize + len], results[i].filename);
            len += wcslen(&buff[datasize + len]);

            if (flags == FILES) {
                buff[datasize + len] = 0;
            }
            else
            {
                buff[datasize + len] = L'\r';
                buff[datasize + len + 1] = L'\n';
                datasize += 1;
            }
            datasize += len + 1;
            if (datasize > 0x7fff0) {
                break;
            }
        }
    }
    buff[datasize] = 0;
    buff[datasize++] = 0;

    //stg.hGlobal = GlobalAlloc(0, sizeof(files) + datasize*sizeof(TCHAR));


    HANDLE hdrop = GlobalAlloc(0, /*sizeof(stg) + */structsize + datasize * sizeof(TCHAR));


    //ptr  = GlobalLock(hdrop);
    //CopyMemory(ptr, &stg, sizeof(STGMEDIUM));
    //GlobalUnlock(hdrop);

    auto ptr = GlobalLock(hdrop);
    CopyMemory(ptr, &files, structsize);
    CopyMemory(PUCHAR(ptr) + structsize, buff.get(), datasize * sizeof(TCHAR));
    GlobalUnlock(hdrop);

    if (OpenClipboard(hWnd) != 0)
    {
        if (EmptyClipboard() != 0)
        {
            if (flags == FILES)
            {
                if (SetClipboardData(CF_HDROP, hdrop) == nullptr)
                {
                    //GlobalFree(stg.hGlobal);
                    GlobalFree(hdrop);
                }
            }
            else
            {
                if (SetClipboardData(CF_UNICODETEXT, hdrop) == nullptr)
                {
                    GlobalFree(hdrop);
                }
            }
        }
        else
        {
            //GlobalFree(stg.hGlobal);
            GlobalFree(hdrop);
        }

        CloseClipboard();
    }
    else
    {
        //GlobalFree(stg.hGlobal);
        GlobalFree(hdrop);
    }
}

void DeleteFiles(HWND hWnd, UINT flags)
{
    DWORD res;
    UINT mask;
    int len;
    TCHAR buff[0x8000];
    TCHAR path[0x8000];

    if (flags == DELETENOW)
    {
        for (int i = 0; i < results.size(); i++)
        {
            mask = ListView_GetItemState(hListView, i, LVIS_SELECTED);
            if (((mask & LVIS_SELECTED) != 0u) && ((results[i].icon & 0x001) != 0))
            {
                wcscpy(path, results[i].path);
                len = wcslen(path);
                wcscpy(&path[len], results[i].filename);
                len += wcslen(&path[len]);
                path[len] = 0;

                wsprintf(buff, szDelete/*TEXT("Are you sure you want to delete\n\n%.1024s\n\nfrom disk?\nYou can't restore this file!")*/, path);
                res = MessageBox(hWnd, buff, szWarning/*TEXT("WARNING")*/, MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON2);
                if (res == IDYES)
                {
                    if (!DeleteFile(path))
                    {
                        ShowError();
                    }
                }
                else if (res == IDCANCEL) {
                    break;
                }
            }
        }
    }
    else if (flags == DELETEONREBOOT)
    {
        for (int i = 0; i < results.size(); i++)
        {
            mask = ListView_GetItemState(hListView, i, LVIS_SELECTED);
            if (((mask & LVIS_SELECTED) != 0u) && ((results[i].icon & 0x001) != 0))
            {
                wcscpy(path, results[i].path);
                len = wcslen(path);
                wcscpy(&path[len], results[i].filename);
                len += wcslen(&path[len]);
                path[len] = 0;

                wsprintf(buff, szDeleteOnReboot/*TEXT("Warning!!!\nThis is very dangerous - you try to delete a file on reboot - this may cause damage to the system.\nYour system might be unaccessable afterwards.\n\nAre you sure you want to delete\n\n%.1024s\n\nfrom disk?\nYou can't restore this file!")*/, path);
                res = MessageBox(hWnd, buff, szWarning/*TEXT("WARNING")*/, MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON2);
                if (res == IDYES)
                {
                    if (!MoveFileEx(path, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT))
                    {
                        ShowError();
                    }
                }
                else if (res == IDCANCEL) {
                    break;
                }
            }
        }

    }
    else
    {

    }
}

DWORD GetDllVersion(LPCTSTR lpszDllName)
{
    HINSTANCE hinstDll;
    DWORD dwVersion = 0;

    /* For security purposes, LoadLibrary should be provided with a
       fully-qualified path to the DLL. The lpszDllName variable should be
       tested to ensure that it is a fully qualified path before it is used. */
    hinstDll = LoadLibrary(lpszDllName);

    if (hinstDll != nullptr)
    {
        DLLGETVERSIONPROC pDllGetVersion;
        pDllGetVersion = reinterpret_cast<DLLGETVERSIONPROC>(GetProcAddress(hinstDll,
            "DllGetVersion"));

        /* Because some DLLs might not implement this function, you
        must test for it explicitly. Depending on the particular
        DLL, the lack of a DllGetVersion function can be a useful
        indicator of the version. */

        if (pDllGetVersion != nullptr)
        {
            DLLVERSIONINFO dvi;
            HRESULT hr;

            ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);

            hr = (*pDllGetVersion)(&dvi);

            if (SUCCEEDED(hr))
            {
                dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
            }
        }

        FreeLibrary(hinstDll);
    }
    return dwVersion;
}

DWORD ShowError()
{
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    //LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&lpMsgBuf),
        0, nullptr);
    MessageBox(nullptr, static_cast<LPCTSTR>(lpMsgBuf), nullptr, MB_OK | MB_ICONINFORMATION);

    LocalFree(lpMsgBuf);
    return dw;
}

int ProcessLoading(HWND hWnd, HWND hCombo, int reload)
{
    EnableWindow(GetDlgItem(hWnd, IDOK), TRUE);
    int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
    int data = SendMessage(hCombo, CB_GETITEMDATA, index, 0);

    if (data > 0 && data < 32)
    {
        int b = SendDlgItemMessage(hWnd, IDC_LOADALWAYS, BM_GETCHECK, 0, 0);

        if (disks[data] == nullptr)
        {
            disks[data] = OpenDisk(0x41 + data);
            if (disks[data] != nullptr)
            {
                //LoadMFT(disks[data], FALSE);
                StartLoading(disks[data], hWnd);
            }
            else
            {
                MessageBox(hWnd, szDiskError/*TEXT("The disk couldn't be opened.\nYOU HAVE TO BE ADMINISTRATOR TO USE THIS TOOL.")*/, nullptr, MB_ICONINFORMATION);
                // set different icon
            }
        }
        else if (b == BST_CHECKED || reload > 0)
        {
            StartLoading(disks[data], hWnd);
        }

    }
    else if (data == 0xfe)
    {
        DWORD drives;
        drives = GetLogicalDrives();

        for (int i = 0; i < 32; i++)
        {
            if (((drives >> (i)) & 0x1) != 0u)
            {
                TCHAR str[5];
                UINT type;

                wsprintf(str, TEXT("%C:\\"), 0x41 + i);
                type = GetDriveType(str);
                if (type == DRIVE_FIXED)
                {
                    if (disks[i] == nullptr)
                    {
                        disks[i] = OpenDisk(0x41 + i);
                        if (disks[i] != nullptr)
                        {
                            StartLoading(disks[i], hWnd);
                        }
                        else
                        {
                            // set different icon
                        }
                    }
                }
            }

        }

        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

    }
    else if (reload > 0)
    {
        for (auto & disk : disks)
        {
            if (disk != nullptr)
            {
                StartLoading(disk, hWnd);
            }
        }
    }
    SetFocus(GetDlgItem(hWnd, IDC_EDIT));
    return 0;
}
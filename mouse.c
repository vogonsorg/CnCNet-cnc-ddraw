/*
 * Copyright (c) 2010 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* This is a special mouse coordinate fix for games that use GetCursorPos and expect to be in fullscreen */

#include <windows.h>
#include <stdio.h>
#include "main.h"
#include "surface.h"

#define MAX_HOOKS 16

BOOL mouse_active = FALSE;

struct hook { char name[32]; void *func; };
struct hack
{
    char name[32];
    struct hook hooks[MAX_HOOKS];
};

BOOL WINAPI fake_GetCursorPos(LPPOINT lpPoint)
{
    lpPoint->x = (int)ddraw->cursor.x;
    lpPoint->y = (int)ddraw->cursor.y;
    return TRUE;
}

BOOL WINAPI fake_ClipCursor(const RECT *lpRect)
{
    return TRUE;
}

int WINAPI fake_ShowCursor(BOOL bShow)
{
    return TRUE;
}

HCURSOR WINAPI fake_SetCursor(HCURSOR hCursor)
{
    return NULL;
}

struct hack hacks[] =
{
    {
        "user32.dll",
        {
            { "GetCursorPos", fake_GetCursorPos },
            { "ClipCursor", fake_ClipCursor },
            { "ShowCursor", fake_ShowCursor },
            { "SetCursor", fake_SetCursor } ,
            { "", NULL }
        }
    },
    {
        "",
        {
            { "", NULL }
        }
    }
};

void hack_iat(struct hack *hck)
{
    int i;
    char buf[32];
    struct hook *hk;
    DWORD tmp;
    HANDLE hProcess;
    DWORD dwWritten;
    IMAGE_DOS_HEADER dos_hdr;
    IMAGE_NT_HEADERS nt_hdr;
    IMAGE_IMPORT_DESCRIPTOR *dir;
    IMAGE_THUNK_DATA thunk;
    PDWORD ptmp;

    GetWindowThreadProcessId(ddraw->hWnd, &tmp);

    HMODULE base = GetModuleHandle(NULL);
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, tmp);

    ReadProcessMemory(hProcess, (void *)base, &dos_hdr, sizeof(IMAGE_DOS_HEADER), &dwWritten);
    ReadProcessMemory(hProcess, (void *)base+dos_hdr.e_lfanew, &nt_hdr, sizeof(IMAGE_NT_HEADERS), &dwWritten);
    dir = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (nt_hdr.OptionalHeader.DataDirectory[1].Size));
    ReadProcessMemory(hProcess, (void *)base+nt_hdr.OptionalHeader.DataDirectory[1].VirtualAddress, dir, nt_hdr.OptionalHeader.DataDirectory[1].Size, &dwWritten);

    while(dir->Name > 0)
    {
        memset(buf, 0, 32);
        ReadProcessMemory(hProcess, (void *)base+dir->Name, buf, 32, &dwWritten);
        if(stricmp(buf, hck->name) == 0)
        {
            ptmp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DWORD) * 64);
            ReadProcessMemory(hProcess, (void *)base+dir->Characteristics, ptmp, sizeof(DWORD) * 64, &dwWritten);
            i=0;
            while(*ptmp)
            {
                memset(buf, 0, 32);
                ReadProcessMemory(hProcess, (void *)base+(*ptmp)+2, buf, 32, &dwWritten);

                hk = &hck->hooks[0];
                while(hk->func)
                {
                    if(stricmp(hk->name, buf) == 0)
                    {
                        thunk.u1.Function = (DWORD)hk->func;
                        thunk.u1.Ordinal = (DWORD)hk->func;
                        thunk.u1.AddressOfData = (DWORD)hk->func;
                        WriteProcessMemory(hProcess, (void *)base+dir->FirstThunk+(sizeof(IMAGE_THUNK_DATA) * i), &thunk, sizeof(IMAGE_THUNK_DATA), &dwWritten);
                        mouse_active = TRUE;
                    }
                    hk++;
                }

                ptmp++;
                i++;
            }
        }
        dir++;
    }

    CloseHandle(hProcess);
}

void mouse_lock()
{
    if(mouse_active && !ddraw->locked)
    {
        ddraw->locked = TRUE;
        SetCursorPos(ddraw->center.x, ddraw->center.y);
        ClipCursor(&ddraw->cursorclip);
        while(ShowCursor(FALSE) > 0);
    }
}

void mouse_unlock()
{
    if(!mouse_active)
    {
        return;
    }

    if(ddraw->locked)
    {
        while(ShowCursor(TRUE) < 0);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }

    ddraw->locked = FALSE;
    ClipCursor(NULL);
    ddraw->cursor.x = ddraw->width / 2;
    ddraw->cursor.y = ddraw->height / 2;
}

void mouse_init(HWND hWnd)
{
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    hack_iat(&hacks[0]);
    mouse_active = TRUE;
}
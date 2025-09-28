#include <assert.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <sstream>
#include <iostream>
#include <DSound.h>
#include <dinput.h>
#include <stdbool.h>
#include <commctrl.h>
#include "bitmap.hpp"
#include "emulator.hpp"

#ifndef UNICODE
#define UNICODE
#endif


enum
{
    MENU_ID_OPEN,
    MENU_ID_SETTINGS
};




static bool running = true;
static Emulator* emu;

static bool emulator_prev_running = true; //used for polished state switching for actions like
//                                          opening the settings menu
//                                          not part of Emulator because it has no use there.??

static const int BPP = 4;

static const unsigned int TICKS_PER_SECOND = 60; // frame rate per second
static const unsigned int SKIP_TICKS = 1000 / TICKS_PER_SECOND;
static const unsigned int MAX_FRAMESKIP = 1;

static const int EMULATOR_STATUS_TEXT_Y = 1; //Y pos

static HWND window_handle = nullptr;
static HWND settings_handle = nullptr;
static std::array<HWND, EMULATOR_KEY_COUNT> hotkeys = {nullptr};

static BITMAPINFO bitmap_info;

static IDirectSound8* ds_object = nullptr;
static IDirectSoundBuffer* ds_buffer = nullptr;

static DSBUFFERDESC ds_buffer_desc = {0};
static WAVEFORMATEX ds_audio_format = {0};

static HMENU menu = nullptr;


static const int SETTINGS_WINDOW_WIDTH = 350;
static const int SETTINGS_WINDOW_HEIGHT = 400;



LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam);

static int win32_init(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PWSTR pCmdLine);

static int win32_init_directsound();

static void win32_loop();

static void win32_destroy();

static void win32_handle_menu_command(WPARAM w_param);

static void win32_resize_bitmap(int width, int height);

static void win32_draw_bitmap();

static int win32_keycode_to_emulator_keycode(int code);

static void win32_fill_sound_buffer_with_square_wave();

static void win32_set_emulator_state(bool new_running);

static void win32_activate_window(HWND window, bool activate);

static void win32_create_hotkey_control(HWND parent, int index, int x, int y, int w, int* h);

static HWND win32_create_label(HWND parent, int x, int y,
    int w, int h, const wchar_t* text);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PWSTR pCmdLine, int nCmdShow)
{
    emu = new Emulator();
    emu->Init();
   
    int code = win32_init(hInstance, hPrevInstance, pCmdLine);
    if (code)
    {
        ShowWindow(window_handle, SW_SHOWNORMAL);

        char buffer[64] = {0};
        snprintf(buffer, sizeof(buffer) / sizeof(char),
        "Initialization ERROR.\nApp Code: %d", code);

        wchar_t wbuffer[128] = {0};
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, buffer, -1, wbuffer,
        64);

        MessageBox(NULL, wbuffer, L"ERROR", MB_ICONERROR | MB_OK);

        DestroyWindow(window_handle);
        return code;
    }
    ShowWindow(window_handle, SW_SHOWNORMAL);

    win32_loop();

//    win32_destroy();

//    delete emu;

    return EXIT_SUCCESS;
}


static void win32_loop()
{
    MSG msg = {0};

    timeBeginPeriod(1);

    win32_draw_bitmap();

    while (running)
    {
        if (emu->running == false) 
        {
            std::string text = "PAUSED";
            TextOutA(GetDC(window_handle), 0,
            EMULATOR_STATUS_TEXT_Y, text.c_str(), (int)(text.length()));               
        }

        while (PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        {
            emu->Update();
            if (emu->should_draw_this_frame)
            {
                win32_draw_bitmap();
            }

            if (emu->running)
            {
                win32_fill_sound_buffer_with_square_wave();
            }

            if (emu->sound_state == SOUND_STATE_PLAY)
            {
                ds_buffer->Play(0, 0, DSBPLAY_LOOPING);
            }
            else if (emu->sound_state == SOUND_STATE_STOP)
            {
                ds_buffer->Stop();
            }

            emu->LateUpdate();
        }

        Sleep(16);
    }
}


static int win32_init(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
    PWSTR /*pCmdLine*/)
{
//    MessageBox(NULL, L"TEST", L"TEST", MB_OK);
    AllocConsole();

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN$", "r", stdin);

    enum
    {
        ERROR_NONE,
        ERROR_WINDOW_CREATION,
        ERROR_DIRECTSOUND_INIT,
        ERROR_MENU_CREATION
    };

    {
    WNDCLASS wc = {0};
    wc.lpszClassName = L"SettingsWC";
    wc.lpfnWndProc = SettingsWindowProc;
    wc.hInstance = hInstance;

    RegisterClass(&wc);
    }

    {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WC";

    RegisterClass(&wc);

    window_handle = CreateWindow(wc.lpszClassName, L"CHIP-8", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT,
    1024, 512,
    NULL, NULL, wc.hInstance, NULL);
    }

    if (window_handle == NULL)
    {
        return ERROR_WINDOW_CREATION;
    }


    //Create settings windows and controls
/*    {
    WNDCLASS wc = {0};
    if (GetClassInfo(GetModuleHandle(NULL),
    L"SettingsWC", &wc))
    {
        settings_handle = CreateWindow(wc.lpszClassName,
        L"Settings",
        WS_POPUP | WS_DISABLED |
        WS_BORDER | WS_OVERLAPPEDWINDOW,
        0, 0,
        500, 200,
        window_handle, NULL, wc.hInstance, NULL);
    }
    }

    if (settings_handle == nullptr)
    {
        printf("WARNING: Creating Settings Window Failed.\n");
    }*/

    INITCOMMONCONTROLSEX icex;  //declare an INITCOMMONCONTROLSEX Structure
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_HOTKEY_CLASS;   //set dwICC member to ICC_HOTKEY_CLASS    
                                    // this loads the Hot Key control class.
    if (InitCommonControlsEx(&icex) == FALSE)
    {
        //TODO(omar): Should this be an error ? or be logged alongside GetLastError ?
        printf("WARNING: Loading Hotkey Control Failed. Keybinding will not be available.");
    }
    else
    {

/*        hotkeys = CreateWindowEx(WS_DISABLED0,       // no extended styles 
                                HOTKEY_CLASS,             // class name 
                                TEXT(""),                 // no title (caption) 
                                WS_CHILD | WS_VISIBLE,    // style 
                                15, 10,                   // position 
                                200, 20,                  // size 
                                settings_handle,          // parent window 
                                NULL,                     // uses class menu 
                                GetModuleHandle(nullptr), // instance 
                                NULL);                    // no WM_CREATE parameter */
    }

    if (SetTextAlign(GetDC(window_handle), TA_LEFT) == GDI_ERROR)
    {
        printf("WARNING: Setting window text alignment failed.\n");
    }

    int code = win32_init_directsound();
    if (code)
    {
        return ERROR_DIRECTSOUND_INIT;
    }

    //Menu
    menu = CreateMenu();
    if (menu == nullptr)
    {
        return ERROR_MENU_CREATION; 
    }

    if (AppendMenu(menu, MF_STRING, MENU_ID_OPEN, TEXT("Open")) == 0)
    {
        return ERROR_MENU_CREATION;
    }
    if (AppendMenu(menu, MF_STRING, MENU_ID_SETTINGS, TEXT("Settings")) == 0)
    {
        return ERROR_MENU_CREATION;
    }

    SetMenu(window_handle, menu);

    return ERROR_NONE;
}


static int win32_init_directsound()
{
    ds_audio_format.wFormatTag = WAVE_FORMAT_PCM;
    ds_audio_format.nChannels = 1;
    ds_audio_format.nSamplesPerSec = 44100;
    ds_audio_format.wBitsPerSample = 16;
    ds_audio_format.nBlockAlign = ds_audio_format.nChannels * ds_audio_format.wBitsPerSample / 8;
    ds_audio_format.nAvgBytesPerSec = ds_audio_format.nSamplesPerSec * ds_audio_format.nBlockAlign;

    ds_buffer_desc.dwSize = sizeof(DSBUFFERDESC);
//        ds_buffer_desc.dwFlags = DSBCAPS_LOCSOFTWARE;
    ds_buffer_desc.dwBufferBytes = ds_audio_format.nSamplesPerSec * sizeof(int16_t);
    ds_buffer_desc.lpwfxFormat = &ds_audio_format;
    ds_buffer_desc.guid3DAlgorithm = GUID_NULL;

    int err_code = DirectSoundCreate8(nullptr, &ds_object, NULL);
    if (err_code == DS_OK)
    {
        err_code = ds_object->CreateSoundBuffer(&ds_buffer_desc, &ds_buffer, nullptr);

        if (err_code != DS_OK)
        {
            printf("Creating secondary sound buffer failed. Code: %x\n", err_code);
            return 1;
        }

        ds_object->SetCooperativeLevel(window_handle, DSSCL_PRIORITY);
    }
    else
    {
        printf("Creating DirectSound object failed. Code: %x\n", err_code);
        return 2;
    }

    return 0;
}


static void win32_destroy()
{
    VirtualFree(emu->bitmap.data, 0, MEM_RELEASE);
    DestroyWindow(window_handle);

    ds_object->Release();
    ds_buffer->Release();

    delete emu;
}


static void win32_handle_menu_command(WPARAM w_param)
{
    switch (w_param)
    {
        case MENU_ID_OPEN:
        {
            std::string text = "PAUSED";
            TextOutA(GetDC(window_handle), 0,
            EMULATOR_STATUS_TEXT_Y, text.c_str(), (int)(text.length()));

            OPENFILENAME ofn = {0};
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = window_handle;
            ofn.nMaxFile = 512;
            wchar_t filepath[256] = {0};
            ofn.lpstrFile = filepath;
            ofn.Flags = OFN_FILEMUSTEXIST;

            GetOpenFileName(&ofn);

            emu->LoadFromFile(filepath);
            
            emu->should_draw_this_frame = true;
        } break;

        case MENU_ID_SETTINGS:
        {
            if (settings_handle) break;

            WNDCLASS wc = {0};
            if (GetClassInfo(GetModuleHandle(NULL),
            L"SettingsWC", &wc))
            {
                settings_handle = CreateWindow(wc.lpszClassName,
                L"Settings",
                WS_POPUP /*| WS_DISABLED*/ |
                WS_BORDER |
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                200, 200,
                SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT,
                window_handle, NULL, wc.hInstance, NULL);
            }

            if (settings_handle == nullptr)
            {
                printf("WARNING: Creating Settings Window Failed.\n");
            }

//            EnableWindow(settings_handle, true);
//            SendMessage(settings_handle, WM_SHOWWINDOW, 0, 0);

        } break;
    }
}


static void win32_draw_bitmap()
{
    PAINTSTRUCT ps;
    BeginPaint(window_handle, &ps);

    RECT rect;
    GetClientRect(window_handle, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    StretchDIBits(GetDC(window_handle), 0, 0,
    width, height,
    0, 0,
    emu->bitmap.w, emu->bitmap.h,
    emu->bitmap.data, &bitmap_info, DIB_RGB_COLORS,
    SRCCOPY);

    EndPaint(window_handle, &ps);
}


static void win32_resize_bitmap(int width, int height)
{
    if (emu->bitmap.data)
    {
        VirtualFree(emu->bitmap.data, 0, MEM_RELEASE);
        emu->bitmap.data = NULL;
    }

    //Create bitmap
    emu->bitmap.w = width;
    emu->bitmap.h = height;
    emu->bitmap.bpp = BPP;

    memset(&bitmap_info, 0, sizeof(BITMAPINFO));
    {
        BITMAPINFOHEADER* h = &bitmap_info.bmiHeader;
        h->biSize = sizeof(BITMAPINFOHEADER);

        h->biWidth = emu->bitmap.w;
        h->biHeight = -(emu->bitmap.h);
        h->biPlanes = 1;
        h->biBitCount = BPP * 8;
        h->biCompression = BI_RGB;
    }

    emu->bitmap.data = (char*)VirtualAlloc(NULL, emu->bitmap.w * emu->bitmap.h * BPP, MEM_COMMIT, PAGE_READWRITE);
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
        {
            win32_handle_menu_command(wParam);
        } break;

        case WM_KEYDOWN:
        {
            int code = win32_keycode_to_emulator_keycode((int)wParam);
            emu->SetKey(code, 1);
        } break;

        case WM_KEYUP:
        {
            int code = win32_keycode_to_emulator_keycode((int)wParam);
            emu->SetKey(code, 0);

            if (wParam == VK_RETURN)
            {
                win32_set_emulator_state(!(emu->running));
            }
        } break;

        case WM_SIZE:
        {
            RECT rect;
            GetClientRect(window_handle, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            win32_resize_bitmap(width, height);
            emu->should_draw_this_frame = true;
        } break;

        case WM_DESTROY:
        case WM_QUIT:
        {
            PostQuitMessage(0);
            running = false;
        } break;

        case WM_PAINT:
        {
            win32_draw_bitmap();
        } break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
        {
            HWND hotkey = (HWND)lParam;
            int index = -1;
            for (int i = 0; i < hotkeys.size(); i++)
            {
                if (hotkeys[i] == hotkey)
                {
                    index = i;
                    break;
                }
            }

            if (index == -1) break; //TODO(omar): do something. but this should never happen

            LRESULT hotKeyInfo = SendMessage(hotkey, HKM_GETHOTKEY, 0, 0);

            // Extract the virtual key code and modifier flags from the result.
            BYTE code = LOBYTE(LOWORD(hotKeyInfo));
            BYTE modifier = HIBYTE(LOWORD(hotKeyInfo));

            if (modifier)
            {
                printf("INFO: Invalid '%d' Key Selection. Modifier Keys Prohibted\n", index);
                break;
            }

            if (((code >= '0') && (code <= '9')) || ((code >= 'A') || (code <= 'Z')))
            {
                emu->keymap[index] = code;
            }
            else
            {
                printf("INFO: Invalid '%d' Key Selection. Alphabet and Numbers Only\n", index);
                break;
            }
        } break;
        case WM_CREATE:
        {
            win32_set_emulator_state(false);

            int w = (int)((double)SETTINGS_WINDOW_WIDTH * 0.8);
            int h = (int)(SETTINGS_WINDOW_HEIGHT * 0.9) / (int)(hotkeys.size());
            int x = 0;
            int y = 0;

            const int LABEL_HOTKEY_MARGIN = 0;
            for (int i = 0; i < hotkeys.size(); i++)
            {
                std::wstringstream stream;
                stream << std::hex << i;
                std::wstring text = stream.str();
                text += L": ";

                HWND label = win32_create_label(hwnd, 0, y, w, h, text.c_str());
                if (label && (x == 0))
                {
                    //Set the hotkey's position according to the label
                    RECT rect;
                    GetClientRect(label, &rect);
                    
                    x = rect.right;
                }

                win32_create_hotkey_control(hwnd, i,
                x + LABEL_HOTKEY_MARGIN, y, w, &h);

                y += h;
            }
            
            if (hotkeys[0] == nullptr)
            {
                printf("WARNING: Creating Hotkey Control Failed.\n");
                break;
            }
                
            SetFocus(hotkeys[0]);

            ShowWindow(hwnd, SW_RESTORE);
        } break;

        case WM_CTLCOLORSTATIC:
        {
            HDC label_hdc = (HDC)wParam;
            SetBkMode(label_hdc, TRANSPARENT);
            return (INT_PTR)GetStockObject(HOLLOW_BRUSH);
        } break;

        case WM_CLOSE:
        {
            win32_set_emulator_state(emulator_prev_running);

/*            for (int i = 0; i < hotkeys.size(); i++)
            {
                DestroyWindow(hotkeys[i]);
                hotkeys[i] = nullptr;
            }*/
        } break;

        case WM_NCDESTROY:
        {
            DefWindowProc(hwnd, uMsg, wParam, lParam);
            settings_handle = nullptr;
            return 0;
        } break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


#define IS_CHAR(x) ('A' <= x) && ('Z' >= x)
#define IS_DIGIT(x) ('0' <= x) && ('9' >= x)


static int win32_keycode_to_emulator_keycode(int code)
{
    return code;
}


static void win32_fill_sound_buffer_with_square_wave()
{
    const int hz = 600;
    const int square_wave_period = ds_audio_format.nSamplesPerSec / hz;
    const int bytes_per_sample = ds_audio_format.wBitsPerSample / 8;
    const int buffer_size = ds_buffer_desc.dwBufferBytes;
    int16_t square_wave_volume = 200;
    static uint32_t sample_index = 0;

    if (ds_buffer)
    {
        int err_code;

        uint32_t write_cursor;
        uint32_t play_cursor;

        err_code = ds_buffer->GetCurrentPosition((LPDWORD)&play_cursor, (LPDWORD)&write_cursor);
//            printf("%u\n", play_cursor);
        if (err_code == DS_OK)
        {
            char* region1;
            unsigned long region1_size;
            char* region2;
            unsigned long region2_size;

            uint32_t lock_start_byte = (sample_index * bytes_per_sample) % buffer_size;
            uint32_t write_size = 0; //amount of bytes to write

            if (lock_start_byte > play_cursor)
            {
                write_size = (buffer_size - lock_start_byte) + play_cursor;
            }
            else
            {
                write_size = play_cursor - lock_start_byte;
                if (write_size == 0)
                {
                    //edge case for when play_cursor is 0. fill an arbitrary "start" size
                    write_size = bytes_per_sample;
                }
            }

            err_code = ds_buffer->Lock(lock_start_byte, write_size,
            (void**)(&region1), &region1_size,
            (void**)(&region2), &region2_size,
            0);

            if (err_code == DS_OK)
            {
                if (region1)
                {
                    int16_t* sample = (int16_t*)region1;
                    int sample_count = region1_size / bytes_per_sample;

                    for (int i = 0; i < sample_count; i++)
                    {
                        int16_t sample_val = square_wave_volume;
                        if (((sample_index / (square_wave_period / 2)) % 2) == 0)
                        {
                            sample_val = -sample_val;
                        }

                        *sample = sample_val;
                        sample++;
                        sample_index++;
                    }
                }

                if (region2)
                {
                    int16_t* sample = (int16_t*)region2;
                    int sample_count = region2_size / bytes_per_sample;

                    for (int i = 0; i < sample_count; i++)
                    {
                        int16_t sample_val = square_wave_volume;
                        if (((sample_index / (square_wave_period / 2)) % 2) == 0)
                        {
                            sample_val = -sample_val;
                        }

                        *sample = sample_val;
                        sample++;
                        sample_index++;
                    }
                }

                err_code = ds_buffer->Unlock(region1, region1_size, region2, region2_size);

                if (err_code != DS_OK)
                {
                    printf("Unlocking sound buffer failed. Code: %x\n", err_code);
                    assert(false);
                }
            }
            else
            {
                printf("Locking sound buffer failed, Code: %x\n", err_code);
                assert(false);
            }
        }
        else
        {
            printf("Getting sound buffer cursors failed, Code: %x\n", err_code);
            assert(false);
        }
    }

}


//TODO(omar): This could be moved to emulator.cpp and be platform independent
static void win32_set_emulator_state(bool new_running)
{
    emulator_prev_running = emu->running;
    emu->running = new_running;
    if ((emu->running == false) && (emulator_prev_running != false))
    {
        std::string text = "PAUSED";
        TextOutA(GetDC(window_handle), 0,
        EMULATOR_STATUS_TEXT_Y, text.c_str(), (int)(text.length()));
    }
    else if (emu->running == true)
    {
        win32_draw_bitmap(); //To clear over the text
    }
}


static void win32_activate_window(HWND window, bool activate)
{
    EnableWindow(window, activate);
}

//Height can be an in or out paramater depending on its value
static void win32_create_hotkey_control(HWND parent, int index, int x, int y, int w, int* h)
{
    if (parent == nullptr) return;
    if (index < 0) return;
    if (index >= EMULATOR_KEY_COUNT) return;

    HWND* hotkey = &(hotkeys[index]);

    *hotkey = CreateWindowEx(/*WS_DISABLED*/0,       // no extended styles 
                        HOTKEY_CLASS,             // class name 
                        TEXT(""),                 // no title (caption) 
                        WS_CHILD | WS_VISIBLE,    // style 
                        x, y,                   // position 
                        w, *h,                  // size 
                        parent,                     // parent window 
                        NULL,                     // uses class menu 
                        GetModuleHandle(nullptr), // instance 
                        NULL);                    // no WM_CREATE parameter
    
    if ((*hotkey) == nullptr) return;

    //Set hotkey height to height of 1 character
    if ((*h) == 0)
    {
        HDC hdc = GetDC(*hotkey);
        HFONT font = (HFONT)SendMessage(*hotkey, WM_GETFONT, 0, 0);
        HFONT dc_font = (HFONT)SelectObject(hdc, font);

        SIZE size;
        wchar_t text[2] = {'N', 0};
        GetTextExtentPoint32(hdc, text, lstrlenW(text), &size);
        size.cy = (LONG)((double)(size.cy) * 1.5);

        SetWindowPos(*hotkey, HWND_TOP,
        x, y, w,
        size.cy, SWP_NOZORDER);

        *h = size.cy;
    }

    //Set default hotkey from keymap
    DWORD w_param = emu->keymap[index];
    SendMessage(*hotkey, HKM_SETHOTKEY, w_param, 0);
}


static HWND win32_create_label(HWND parent, int x, int y, int w, int h, const wchar_t* text)
{
    if (parent == nullptr) return nullptr;
    if (text == nullptr) return nullptr;

    HWND label = CreateWindowEx(
        0, TEXT("STATIC"), (text),
        WS_CHILD | WS_VISIBLE,
        x, y,
        w, h,
        parent,
        NULL,
        GetModuleHandle(nullptr),
        NULL
    );

    if (label == nullptr) return nullptr;

    //Set the label's size to the text's size
    HDC hdc = GetDC(label);
    HFONT font = (HFONT)SendMessage(label, WM_GETFONT, 0, 0);
    HFONT dc_font = (HFONT)SelectObject(hdc, font);

    SIZE size;
    GetTextExtentPoint32(hdc, text, lstrlenW(text), &size);

    SetWindowPos(label, HWND_TOP,
    x, y, size.cx,
    size.cy, SWP_NOZORDER);

    return label;
}

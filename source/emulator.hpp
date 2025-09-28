#pragma once

#include "bitmap.hpp"

#include <array>

enum
{
    KEYCODE_NONE,
    KEYCODE_RETURN,
    KEYCODE_COUNT,
    KEYCODE_MAX = KEYCODE_COUNT + 27 + 10
};

#define ASCII_TO_KEYCODE(x) (KEYCODE_COUNT + (x - 'A' + 1))
#define KEYCODE_TO_ASCII(x) ((x + 'A' - 1) - KEYCODE_COUNT);

const int EMULATOR_RAM_SIZE = 4096;
const int EMULATOR_STACK_SIZE = 16;
const int EMULATOR_REGISTER_COUNT = 16;
const int EMULATOR_KEY_COUNT = 0xf + 1;

const int DISPLAY_WIDTH = 64;
const int DISPLAY_HEIGHT = 32;

const int DISPLAY_PIXEL_COUNT = DISPLAY_WIDTH * DISPLAY_HEIGHT;

const int16_t FONT_ADDRESS = 0x0;
const int FONT_CHAR_HEIGHT = 5;

const uint8_t font_data[] = {
0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
0x20, 0x60, 0x20, 0x20, 0x70, // 1
0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
0x90, 0x90, 0xF0, 0x10, 0x10, // 4
0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
0xF0, 0x10, 0x20, 0x40, 0x40, // 7
0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
0xF0, 0x90, 0xF0, 0x90, 0x90, // A
0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
0xF0, 0x80, 0x80, 0x80, 0xF0, // C
0xE0, 0x90, 0x90, 0x90, 0xE0, // D
0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


enum
{
    COMP_MODE_COSMAC = 1 << 0,
    COMP_MODE_MODERN = 1 << 1,
    COMP_MODE_AMIGA = 1 << 2 //only affects the FX1E instruction. SpaceFight 2091! relies on the behavior
};


//platform layer stuff
enum
{
    SOUND_STATE_CONTINUE, //Don't stop or play sound. whatever is happening leave it
    SOUND_STATE_PLAY, //start playing sound
    SOUND_STATE_STOP //stop playing sound
};


struct Keypad
{
    std::array<uint8_t, EMULATOR_KEY_COUNT> keys = {0}; //the chip 8's emulated keypad. from 0 to F
    bool key_just_pressed = false;
    int8_t last_key_pressed = 0; //0, 0xa, 9
};


struct Emulator
{
    bool running = false;
    int compatibility_mode = COMP_MODE_COSMAC;
    int tic = 0;

    Bitmap bitmap;

//    std::array<

    std::array<uint8_t, EMULATOR_RAM_SIZE> memory = {0};

    Keypad keypad;

    //Maps emulator keys (0 to F) to ascii characters. used for keybinds 
    //This array should be set by the platform layer
    std::array<char, EMULATOR_KEY_COUNT> keymap = {0};

    //registers
    std::array<uint8_t, EMULATOR_REGISTER_COUNT> v = {0};
    uint16_t I = 0;
    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;
    uint16_t program_counter = 0;
    uint8_t stack_pointer = 0;

    std::array<uint16_t, EMULATOR_STACK_SIZE> stack = {0};

    std::array<uint8_t, DISPLAY_PIXEL_COUNT> display = {0};
    bool get_key_key_pressed = false; //used for the 0x0A (get key) instruction


    //flags/signals for the platform layer.
    //This can be revamped to something cleaner but it works fine for this simple use case
    bool should_draw_this_frame = false;
    int sound_state = SOUND_STATE_CONTINUE;

    void Init();
    void Update();
    void Draw();
    void LateUpdate(); //Called at the very end of the frame

    void Execute();

    bool LoadFromFile(wchar_t* filename);

    void ClearDisplay();

    inline void WriteInstToMemory(uint16_t inst);

    void SetKey(int code, uint8_t state);

//    int IsCharKeyDown(char c);
};
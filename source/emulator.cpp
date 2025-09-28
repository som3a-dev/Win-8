#include "emulator.hpp"

#include <stdlib.h>
#include <fstream>
#include <filesystem>
#include <time.h>
#include <stdio.h>
#include <string>
#include <windows.h>


void Emulator::Init()
{
    srand((unsigned)time(NULL));

    memcpy(memory.data() + FONT_ADDRESS, font_data, sizeof(font_data));

    program_counter = 0x200;

    //init keymap
    for (int i = 0; i < keymap.size(); i++)
    {
        if (i < 10)
        {
            keymap[i] = i + '0';
        }
        else
        {
            keymap[i] = (i - 10) + 'A';
        }
    }
}


void Emulator::WriteInstToMemory(uint16_t inst)
{
    memory[program_counter] = (inst >> 8) & 0x00FF;
    program_counter++;
    memory[program_counter] = inst & 0x00FF;
    program_counter++;
}


bool Emulator::LoadFromFile(wchar_t* filename)
{
    std::filesystem::path path = filename;
    std::ifstream file(path, std::ios::binary);

    file.seekg(0, std::ios::end);
    long long filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file.good() && (filesize < EMULATOR_RAM_SIZE))
    {
        int address = 0x200;
        unsigned char byte;
        while (file.eof() == false)
        {
            file.read(reinterpret_cast<char*>(&byte), 1);
            memory[address] = byte;
            address++;
        }

        program_counter = 0x200;
        ClearDisplay();
        running = true; //unpause

        file.close();
        return true;
    }

    wprintf(L"Loading ROM from file '%ls' failed.\n", filename);
    file.close();

    return false;
}


void Emulator::ClearDisplay()
{
    for (int i = 0; i < DISPLAY_PIXEL_COUNT; i++)
    {
        display[i] = 0;
    }
}


void Emulator::LateUpdate()
{
    should_draw_this_frame = false;
}


void Emulator::Update()
{
    tic++;
    if (running == false)
    {
        if (should_draw_this_frame)
        {
            Draw();
        }

        sound_state = SOUND_STATE_STOP;
        return;
    }

    should_draw_this_frame = false;
    sound_state = SOUND_STATE_CONTINUE;

    if (delay_timer > 0)
    {
        delay_timer--;
    }
    if (sound_timer > 0)
    {
        sound_timer--;
        if (sound_timer == 0)
        {
            sound_state = SOUND_STATE_STOP;
        }
    }

    for (int i = 0; i < 15; i++)
    {
        if (program_counter < (EMULATOR_RAM_SIZE-2))
        {
            Execute();
        }
    }

    if (should_draw_this_frame)
    {
        Draw();
    }

/*    if ((tic % 60) == 0)
    {
        display[0] = !(display[0]);
    }*/
    
    keypad.key_just_pressed = false;
}


void Emulator::Draw()
{
    int scale_w = bitmap.w / DISPLAY_WIDTH;
    int scale_h = bitmap.h / DISPLAY_HEIGHT;

    for (int y = 0; y < DISPLAY_HEIGHT; y++)
    {
        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            uint8_t color = display[x + (y * DISPLAY_WIDTH)];

            if (color == 1)
            {
                bitmap.DrawRect(x * scale_w, y * scale_h,
                scale_w, scale_h, 160, 160, 160);
            }
            else if (color == 0)
            {
                bitmap.DrawRect(x * scale_w, y * scale_h,
                scale_w, scale_h, 30, 30, 30);
            }
        }
    }
}

#define NIBBLE(x, n_index) (x >> (n_index * 4)) & 0xF;

void Emulator::Execute()
{
    uint16_t instruction = memory[program_counter+1] | (((uint16_t)memory[program_counter]) << 8); 
    uint8_t code = NIBBLE(instruction, 3);
    uint8_t x = NIBBLE(instruction, 2);
    uint8_t y = NIBBLE(instruction, 1);
    uint8_t n = NIBBLE(instruction, 0);
    uint8_t nn = instruction & 0x00FF;
    uint16_t nnn = instruction & 0x0FFF;

    //For instructions that can use v[f] as vx or vy
    uint8_t vx = v[x];
    uint8_t vy = v[y];

    bool increment_pc = true;

    switch (code)
    {
        case 0:
        {
            switch (instruction)
            {
                case 0x00E0: //CLEAR SCREEN
                {
                    memset(display.data(), 0, sizeof(display));
                    should_draw_this_frame = true;
                } break;

                case 0x00EE: //RETURN FROM SUBROUTINE
                {
                    if (stack_pointer >= 0)
                    {
                        stack_pointer--;
                    }
                    program_counter = stack[stack_pointer];
                    stack[stack_pointer] = 0;

//                    increment_pc = false;
                } break;
            }
        } break;

        case 1: //JUMP
        {
            uint16_t address = instruction & 0x0FFF;
            program_counter = address;

            increment_pc = false;
        } break;

        case 2: //JUMP SUBROUTINE
        {
            stack[stack_pointer] = program_counter;
            stack_pointer++;
            if (stack_pointer >= EMULATOR_STACK_SIZE)
            {
                //TODO(omar): stack overflow
            }

            program_counter = nnn;

            increment_pc = false;
        } break;

        case 3: //SKIP IF EQUAL
        {
            if (v[x] == nn)
            {
                program_counter += 2; 
            }
        } break;

        case 4: //SKIP IF NOT EQUAL
        {
            if (v[x] != nn)
            {
                program_counter += 2; 
            }
        } break;

        case 5: //SKIP IF EQUAL
        {
            if (v[x] == v[y])
            {
                program_counter += 2;
            }
        } break;

        case 9: //SKIP IF NOT EQUAL
        {
            if (v[x] != v[y])
            {
                program_counter += 2;
            }
        } break;

        case 6: //SET V
        {
            v[x] = nn;
        } break;

        case 7: //ADD TO V
        {
            v[x] += nn;
        } break;

        case 0xA: //SET I
        {
            I = instruction & 0x0FFF;
        } break;

        case 0xB: //JUMP WITH OFFSET
        {
            if (compatibility_mode & COMP_MODE_COSMAC)
            {
                program_counter = nnn + v[0];
            }
            else
            {
                program_counter = (instruction & 0x0FFF) + vx;
            }

            increment_pc = false;
        } break;

        case 0xC: //RANDOM NUMBER GEN
        {
            v[x] = ((uint8_t)rand()) & (instruction & 0x00FF);
        } break;

        case 0xE: //SKIP IF KEY
        {
            int index  = (v[x] % EMULATOR_KEY_COUNT);
            if (index >= EMULATOR_KEY_COUNT) break;

            if (nn == 0x9e)
            {
                if (keypad.keys[index] == 1)
                {
                    program_counter += 2;
                }
            }
            else if (nn == 0xA1)
            {
                if (keypad.keys[index] == 0)
                {
                    program_counter += 2;
                }
            }
        } break;

        case 0xF: //WEIRD INSTRUCTION FAMILY
        {
            switch (nn)
            {
                //TIMER STUFF
                case 0x7:
                {
                    v[x] = delay_timer;
                } break;

                case 0x15:
                {
                    delay_timer = v[x];
                } break;

                case 0x18:
                {
                    sound_timer = v[x];
                    if (sound_timer)
                    {
                        sound_state = SOUND_STATE_PLAY;
                    }
                    else
                    {
                        sound_state = SOUND_STATE_STOP;
                    }
                } break;

                
                case 0x1E: //ADD TO INDEX REGISTER
                {
                    I += v[x];

                    if (compatibility_mode & COMP_MODE_AMIGA)
                    {
                        if (I > 0x1000) //overflow from addressing range
                        {
                            v[0xf] = 1;
                        }
                    }
                } break;

                case 0x0A: //Get key
                {
                    if (get_key_key_pressed)
                    {
                        uint8_t key = keypad.keys[keypad.last_key_pressed];
                        if (key != 0) //not yet released
                        {
                            increment_pc = false;
                        }
                        else
                        {
                            //instruction done
                            v[x] = keypad.last_key_pressed;
                            get_key_key_pressed = false;
                        }
                    }
                    else
                    {

                        increment_pc = false; //loop 
                        if (keypad.key_just_pressed == true)
                        {
                            get_key_key_pressed = true;
                        }
                    }
                } break;

                case 0x29: //GET FONT CHARACTER ADDRESS
                {
                    uint8_t character = (v[x] % EMULATOR_KEY_COUNT);
                    I = FONT_ADDRESS + (character * FONT_CHAR_HEIGHT);
                } break;

                case 0x33: //SPLIT NUM INTO DIGITS
                {
                    int num = v[x]; //NOTE(omar): the original COSMAC only took the last nibble
                    uint8_t digits[3] = {0};
                    int digit_count = 0;

                    while (num != 0 && (digit_count <= 2))
                    {
                        digits[digit_count] = num % 10;
                        num /= 10;
                        digit_count++;
                    }

                    int j = 0;
                    for (int i = 2; i >= 0; i--)
                    {
                        memory[(I + j) % EMULATOR_RAM_SIZE] = digits[i];
                        j++;
                    }
                } break;

                case 0x55: //STORE REGISTER TO MEMORY
                {
                    int16_t address = I;
                    for (int i = 0; i <= x; i++)
                    {
                        memory[address % EMULATOR_RAM_SIZE] = v[i];
                        address++;
                    }

                    if (compatibility_mode & COMP_MODE_COSMAC) 
                    {
                        I += x + 1; //the og cosmac incremented the I register
                    }
                } break;

                case 0x65: //LOAD REGISTER FROM MEMORY
                {
                    int16_t address = I;
                    for (int i = 0; i <= x; i++)
                    {
                        v[i] = memory[address % EMULATOR_RAM_SIZE];
                        address++;
                    }

                    if (compatibility_mode & COMP_MODE_COSMAC) 
                    {
                        I += x + 1; //the og cosmac incremented the I register
                    }
                } break;
            }
        } break;

        case 0x8: //LOGICAL/ARITHEMTIC FAMILY
        {
            switch (n)
            {
                case 0: //SET
                {
                    v[x] = v[y];
                } break;

                case 1: //OR
                {
                    v[x] |= v[y];
                    if (compatibility_mode == COMP_MODE_COSMAC)
                    {
                        v[0xf] = 0;
                    }
                } break;

                case 2: //AND
                {
                    v[x] &= v[y];
                    if (compatibility_mode == COMP_MODE_COSMAC)
                    {
                        v[0xf] = 0;
                    }
                } break;

                case 3: //XOR
                {
                    v[x] ^= v[y];
                    if (compatibility_mode == COMP_MODE_COSMAC)
                    {
                        v[0xf] = 0;
                    }
                } break;

                case 4: //ADD
                {
                    v[x] += v[y];

                    //Check carry
                    if ((vx + vy) > 255)
                    {
                        v[0xf] = 1;
                    }
                    else
                    {
                        v[0xf] = 0;
                    }
                } break;

                case 5: //SUBTRACT v[x] - v[y]
                {
                    v[x] = vx - vy;

                    if (vx >= vy)
                    {
                        v[0xf] = 1;
                    }
                    else
                    {
                        v[0xf] = 0;
                    }
                } break;

                case 7: //SUBTRACT v[y] - v[x]
                {
                    v[x] = vy - vx;

                    if (vy >= vx)
                    {
                        v[0xf] = 1;
                    }
                    else
                    {
                        v[0xf] = 0;
                    }
                } break;

                case 6: //SHIFT RIGHT
                {
                    if (compatibility_mode == COMP_MODE_COSMAC) 
                    {
                        v[x] = vy;
                    }

                    uint8_t bit = v[x] & 1;

                    v[x] >>= 1;
                    v[0xf] = bit;
                } break;

                case 0xE: //SHIFT LEFT
                {
                    if (compatibility_mode == COMP_MODE_COSMAC) 
                    {
                        v[x] = vy;
                    }

                    uint8_t bit = (v[x] & 0x80) >> 7;

                    v[x] <<= 1;
                    v[0xf] = bit;
                } break;
            }
        } break;

        case 0xD: //DRAW
        {
            uint8_t sprite_height = NIBBLE(instruction, 0);
            x = v[x] % DISPLAY_WIDTH; 
            y = v[y] % DISPLAY_HEIGHT;

            v[0xf] = 0; //Only set to 1 if any pixel gets turned off

            for (int i = I; i < (I + sprite_height); i++)
            {
                uint8_t row = memory[i % EMULATOR_RAM_SIZE];

                for (int j = 0; j < 8; j++)
                {
                    if ((row & (0x80 >> j)) != 0)
                    {
                        if ((x + j) >= DISPLAY_WIDTH)
                        {
                            break;
                        }

                        uint8_t* pixel = display.data() + ((x+j) + (y * DISPLAY_WIDTH));

                        if ((*pixel) == 1)
                        {
                            //it will be turned off. set vf to 1
                            v[0xf] = 1;
                        }
                        *pixel ^= 1; 
                    }
                }

                y++;
                if (y > DISPLAY_HEIGHT)
                {
                    break;
                }
            }

            should_draw_this_frame = true;
        } break;
    }

    if (increment_pc)
    {
        program_counter += 2;
    }
}

#define IS_KEYPAD_CHAR(x) ((x >= 'A') && (x <= 'F'))
#define IS_KEYPAD_DIGIT(x) ((x >= '0') && (x <= '9'))

//TODO(omar):
///On the original COSMAC VIP, the key was only registered when it was pressed and then released. maybe we should do that
void Emulator::SetKey(int code, uint8_t new_state)
{

/*    switch (code)
    {
        case 'W': chip8_code = '2'; break;
        case 'A': chip8_code = '4'; break;
        case 'D': chip8_code = '6'; break;
        case 'S': chip8_code = '8'; break;

        default: chip8_code = code;
    }*/

    int chip8_code = code;
    int index = -1;

    //See if the key pressed is one of the mapped out keys
    for (int i = 0; i < keymap.size(); i++)
    {
        if (code == keymap[i])
        {
            index = i;
            break;
        }
    }

    //If not. Check if its a valid key itself (0 to 9, A to F)
    //Then the EMULATOR_KEY_COUNT cap will validate A to F
    if (index == -1)
    {
        if (((code >= '0') && (code <= '9')) || ((code >= 'A') && (code <= 'F')))
        {
            char str[2] = {(char)code, 0};
            int num = std::stoi(str, nullptr, 16);

            if (num < EMULATOR_KEY_COUNT)
            {
                index = num;
            }
        }

    }

    if (index > -1)
    {
        keypad.keys[index] = new_state;
        if (new_state == 1)
        {
            keypad.key_just_pressed = true;
            keypad.last_key_pressed = (int8_t)(index);
        }
    }

/*    if (IS_KEYPAD_DIGIT(chip8_code) || IS_KEYPAD_CHAR(chip8_code))
    {
        char str[2] = {(char)chip8_code, 0};
        int index = std::stoi(str, nullptr, 16);

        if (index < EMULATOR_KEY_COUNT)
        {
            keypad.keys[index] = new_state;
            if (new_state == 1)
            {
                keypad.key_just_pressed = true;
                keypad.last_key_pressed = (int8_t)(index);
            }
        }
    }*/
}


/*int Emulator::IsCharKeyDown(char c)
{
    int code = ASCII_TO_KEYCODE(c);
    if (code >= KEYCODE_MAX)
    {
        return KEYCODE_NONE;
    }

    return keystate[ASCII_TO_KEYCODE(c)];
}
*/
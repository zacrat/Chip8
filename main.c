#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include  <SDL3/SDL.h>

#define StackLimit 16
#define Type_mask 0xF000
#define W 64
#define H 32
#define SCALE 8

uint8_t display[W*H];


uint8_t memory[4096];
uint16_t stack[StackLimit];
uint16_t PC = 0x200;
uint16_t IndexRegister = 0;
uint16_t instruction;
uint8_t delay_timer;
uint8_t sound_timer;
uint8_t registers[16] = {0};
int stack_pointer = 0;

void push(uint16_t value) {
    stack[stack_pointer] = value;
    stack_pointer++;
}
uint16_t pop() {
    stack_pointer--;
    return stack[stack_pointer];
}

void render(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (display[y*W+x]) {
                SDL_FRect r = {x * SCALE, y * SCALE, SCALE, SCALE};
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

int load_rom(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0 || size > 3584) {
        fprintf(stderr, "File size is invalid\n");
        fclose(f);
        return 0;
    }

    // Reset machine state so the previous ROM doesn't leak into this one
    memset(&memory[0x200], 0, sizeof(memory) - 0x200);
    memset(display, 0, sizeof display);
    memset(registers, 0, sizeof registers);
    stack_pointer = 0;
    PC = 0x200;
    IndexRegister = 0;

    size_t bytes_read = fread(memory+0x200, 1, size, f);
    fclose(f);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "File read error\n");
        return 0;
    }

    printf("Loaded ROM\n");
    return 1;
}

int run_rom(SDL_Renderer *renderer);

// Draw the splash once; the last frame stays on screen until a new ROM loads
void show_splash(SDL_Renderer *renderer) {
    if (!load_rom("splash.ch8")) return;
    for (int i = 0; i < 1000; i++) run_rom(renderer);
}

void choose_rom(SDL_Renderer *renderer) {
    show_splash(renderer);
    char filename[256];
    printf("Choose ROM from list\n");
    system("ls *.ch8");
    while (true) {
        if (!fgets(filename, sizeof filename, stdin)) {
            show_splash(renderer);
            break;
        }
        // Trim newline and surrounding quotes/whitespace
        size_t len = strcspn(filename, "\r\n");
        filename[len] = '\0';
        char *name = filename;
        while (*name == ' ' || *name == '"') name++;
        len = strlen(name);
        while (len > 0 && (name[len-1] == ' ' || name[len-1] == '"')) name[--len] = '\0';
        if (len == 0) continue;
        if (load_rom(name)) {
            break;
        }
        show_splash(renderer);
    }
}

int run_rom(SDL_Renderer *renderer) {
        instruction = memory[PC]<<8 | memory[PC+1];
        PC += 2;
        uint8_t n = instruction & 0x000F;
        uint8_t nn = instruction & 0x00FF;
        uint16_t nnn = instruction & 0x0FFF;
        uint8_t x = (instruction & 0x0F00) >> 8;
        uint8_t y = (instruction & 0x00F0) >> 4;
        uint8_t type = (instruction & 0xF000) >> 12;

        switch (type) {
            case 0: {
                switch (nnn) {
                    case 0xE0: {
                        memset(display, 0, sizeof display);
                        break;}
                    case 0xEE: {
                        PC = pop();
                        break;
                    }
                }
                break;
            }

            case 1: {
                PC = nnn;
                break;
            }
            case 2: {
                push(nnn);
                PC = nnn;
                break;
            }
            case 3: {
                if (registers[x] == nn) {PC +=2;}
                break;
            }
            case 4: {
                if (registers[x] != nn) {PC += 2;}
                break;
            }
            case 5: {
                if (registers[x] == registers[y]) {PC += 2;}
                break;
            }
            case 6: {
                registers[x] = nn;
                break;
            }
            case 7: {
                registers[x] += nn;
                break;
            }
            case 8: {
                switch (n) {
                    case 0: {
                        registers[x] = registers[y];
                        break;
                    }
                    case 1: {
                        registers[x] = registers[y] | registers[x];
                        break;
                    }
                    case 2: {
                        registers[x] = registers[y] & registers[x];
                        break;
                    }
                    case 3: {
                        registers[x] = registers[y] ^ registers[x];
                        break;
                    }
                    case 4: {
                        uint16_t sum = registers[x] + registers[y];
                        registers[x] = sum & 0xFF;
                        registers[0xF] = sum > 255;
                        break;
                    }
                    case 5: {
                        uint8_t no_borrow = registers[x] >= registers[y];
                        registers[x] -= registers[y];
                        registers[0xF] = no_borrow;
                        break;
                    }
                    case 6: {
                        registers[0xF] = registers[x] & 1;
                        registers[x] = registers[x] >> 1;
                        break;
                    }
                    case 0xE: {
                        registers[0Xf] = registers[x] >> 7;
                        registers[x] = registers[x] << 1;
                        break;
                    }
                }
                break;
            }

            case 9: {
                if (registers[x] != registers[y]) {PC += 2;}
                break;
            }

            case 0xA: {
                IndexRegister = nnn;
                break;
            }

            case 0xB: {
                PC = nnn + registers[0];
                break;
            }
            case 0xC: {
                registers[x] = rand() & nn;
                break;
            }

            case 0xD: {
                int rows = n;
                int posX = registers[x] % 64;
                int posY = registers[y] % 32;
                registers[0xF] = 0;

                for (int i = 0; i < rows; i++) {
                    int py = posY + i;
                    if (py >= H) break;

                    uint8_t data = memory[IndexRegister+i];

                    for (int col = 0; col < 8; col++) {
                        int px = posX + col;
                        if (px>=W) break;

                        if (data & (0x80 >> col)) {
                            uint8_t *pixel = &display[py*W+px];
                            if (*pixel) registers[0xF] = 1;
                            *pixel ^= 1;
                        }
                    }
                }
                break;
            }

            case 0xE: {
                switch (nn) {
                    case 0x9E: PC += 2; break; {}
                }
            }

            default: {
                perror("Invalid opcode");
            }
        }
        render(renderer);
}

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("Chip 8", 512, 256, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window,NULL);


    const uint8_t font[80] ={
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
    memcpy(&memory[0x050], font, sizeof font );

    bool running = true;
    SDL_Event event;

    choose_rom(renderer);

    //Update
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
            }
        }
        run_rom(renderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define KEYS_BASE 0xFF200050
#define AUDIO_BASE 0xFF203040
#define LED_BASE 0xFF200000

#define PI 3.141592653589
#define NUMSAMPLES 16384
#define PADDING 5

#define E4 329.63
#define B3 246.94
#define G3 196.00
#define D3 146.83
#define A2 110.00
#define E2 82.41

/*****************************************************************************/
/* GLOBALS */
/*****************************************************************************/  // Define states for different guitar strings
float frequencyOfString = -1;
float expectedFrequencyForString = D3;
bool areWeTuning = false;  // If true, exit empty while loop in main and record
                           // and do fourier transform

enum GuitarString {
  D_STRING,
  A_STRING,
  E_STRING,
  G_STRING,
  B_STRING,
  HIGH_E_STRING
};

float guitarStringFrequencies[6] = {D3, A2, E2, G3, B3, E4};

enum GuitarString stringState =
    D_STRING;  // Initialize string state to E_STRING

// Forward declaration of guitar and triangle (arrow) image arrays
extern const uint16_t guitar[162][85];
extern const uint16_t triangle[15][15];

// Forward declaration of functions
void setupKeys();
void clearKeyEdgeCapture();
void setupAudio();
void setupProcessorForInterrupts();
void interrupt_handler();
void write_pixel(int x, int y, short colour);
void draw_vertical_line(int x, int higherYValue, int lowerYValue, short colour);
void clear_screen();
void write_char(int x, int y, char c);
void write_phrase(int x, int y, char *phrase);
void drawGuitar();
void drawScale();
void drawArrow();
void drawNoteOnScale(float frequencyRecorded, float expectedFrequency);
void clear_character_buffer();
void drawBox(int x1, int x2, int y1, int y2, short colour);
void clearArrows();

// Forward declaration of Fourier Transform functions
void rearrange(float data_re[], float data_im[], const int N);
void compute(float data_re[], float data_im[], const int N);
void fft(float data_re[], float data_im[], const int N);
int recordAndPrint();

/*****************************************************************************/
/* MAIN */
/*****************************************************************************/

int main(void) {
  /******************** SET UP ********************/
  setupKeys();  // clears edge capture register and enables interrupts from all
                // buttons

  setupAudio();  // clears input and output FIFOs for both channels
  setupProcessorForInterrupts();  // enables the processor to be interrupted and
                                  // enables buttons to interrupt
  clear_screen();
  clear_character_buffer();

  drawGuitar();

  drawScale();
  drawArrow();

  while (/*!areWeTuning*/1) {
    // LEDptr->onoff = *((volatile unsigned long int*) (0xFF200040));
  }

  // exit from while loop to here if we are tuning by pressing pushbutton 3

  // write_phrase(30, 16, "Begin recording in...");
  // for (int i = 0; i < 25000000; i++) {
  // }
  // clear_character_buffer();
  // write_phrase(40, 16, "3");
  // for (int i = 0; i < 6250000; i++) {
  // }
  // clear_character_buffer();
  // write_phrase(40, 16, "2");
  // for (int i = 0; i < 6250000; i++) {
  // }
  // clear_character_buffer();
  // write_phrase(40, 16, "1");
  // for (int i = 0; i < 6250000; i++) {
  // }
  // clear_character_buffer();
  // write_phrase(36, 16, "Recording");

  // areWeTuning = false;

  // goto beforeWhileLoop;

  return 0;
}

/*****************************************************************************/
/* PUSHBUTTONS */
/*****************************************************************************/

struct pushbuttonStruct {
  volatile unsigned long int data;
  volatile unsigned long int unused;
  volatile unsigned long int interruptMask;
  volatile unsigned long int edgeCapture;
};

struct pushbuttonStruct *buttonptr = (struct pushbuttonStruct *)KEYS_BASE;

// sets up keys for interrupts and clears edgeCapture register
void setupKeys() {
  // clear edge capture register
  buttonptr->edgeCapture = 0b1111;

  // turn on interrupts for all buttons
  buttonptr->interruptMask = 0b1111;
}

void clearKeyEdgeCapture() {
  // clear edge capture register
  buttonptr->edgeCapture = 0b1111;
}

/*****************************************************************************/
/* AUDIOCORE */
/*****************************************************************************/

struct audioCoreStruct {
  // Control Register
  volatile unsigned long int control;

  // FIFO SPACE REGISTER
  volatile unsigned char rarc;  // read available, right channel
  volatile unsigned char ralc;  // read available, left channel
  volatile unsigned char wsrc;  // write space, right channel
  volatile unsigned char wslc;  // write space, left channel

  // LEFT FIFO DATA REGISTER
  volatile unsigned long int leftFIFO;

  // RIGHT FIFO DATA REGISTER
  volatile unsigned long int rightFIFO;
};

struct audioCoreStruct *audioptr = (struct audioCoreStruct *)AUDIO_BASE;

// clear read and write FIFOs for both left and right channels
void setupAudio() {
  audioptr->control =
      0b1100;  // bit 2 clears read FIFOs and bit 3 clears write FIFOs
  audioptr->control = 0x0;  // resume
}

/*****************************************************************************/
/* LEDS */
/*****************************************************************************/

struct LEDstruct {
  volatile unsigned long int onoff;
};

struct LEDstruct *LEDptr = (struct LEDstruct *)LED_BASE;

/*****************************************************************************/
/* Macros for accessing the control registers. */
/*****************************************************************************/
#define NIOS2_READ_STATUS(dest) \
  do {                          \
    dest = __builtin_rdctl(0);  \
  } while (0)

#define NIOS2_WRITE_STATUS(src) \
  do {                          \
    __builtin_wrctl(0, src);    \
  } while (0)

#define NIOS2_READ_ESTATUS(dest) \
  do {                           \
    dest = __builtin_rdctl(1);   \
  } while (0)

#define NIOS2_READ_BSTATUS(dest) \
  do {                           \
    dest = __builtin_rdctl(2);   \
  } while (0)

#define NIOS2_READ_IENABLE(dest) \
  do {                           \
    dest = __builtin_rdctl(3);   \
  } while (0)

#define NIOS2_WRITE_IENABLE(src) \
  do {                           \
    __builtin_wrctl(3, src);     \
  } while (0)

#define NIOS2_READ_IPENDING(dest) \
  do {                            \
    dest = __builtin_rdctl(4);    \
  } while (0)

#define NIOS2_READ_CPUID(dest) \
  do {                         \
    dest = __builtin_rdctl(5); \
  } while (0)

void setupProcessorForInterrupts() {
  NIOS2_WRITE_IENABLE(0b10);  // enables interrupts from keys

  NIOS2_WRITE_STATUS(
      0b1);  // enables processor to be interrupted by setting PIE bit to 1
}

/*****************************************************************************/
/* EXCEPTION FUNCTION */
/*****************************************************************************/

/*
The assembly language code below handles CPU exception
processing. This code should not be modified; instead, the C
language code in the function interrupt_handler() can be
modified as needed for a given application.
*/
void the_exception() __attribute__((section(".exceptions")));
void the_exception() {
  asm(".set noat");     // Magic, for the C compiler
  asm(".set nobreak");  // Magic, for the C compiler
  asm("subi sp, sp, 128");
  asm("stw et, 96(sp)");
  asm("rdctl et, ctl4");
  asm("beq et, r0, SKIP_EA_DEC");  // Interrupt is not external
  asm("subi ea, ea, 4");           /* Must decrement ea by one
                                     instruction for external interupts, so that the
                                     interrupted           instruction will be run */
  asm("SKIP_EA_DEC:");
  asm("stw r1, 4(sp)");  // Save all registers
  asm("stw r2, 8(sp)");
  asm("stw r3, 12(sp)");
  asm("stw r4, 16(sp)");
  asm("stw r5, 20(sp)");
  asm("stw r6, 24(sp)");
  asm("stw r7, 28(sp)");
  asm("stw r8, 32(sp)");
  asm("stw r9, 36(sp)");
  asm("stw r10, 40(sp)");
  asm("stw r11, 44(sp)");
  asm("stw r12, 48(sp)");
  asm("stw r13, 52(sp)");
  asm("stw r14, 56(sp)");
  asm("stw r15, 60(sp)");
  asm("stw r16, 64(sp)");
  asm("stw r17, 68(sp)");
  asm("stw r18, 72(sp)");
  asm("stw r19, 76(sp)");
  asm("stw r20, 80(sp)");
  asm("stw r21, 84(sp)");
  asm("stw r22, 88(sp)");
  asm("stw r23, 92(sp)");
  asm("stw r25, 100(sp)");  // r25 = bt (skip r24 = et, because it is saved
                            // above)
  asm("stw r26, 104(sp)");  // r26 = gp
                            // skip r27 because it is sp, and there is no point
                            // in saving this
  asm("stw r28, 112(sp)");  // r28 = fp
  asm("stw r29, 116(sp)");  // r29 = ea
  asm("stw r30, 120(sp)");  // r30 = ba
  asm("stw r31, 124(sp)");  // r31 = ra
  asm("addi fp, sp, 128");
  asm("call interrupt_handler");  // Call the C language interrupt handler
  asm("ldw r1, 4(sp)");           // Restore all registers
  asm("ldw r2, 8(sp)");
  asm("ldw r3, 12(sp)");
  asm("ldw r4, 16(sp)");
  asm("ldw r5, 20(sp)");
  asm("ldw r6, 24(sp)");
  asm("ldw r7, 28(sp)");
  asm("ldw r8, 32(sp)");
  asm("ldw r9, 36(sp)");
  asm("ldw r10, 40(sp)");
  asm("ldw r11, 44(sp)");
  asm("ldw r12, 48(sp)");
  asm("ldw r13, 52(sp)");
  asm("ldw r14, 56(sp)");
  asm("ldw r15, 60(sp)");
  asm("ldw r16, 64(sp)");
  asm("ldw r17, 68(sp)");
  asm("ldw r18, 72(sp)");
  asm("ldw r19, 76(sp)");
  asm("ldw r20, 80(sp)");
  asm("ldw r21, 84(sp)");
  asm("ldw r22, 88(sp)");
  asm("ldw r23, 92(sp)");
  asm("ldw r24, 96(sp)");
  asm("ldw r25, 100(sp)");  // r25 = bt
  asm("ldw r26, 104(sp)");  // r26 = gp
  asm("ldw r28, 112(sp)");  // r28 = fp
  asm("ldw r29, 116(sp)");  // r29 = ea
  asm("ldw r30, 120(sp)");  // r30 = ba
  asm("ldw r31, 124(sp)");  // r31 = ra
  asm("addi sp, sp, 128");
  asm("eret");
}

/*****************************************************************************/
/* INTERRUPT HANDLER */
/*****************************************************************************/

void interrupt_handler() {
  if (__builtin_rdctl(4) == 0b10) {
    // if (LEDptr->onoff == 0b1111111111) {
    //   LEDptr->onoff = 0;
    //   clearKeyEdgeCapture();
    // } else {
    //   LEDptr->onoff = 0b1111111111;
    //   clearKeyEdgeCapture();
    // }

    // Cycle forward through string states
    if (buttonptr->edgeCapture & 0b1) {
      stringState++;
      if (stringState > 5) {
        stringState = 0;  // Wrap around to the first string state
      }
    }
    // Cycle backward through string states
    else if (buttonptr->edgeCapture & 0b10) {
      if (stringState == 0) {  // if stringState == 0, wrap around to 5
        stringState = 5;
      } else {
        stringState--;
      }
    } else if (buttonptr->edgeCapture & 0b1000) {
      // areWeTuning = !areWeTuning;
      // printf("areWeTuning = %d\n", areWeTuning);
      
      frequencyOfString = recordAndPrint();
      //clear previous scale
      drawBox(49, 269, 10, 50, 0x0);
      drawScale();
      drawNoteOnScale(frequencyOfString, expectedFrequencyForString);
      printf("frequency of String: %f and expected frequency: %f\n", frequencyOfString, expectedFrequencyForString);
    }

    // assign expectedFrequencyForString the frequency expected for string
    // selected via pushbuttons 0 and 1
    expectedFrequencyForString = guitarStringFrequencies[stringState];

    printf("expected frequency for string state: %d: %f\n", stringState,
           expectedFrequencyForString);
    clearArrows();
    drawArrow();

    clearKeyEdgeCapture();
  }
}

/*****************************************************************************/
/* VGA */
/*****************************************************************************/
/* set a single pixel on the screen at x,y
 * x in [0,319], y in [0,239], and colour in [0,65535]
 */
void write_pixel(int x, int y, short colour) {
  volatile short *vga_addr =
      (volatile short *)(0x08000000 + (y << 10) + (x << 1));
  *vga_addr = colour;
}

void draw_vertical_line(int x, int higherYValue, int lowerYValue,
                        short colour) {
  // draws vertical line by iterating by deltay and drawing pixels down
  // the screen add deltay to higherYValue because the top left corner is (0, 0)

  for (int deltay = higherYValue; deltay < lowerYValue; ++deltay) {
    write_pixel(x, deltay, colour);
  };
}

/* use write_pixel to set entire screen to black (does not clear the character
 * buffer) */
void clear_screen() {
  int x, y;
  for (x = 0; x < 320; x++) {
    for (y = 0; y < 240; y++) {
      write_pixel(x, y, 0x0);
    }
  }
}

/* write a single character to the character buffer at x,y
 * x in [0,79], y in [0,59]
 */
void write_char(int x, int y, char c) {
  // VGA character buffer
  volatile char *character_buffer = (char *)(0x09000000 + (y << 7) + x);
  *character_buffer = c;
}

void clear_character_buffer() {
  for (int x = 0; x < 80; ++x) {
    for (int y = 0; y < 60; ++y) {
      volatile char *character_buffer = (char *)(0x09000000 + (y << 7) + x);
      *character_buffer = 0;
    }
  }
}

void write_phrase(int x, int y, char *phrase) {
  while (*phrase) {
    write_char(x, y, *phrase);
    ++phrase;
    ++x;
  }
}

void drawGuitar() {
  for (int x = 0; x < 85; ++x) {
    for (int y = 0; y < 162; ++y) {
      write_pixel(x + 117, y + 78,
                  guitar[y][x]);  // Guitar png is of certain size. The x + 117
                                  // and y + 78 are chosen so that the bottom
                                  // screen is the bottom pixels of the guitar
    }
  }
}

// draws the 23 lines that make up the scale
void drawScale() {
  for (int lineNumber = 0; lineNumber < 23; ++lineNumber) {
    if (lineNumber == 0 || lineNumber == 11 || lineNumber == 22) {
      draw_vertical_line(
          lineNumber * 10 + 49, 10, 50,
          0xFFFF);  // lines are spaced 10 pixels apart, starting at x = 49.
                    // Lines are drawn from y = 10 to y = 50 and are 0x0 (black)
    } else {
      draw_vertical_line(lineNumber * 10 + 49, 25, 35, 0xFFFF);
    }
  }
}

void drawBox(int x1, int x2, int y1, int y2, short colour) {
  for (int x = x1; x < x2; ++x) {
    for (int y = y1; y < y2; ++y) {
      write_pixel(x, y, colour);
    }
  }
};

void clearArrows() {
  // draws black box over the left and right sides of the guitar where the
  // arrows would be drawn
  drawBox(208, 235, 90, 200, 0x0);
  drawBox(95, 110, 90, 200, 0x0);
}

// draws the arrow that shows what string is selected for tuning
void drawArrow() {
  if (stringState == HIGH_E_STRING || stringState == B_STRING ||
      stringState == G_STRING) {
    if (stringState == HIGH_E_STRING) {
      for (int x = 0; x < 15; ++x) {
        for (int y = 0; y < 15; ++y) {
          write_pixel(x + 208, y + 151, triangle[y][x]);
        }
      }

    } else if (stringState == B_STRING) {
      for (int x = 0; x < 15; ++x) {
        for (int y = 0; y < 15; ++y) {
          write_pixel(x + 208, y + 125, triangle[y][x]);
        }
      }

    } else if (stringState == G_STRING) {
      for (int x = 0; x < 15; ++x) {
        for (int y = 0; y < 15; ++y) {
          write_pixel(x + 208, y + 98, triangle[y][x]);
        }
      }
    }

    else {
      // error
    }
  }

  else if (stringState == E_STRING || stringState == A_STRING ||
           stringState == D_STRING) {
    if (stringState == E_STRING) {
      for (int x = 0; x < 15; ++x) {
        for (int y = 0; y < 15; ++y) {
          write_pixel(x * -1 + 110, y + 151, triangle[y][x]);
        }
      }
    } else if (stringState == A_STRING) {
      for (int x = 0; x < 15; ++x) {
        for (int y = 0; y < 15; ++y) {
          write_pixel(x * -1 + 110, y + 125, triangle[y][x]);
        }
      }
    } else if (stringState == D_STRING) {
      for (int x = 0; x < 15; ++x) {
        for (int y = 0; y < 15; ++y) {
          write_pixel(x * -1 + 110, y + 98, triangle[y][x]);
        }
      }
    } else {
      // error
    }
  }

  else {
    printf("Error: stringState is not one of the strings");
  }
}

// draws a line on the scale that represents the frequency of the note recorded
void drawNoteOnScale(float frequencyRecorded, float expectedFrequency) {
  int difference_in_frequency = (int)(frequencyRecorded - expectedFrequency);
  int colour = 0xF81F;  // initialized as purple for debugging
  int sign = (difference_in_frequency < 0) ? -1 : 1;

  int absDifference = abs(difference_in_frequency);
  char *tuningInstructions;

  // if difference within 16 Hz, line drawn on scale is green
  if (absDifference < 16) {
    colour = 0x07E0;  // hexadecimal for green

    // if difference is less than 8 Hz, then difference is barely perceptible
    if (absDifference < 8) {
      // print "Good!"
      clear_character_buffer();
      tuningInstructions = "Good!";
      write_phrase(38, 16, tuningInstructions);
    }
    // if difference is greater than 8 Hz, but less than 16 Hz, tell which
    // direction to tune
    else {
      // print tune in whatever direction
      if (sign > 0) {
        clear_character_buffer();
        tuningInstructions = "Tune down";
        write_phrase(36, 16, tuningInstructions);
      } else {
        clear_character_buffer();
        tuningInstructions = "Tune up";
        write_phrase(37, 16, tuningInstructions);
      }
    }
  }
  // else if, difference is within 50 Hz
  else if (absDifference < 50) {
    colour = 0xFFC0;  // hex for yellow
    // print tune in whatever direction
    if (sign > 0) {
      clear_character_buffer();
      tuningInstructions = "Tune down";
      write_phrase(36, 16, tuningInstructions);
    } else {
      clear_character_buffer();
      tuningInstructions = "Tune up";
      write_phrase(37, 16, tuningInstructions);
    }
  }
  // if difference greater than 50 Hz, set colour to red
  else {
    colour = 0xF800;  // hex for red
    // else, difference is greater than 110, set the difference to 110 so that
    // line is drawn within the scale
    if (absDifference > 110) {
      difference_in_frequency = sign * 110;
    }
    // print tune in whatever direction
    if (sign > 0) {
      clear_character_buffer();
      tuningInstructions = "Tune down";
      write_phrase(36, 16, tuningInstructions);
    } else {
      clear_character_buffer();
      tuningInstructions = "Tune up";
      write_phrase(37, 16, tuningInstructions);
    }
  }

  draw_vertical_line(159 + difference_in_frequency, 10, 50,
                     colour);  // line is drawn starting at x = 159 (the
                               // middle of the scale) in red
}

/*****************************************************************************/
/* FOURIER TRANSFORM */
/*****************************************************************************/

void rearrange(float data_re[], float data_im[], const int N) {
  unsigned int target = 0;
  for (unsigned int position = 0; position < N; position++) {
    if (target > position) {
      const float temp_re = data_re[target];
      const float temp_im = data_im[target];
      data_re[target] = data_re[position];
      data_im[target] = data_im[position];
      data_re[position] = temp_re;
      data_im[position] = temp_im;
    }
    unsigned int mask = N;
    while (target & (mask >>= 1)) target &= ~mask;
    target |= mask;
  }
}

void compute(float data_re[], float data_im[], const int N) {
  const float pi = -3.14159265358979323846;

  for (unsigned int step = 1; step < N; step <<= 1) {
    const unsigned int jump = step << 1;
    const float step_d = (float)step;
    float twiddle_re = 1.0;
    float twiddle_im = 0.0;
    for (unsigned int group = 0; group < step; group++) {
      for (unsigned int pair = group; pair < N; pair += jump) {
        const unsigned int match = pair + step;
        const float product_re =
            twiddle_re * data_re[match] - twiddle_im * data_im[match];
        const float product_im =
            twiddle_im * data_re[match] + twiddle_re * data_im[match];
        data_re[match] = data_re[pair] - product_re;
        data_im[match] = data_im[pair] - product_im;
        data_re[pair] += product_re;
        data_im[pair] += product_im;
      }

      // we need the factors below for the next iteration
      // if we don't iterate then don't compute
      if (group + 1 == step) {
        continue;
      }

      float angle = pi * ((float)group + 1) / step_d;
      twiddle_re = cos(angle);
      twiddle_im = sin(angle);
    }
  }
}

void fft(float data_re[], float data_im[], const int N) {
  rearrange(data_re, data_im, N);
  compute(data_re, data_im, N);
}

int recordAndPrint() {
  volatile int *LEDS = (int *)0xff200000;
  volatile int *audio_ptr = (int *)AUDIO_BASE;

  write_phrase(30, 16, "Begin recording in...");
      for (int i = 0; i < 25000000; i++) {
      }
      clear_character_buffer();
      write_phrase(40, 16, "3");
      for (int i = 0; i < 12500000; i++) {
      }
      clear_character_buffer();
      write_phrase(40, 16, "2");
      for (int i = 0; i < 12500000; i++) {
      }
      clear_character_buffer();
      write_phrase(40, 16, "1");
      for (int i = 0; i < 12500000; i++) {
      }
      clear_character_buffer();
      write_phrase(36, 16, "Recording");

  int fifospace;
  fifospace = *(audio_ptr + 1);  // read the audio port fifospace register

  *LEDS = 0;

  float re[NUMSAMPLES] = {0};
  float im[NUMSAMPLES] = {0};

  int samples[NUMSAMPLES] = {0};

  // Clear FIFO Read and Write

setupAudio();

  int i = 0;
  while (i < NUMSAMPLES) {
    fifospace = *(audio_ptr + 1);
    if ((fifospace & 0x000000FF) > 0) {
      samples[i] = *(audio_ptr + 2);
      samples[i] = *(audio_ptr + 3);
      i++;
    }
  }

  clear_character_buffer();
  write_phrase(33, 16, "Done recording");
  for (int i = 0; i < 25000000; ++i){

  }
  clear_character_buffer();
  write_phrase(35, 16, "Calculating");

  // Compute RMS of signal:

  for (int j = 0; j < NUMSAMPLES; j++) {
    re[j] = 1.0 * samples[j];
  }

  fft(re, im, NUMSAMPLES);

  int maxK = 0;
  float maxAmp = 0;

  for (int i = 0; i < NUMSAMPLES / 2; i++) {
    if (re[i] > maxAmp && ((1.0) / NUMSAMPLES) * 1.0 * i * 8000.0 > 50 &&
        ((1.0) / NUMSAMPLES) * 1.0 * i * 8000.0 < 380) {
      maxK = i;
      maxAmp = re[i];
    }
  }

  float maxAng = ((1.0) / NUMSAMPLES) * 1.0 * maxK * 8000.0;
  return maxAng;
  // Clear buffer out of old samples
}

// Draws triangle to display to user which string is currently selected for
// tuning 15 x 15
const uint16_t triangle[15][15] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10565, 40147, 4226},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6307, 38034, 65535, 65503, 2081},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 27501, 59196, 65535, 65535, 59228, 2113},
    {0, 0, 0, 0, 0, 0, 0, 19017, 52825, 65535, 65535, 65535, 65535, 59228,
     2113},
    {0, 0, 0, 0, 0, 10565, 44373, 65535, 65535, 65535, 65535, 65535, 65535,
     59228, 2113},
    {0, 0, 0, 4226, 35921, 63422, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 59228, 2113},
    {0, 2145, 25388, 57115, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 59228, 2113},
    {0, 33808, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 59228, 2113},
    {0, 2113, 25324, 57083, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 59228, 2113},
    {0, 0, 0, 4194, 33840, 63422, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 59228, 2113},
    {0, 0, 0, 0, 0, 10565, 44373, 65535, 65535, 65535, 65535, 65535, 65535,
     59228, 2113},
    {0, 0, 0, 0, 0, 0, 0, 19017, 52825, 65535, 65535, 65535, 65535, 59228,
     2113},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 27469, 59196, 65535, 65535, 59228, 2113},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6307, 35985, 65535, 65503, 2081},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10565, 38098, 4226},

};

// GUITAR IMAGE ARRAY
// 162 x 85
const uint16_t guitar[162][85] = {
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     19049, 44405, 65535, 65535,
     63389, 61243, 59162, 61243, 63389, 65534, 65535, 44437, 19049, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     31727, 65535, 65469, 50547, 44143, 37706,
     37673, 37706, 35593, 37674, 37673, 37738, 44110, 50515, 65469, 65535,
     31727, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     4226,  61308, 65534, 44111, 37706, 37673, 31269, 29123,
     26978, 27043, 27010, 26946, 29058, 29123, 31236, 37609, 37674, 44110,
     65534, 61341, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     19017, 65535, 54774, 37706, 35528, 26978, 29091, 31171, 29156,
     29123, 29156, 31172, 27011, 29091, 29124, 29123, 26978, 26978, 35528,
     37706, 54774, 65535, 19017, 0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     42259, 65535, 46224, 37706, 27011, 27010, 27043, 29123, 31204, 31236,
     31236, 29156, 27043, 27043, 27043, 29156, 29124, 27011, 31204, 27010,
     29124, 39787, 46256, 65535, 42259, 0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     59195,
     65534, 39852, 37674, 26978, 27011, 29123, 27043, 29123, 31236, 31236,
     31236, 31268, 27043, 29091, 29091, 29124, 29156, 29091, 29123, 29091,
     29091, 26978, 37674, 39852, 65501, 65535, 10565, 0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     32,    38066, 65535, 50548,
     35626, 31335, 24898, 27011, 27043, 29123, 29091, 31204, 31236, 31236,
     31236, 31204, 27043, 29091, 29091, 29124, 29156, 27011, 29124, 29124,
     29091, 29124, 24866, 33415, 37739, 50515, 65535, 38066, 32,    0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     33,    40179, 65535, 61210, 39852, 37706,
     22818, 26978, 27011, 27043, 29091, 29123, 29124, 29156, 33316, 33316,
     29156, 31204, 29124, 29123, 29091, 29124, 31204, 31204, 31204, 29124,
     29124, 31204, 29123, 29059, 26978, 35658, 39820, 61210, 65535, 40179,
     32,    0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     2113,
     8452,  16904, 35953, 65535, 65535, 59097, 41998, 37706, 33349, 24866,
     24898, 27011, 27011, 29091, 27043, 29124, 29123, 31204, 29156, 33316,
     31236, 33316, 29124, 27043, 29124, 29124, 29124, 31204, 31204, 29124,
     29123, 29124, 27075, 29123, 27043, 24866, 31302, 39786, 44110, 59098,
     65535, 52857, 38034, 16904, 8452,  2113,  0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     40146,
     65535, 65501, 59065, 50514, 39852, 37706, 33382, 26978, 29091, 27043,
     24931, 27043, 27011, 29123, 27043, 29123, 27043, 29156, 31204, 33317,
     33316, 31236, 29156, 29091, 29124, 31204, 29123, 31204, 31236, 29124,
     31172, 29124, 29124, 29124, 27043, 27011, 27011, 27010, 33382, 37706,
     41932, 48434, 59097, 65502, 65535, 40146, 0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     63421,
     46223, 37738, 37673, 37608, 27076, 26978, 27043, 27043, 29124, 27043,
     24931, 27011, 27011, 29123, 29123, 29091, 29123, 29156, 31236, 29156,
     33316, 31236, 29124, 31171, 29124, 29156, 31204, 31204, 31236, 31204,
     29124, 29156, 29123, 31204, 27043, 27011, 27043, 29124, 31204, 26978,
     29156, 35495, 37673, 37706, 44143, 63421, 0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     33808, 65535,
     39819, 26978, 27011, 29123, 27011, 29124, 27043, 29091, 29124, 27011,
     24963, 27043, 27043, 29091, 27043, 29123, 29091, 31204, 31236, 31236,
     31236, 31236, 29188, 31204, 31204, 29123, 31204, 31172, 31236, 29124,
     31204, 29124, 29124, 29156, 29123, 24931, 27043, 31204, 31204, 29092,
     27011, 27011, 29059, 26946, 37739, 65535, 21162, 0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     6371,  65535, 44078,
     37641, 29091, 29123, 31172, 27043, 27043, 27043, 29091, 31172, 27011,
     27011, 27011, 29091, 29123, 27043, 31171, 29091, 29124, 31236, 31236,
     33316, 31236, 31204, 31171, 33284, 29091, 31204, 31204, 31204, 29124,
     29092, 29124, 29124, 31204, 29124, 24931, 27043, 31204, 29124, 29092,
     27043, 29124, 31204, 29091, 33480, 44078, 65535, 6371,  0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     29581, 65535, 48401, 39819,
     26978, 29124, 29124, 31204, 29123, 29123, 29091, 27043, 29123, 27043,
     27043, 27011, 27011, 29091, 29124, 29091, 27011, 27043, 33316, 31236,
     33284, 29124, 29091, 29091, 31204, 29124, 29124, 29156, 31236, 31172,
     29124, 29124, 29156, 31204, 27043, 27011, 29124, 31236, 29156, 31204,
     27043, 27075, 33284, 29123, 24898, 39851, 48369, 65535, 29581, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     6339,  29581, 59163, 65535, 44078, 39819, 27010,
     29091, 29156, 29123, 33284, 27043, 27043, 29123, 29091, 29123, 27011,
     27043, 27043, 29091, 27043, 29156, 29124, 29091, 27011, 29156, 31236,
     31204, 29123, 29124, 31171, 29124, 29156, 31236, 29156, 31236, 31172,
     29124, 29124, 29124, 31236, 29124, 27043, 27076, 33284, 31204, 29156,
     29092, 27075, 33284, 29123, 27043, 29058, 39819, 44078, 65535, 59163,
     29582, 6339,  0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     52791, 65469, 46256, 37706, 35528, 26978, 29123,
     29124, 29124, 29124, 31236, 27043, 29123, 31204, 29091, 31172, 29124,
     24963, 27011, 29123, 29124, 27076, 29156, 29124, 27043, 29156, 31236,
     31236, 33316, 27076, 31204, 31204, 29156, 29156, 31204, 31236, 29124,
     31204, 29124, 29124, 31236, 27043, 27043, 27044, 33316, 29156, 31204,
     29124, 27043, 31236, 29124, 29091, 29124, 26978, 35528, 37739, 46289,
     65469, 52792, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     46518, 52595, 39786, 26978, 26978, 29123, 29123,
     29124, 31204, 29124, 31204, 27043, 29124, 29123, 31204, 29124, 29123,
     27011, 27043, 29123, 31172, 29124, 29124, 29091, 29091, 31204, 31204,
     31236, 31236, 31204, 29124, 29124, 31172, 29156, 29156, 29124, 31204,
     29124, 29124, 29124, 33284, 29124, 29124, 29124, 33316, 31204, 31204,
     29124, 27044, 33284, 29124, 27043, 29124, 29123, 26978, 27011, 39819,
     52596, 46518, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     16936, 63290, 37641, 26946, 27011, 31172, 29123,
     27043, 31204, 29124, 31204, 27011, 29124, 29123, 31204, 29124, 27043,
     27011, 27043, 31172, 29123, 31204, 29123, 31171, 27043, 29156, 29156,
     31236, 31204, 31236, 29123, 27043, 29123, 29124, 31236, 29123, 29124,
     29124, 29124, 29124, 31204, 29124, 27043, 29124, 33316, 31204, 31236,
     27044, 27076, 33284, 29124, 27043, 31204, 29091, 29091, 26978, 37609,
     61243, 16936, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     19017, 21130, 19049, 19049, 19049,
     0,     0,     0,     23210, 65534, 39721, 26978, 29091, 31172, 29123,
     29124, 31236, 27076, 31236, 27011, 29124, 29123, 31204, 29123, 27076,
     27011, 27011, 31172, 29091, 31204, 31204, 29123, 27011, 31204, 29123,
     31236, 31204, 31172, 29091, 29091, 29124, 31204, 31236, 29123, 31172,
     29124, 29124, 29156, 33284, 29124, 29124, 29124, 33316, 31236, 31236,
     29124, 27076, 33284, 29124, 27043, 29124, 29124, 29123, 26978, 37641,
     65534, 23243, 0,     0,     0,     0,     27501, 27469, 8420,  0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     19049, 46486, 19049, 8420,  4258,  4226,  8452,
     35854, 4226,  0,     2081,  65535, 37641, 29091, 27043, 31171, 31203,
     31204, 31204, 31204, 31236, 27043, 31204, 29156, 31204, 31236, 29124,
     27011, 27011, 29156, 27043, 31204, 29123, 29124, 29091, 31236, 29124,
     33316, 31236, 31204, 29124, 31204, 29123, 31204, 31236, 29123, 29123,
     29091, 29124, 31204, 31236, 31236, 27043, 31204, 33316, 31204, 29156,
     29156, 31236, 31236, 29124, 27043, 29124, 27011, 29091, 27011, 37641,
     65535, 33,    0,     0,     46486, 42292, 19049, 21130, 29582, 38034,
     21130, 0,     0,     0,     0},
    {0,     0,     21162, 16904, 4193,  8452,  10565, 10565, 10565, 8452,
     12579, 21129, 0,     0,     65535, 44045, 29124, 27011, 29123, 31204,
     31204, 31204, 31236, 31204, 27011, 31204, 31204, 29124, 31204, 29124,
     27011, 26979, 31204, 29091, 29124, 31204, 29091, 29123, 29156, 31236,
     33316, 31204, 31204, 31172, 29091, 29091, 29124, 33316, 29123, 29123,
     27043, 29124, 31204, 31236, 31204, 29156, 29156, 31236, 29156, 29156,
     29156, 31236, 33316, 27075, 27011, 29123, 24963, 27011, 29092, 44045,
     65535, 0,     0,     19049, 16837, 2145,  6339,  6307,  6307,  4226,
     4226,  27469, 10565, 0,     0},
    {0,     25389, 10565, 6371,  10630, 12710, 12711, 12743, 14823, 14856,
     14790, 40178, 0,     0,     65535, 46256, 29156, 29091, 31171, 31172,
     29123, 29091, 26978, 29090, 27010, 31172, 29124, 29123, 31236, 29123,
     26979, 27011, 31204, 27043, 29123, 31172, 29123, 27043, 31204, 31236,
     29156, 29124, 31204, 31171, 29091, 27043, 29123, 33284, 31204, 31204,
     27011, 29124, 31236, 31204, 31204, 29156, 31236, 31236, 31236, 29156,
     29124, 27010, 31138, 26978, 29091, 29156, 27043, 27011, 29156, 46289,
     65535, 0,     0,     31662, 10499, 14824, 16937, 14856, 14856, 14856,
     12678, 4226,  16904, 8484,  0},
    {6307,  25356, 6339,  10598, 10598, 10598, 10630, 12678, 12710, 12743,
     16903, 40146, 0,     0,     46518, 48402, 33415, 29059, 31171, 29091,
     29091, 46191, 54806, 46354, 31302, 29058, 29123, 29123, 31172, 29124,
     26979, 27043, 29124, 27011, 29124, 31204, 29124, 29091, 31204, 33284,
     31236, 29124, 31204, 29123, 29091, 27011, 27043, 33316, 31204, 31204,
     27043, 29091, 31236, 31204, 31236, 29156, 33284, 31204, 31236, 29091,
     33382, 50515, 54806, 42031, 29091, 31171, 29091, 27011, 33415, 50482,
     42292, 0,     0,     40178, 14790, 17001, 16969, 16969, 16969, 16969,
     19050, 16936, 4194,  29614, 0},
    {27534, 12710, 10630, 10630, 10630, 12678, 12678, 12711, 12711, 12743,
     16903, 40114, 0,     0,     44405, 50548, 37673, 27010, 31171, 31171,
     52792, 59261, 63422, 50777, 40212, 39885, 26946, 29124, 29092, 29156,
     27011, 27011, 29123, 24963, 29124, 31204, 29124, 29091, 31204, 31204,
     31236, 31204, 31204, 31172, 27043, 27011, 29123, 31236, 29124, 29123,
     29091, 29123, 31204, 31204, 31236, 29156, 31236, 33316, 31171, 37674,
     57083, 61342, 61309, 44406, 40212, 35495, 24930, 27010, 37673, 52596,
     44405, 0,     0,     38066, 14758, 16969, 16937, 16937, 16937, 16937,
     16937, 16969, 14856, 6307,  21162},
    {31760, 10597, 12711, 12678, 12678, 12678, 12710, 12711, 12711, 12743,
     16871, 33775, 12678, 19017, 38033, 56951, 37673, 26978, 31106, 37903,
     40212, 57115, 61277, 50744, 46551, 59261, 33415, 29123, 29123, 31204,
     27043, 29091, 29124, 24931, 31204, 31204, 31204, 29124, 31236, 29123,
     29156, 29156, 31236, 31172, 27011, 22818, 29123, 33284, 31204, 31204,
     29091, 29123, 31236, 31204, 31236, 29156, 31236, 31236, 31236, 38099,
     46486, 59228, 57115, 50712, 48631, 59130, 24865, 27010, 37706, 56919,
     50744, 4226,  0,     38066, 14758, 16969, 16937, 16937, 16937, 16937,
     16937, 16937, 16969, 8484,  27469},
    {40179, 10598, 21260, 17002, 19115, 19115, 21195, 21228, 21228, 23308,
     21162, 16870, 40114, 40147, 40179, 61178, 35561, 26978, 31171, 29582,
     33840, 54970, 59196, 59196, 59196, 61342, 48370, 29026, 31172, 29124,
     29124, 27011, 31204, 24931, 27043, 31236, 29156, 29124, 31204, 31204,
     31236, 31236, 33284, 29091, 27011, 24931, 31236, 31236, 31204, 29123,
     29091, 29123, 31236, 31204, 31204, 29156, 33316, 31204, 33448, 25421,
     44405, 57051, 59196, 61277, 57115, 65535, 31302, 26978, 35593, 61210,
     40179, 38098, 40179, 29581, 12710, 14888, 14856, 14856, 14856, 14856,
     14856, 14856, 16937, 10565, 29647},
    {25356, 27567, 57149, 59229, 59261, 59262, 59262, 61342, 61342, 61375,
     57083, 27272, 29614, 29647, 35954, 63421, 37706, 26978, 31204, 19082,
     19049, 52825, 52857, 61277, 52825, 55035, 46354, 29026, 29123, 29124,
     29124, 27011, 29123, 27011, 29124, 31204, 29124, 31204, 33284, 31204,
     33284, 31236, 33284, 31172, 27011, 27011, 33284, 31236, 31204, 29123,
     29091, 29123, 31204, 31204, 31204, 29156, 33316, 31236, 27239, 16904,
     31727, 52825, 54938, 61309, 48631, 59261, 33448, 26978, 37706, 65502,
     12645, 21162, 25389, 21063, 33905, 42358, 40277, 40245, 38197, 38164,
     38164, 38164, 38197, 21195, 16936},
    {21163, 38132, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     61308, 16837, 18984, 14856, 12711, 65535, 37706, 27011, 31139, 14823,
     6372,  23243, 44405, 42260, 25356, 52890, 37805, 29090, 29091, 29124,
     29124, 29123, 29124, 27011, 31204, 29124, 31236, 29124, 29124, 31204,
     31204, 31236, 33284, 29123, 29091, 27011, 31204, 31236, 31204, 31172,
     29091, 29123, 31236, 31204, 29124, 29156, 31236, 33284, 27141, 10630,
     12710, 35921, 46486, 38034, 42292, 55003, 29091, 27010, 37706, 65535,
     46518, 40244, 42325, 23176, 61341, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 65535, 35954, 12678},
    {35953, 33841, 63487, 61374, 63422, 63422, 63454, 63455, 63455, 65535,
     54904, 10532, 29582, 27501, 21162, 63421, 39787, 29156, 31139, 20901,
     8517,  6371,  4258,  12710, 31695, 44373, 27010, 31204, 29124, 29124,
     29124, 27043, 29123, 27043, 31204, 27043, 29123, 29123, 29124, 31236,
     31204, 31204, 31204, 31204, 29091, 29124, 31204, 31204, 31236, 29123,
     29124, 29124, 31204, 29124, 29124, 29124, 33316, 33284, 29123, 12645,
     29647, 12710, 4258,  23275, 42357, 37903, 26946, 31204, 41900, 63421,
     0,     6306,  4226,  10531, 59196, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 65535, 46486, 14791},
    {23275, 21163, 59261, 57116, 55068, 57116, 57116, 57148, 57148, 61374,
     48598, 29581, 6307,  6371,  6339,  65535, 44078, 31236, 29091, 27011,
     18821, 12710, 10565, 16904, 29647, 22883, 29091, 31204, 29123, 29124,
     29156, 29124, 29123, 29124, 31236, 29123, 31204, 31204, 29156, 31236,
     31236, 29156, 31236, 29123, 27011, 29123, 31204, 33316, 31204, 31204,
     31204, 29124, 29124, 29124, 29124, 29156, 33316, 31204, 31204, 22818,
     38034, 12678, 10597, 23308, 29484, 27010, 27011, 31269, 46159, 57050,
     16937, 19049, 25389, 12710, 50711, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 65535, 40180, 27534},
    {25388, 6339,  40278, 48729, 48697, 48697, 48729, 48729, 48729, 52923,
     46453, 35953, 0,     0,     0,     65535, 48337, 31237, 29091, 27043,
     33252, 22883, 20868, 20802, 27469, 27076, 29123, 31236, 29124, 27043,
     29124, 27043, 29123, 27043, 31236, 27075, 33284, 31236, 31204, 33284,
     31204, 31204, 31204, 29091, 24931, 29091, 31204, 31236, 31204, 31204,
     29124, 31204, 29123, 31204, 29124, 31204, 31236, 31236, 27044, 27010,
     42259, 20770, 20868, 22883, 24898, 33284, 27011, 33349, 48337, 63422,
     16904, 16904, 6339,  31727, 44373, 65535, 61342, 61342, 61342, 61342,
     61342, 61342, 65535, 21163, 29615},
    {32,    29614, 10598, 44503, 46616, 46584, 46616, 46648, 46648, 48762,
     44405, 35920, 0,     0,     0,     59196, 48434, 33382, 29091, 29123,
     29124, 31204, 31171, 29091, 27371, 27142, 29091, 31204, 29123, 29124,
     29124, 27043, 29124, 27043, 31236, 29123, 31236, 31204, 31204, 33284,
     31268, 31204, 33284, 29123, 27011, 29091, 31204, 29156, 31204, 31204,
     31204, 31172, 29124, 29123, 29124, 29156, 31204, 31204, 29124, 27043,
     40146, 31171, 33284, 31204, 29123, 31204, 27011, 33382, 48434, 59228,
     0,     0,     0,     35921, 42292, 59262, 57116, 55068, 55068, 55068,
     55036, 57148, 44503, 4226,  25388},
    {0,     19017, 12678, 14824, 42390, 46584, 44503, 44503, 46584, 48729,
     33872, 35920, 0,     0,     0,     38098, 52628, 33447, 27011, 29123,
     31204, 31204, 31204, 31171, 25192, 27305, 29091, 31204, 29124, 27076,
     29124, 29123, 27043, 27043, 31204, 29123, 31204, 31204, 31204, 31204,
     33284, 31204, 31236, 29123, 24931, 29123, 29156, 31236, 31204, 29123,
     31171, 31171, 29124, 29124, 29156, 31236, 31236, 29156, 29123, 29222,
     37968, 31171, 33284, 31204, 29124, 29156, 24931, 35496, 52660, 38098,
     0,     0,     0,     33807, 42292, 57181, 55035, 55035, 55003, 55003,
     55035, 50810, 8452,  23275, 4194},
    {0,     0,     35921, 14791, 6404,  27567, 40278, 40277, 40278, 29713,
     12579, 14791, 0,     0,     0,     40179, 52692, 35593, 29059, 29123,
     31172, 31204, 29124, 31172, 25029, 29483, 26978, 31204, 29124, 29124,
     29124, 27043, 27075, 24963, 29123, 29124, 31204, 29124, 31172, 31204,
     31236, 31171, 31171, 31204, 27011, 29123, 29124, 31236, 31204, 31171,
     29124, 31171, 29124, 31204, 29124, 31236, 29156, 29156, 29091, 31465,
     35725, 31204, 31204, 29124, 29123, 29124, 27011, 37641, 52693, 40211,
     0,     0,     0,     33775, 23275, 55068, 52955, 52923, 52922, 55035,
     44471, 8517,  16904, 19017, 0},
    {0,     0,     0,     14824, 29614, 8484,  8484,  10630, 16936, 10532,
     29581, 8420,  0,     0,     0,     42259, 56919, 37673, 29058, 31171,
     29123, 29156, 31204, 31204, 24963, 29614, 26978, 31204, 29123, 29124,
     29123, 27011, 27043, 27011, 31204, 29124, 31204, 31204, 29123, 31204,
     31204, 31204, 31203, 31171, 27011, 29123, 29123, 31236, 29123, 31204,
     31204, 29123, 29124, 31204, 29124, 31236, 29156, 29156, 29091, 35822,
     31400, 31236, 31204, 29124, 29123, 29123, 24930, 37673, 56919, 42259,
     0,     0,     0,     21162, 10498, 21228, 38164, 38132, 36051, 19114,
     4226,  16936, 19017, 0,     0},
    {0,     0,     0,     0,     0,     25356, 16936, 16904, 19017, 21130,
     8420,  0,     0,     0,     0,     38066, 59097, 37640, 29058, 31171,
     29091, 31204, 31204, 31172, 24898, 29614, 26978, 29124, 29091, 29124,
     27043, 27011, 29124, 27011, 27043, 29123, 29124, 31204, 29123, 29123,
     33284, 31171, 31204, 31171, 27011, 31171, 29124, 31204, 29123, 31203,
     29123, 29123, 31204, 29124, 29124, 29156, 31236, 29156, 27010, 40049,
     27109, 31204, 31204, 29124, 27043, 29124, 24898, 35528, 59097, 38066,
     0,     0,     0,     6339,  29614, 19017, 10565, 4226,  12678, 18984,
     25356, 4194,  0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     10565, 63388, 37738, 27010, 29123,
     29091, 31204, 31204, 31204, 26946, 29582, 24963, 29091, 29124, 31236,
     29124, 27011, 29123, 27043, 29123, 31204, 29091, 31204, 29124, 31203,
     33316, 31204, 31204, 31171, 29123, 31204, 29123, 31204, 29123, 31204,
     31204, 29123, 29156, 29124, 29156, 29156, 31204, 29156, 26978, 42227,
     27043, 29156, 31204, 29123, 29091, 29123, 26978, 37706, 65436, 10565,
     0,     0,     0,     0,     0,     19017, 25356, 25388, 25356, 8420,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     12678, 65501, 39786, 26978, 29091,
     29091, 31204, 29124, 31204, 26946, 27403, 27141, 31171, 29124, 31204,
     29124, 27011, 29123, 29091, 29123, 29124, 29123, 29156, 29123, 31204,
     33284, 31236, 31203, 29091, 27043, 29123, 29123, 31236, 31203, 31171,
     31204, 31204, 29156, 29124, 27043, 29156, 31204, 29124, 27011, 42227,
     29058, 31204, 31236, 27043, 29091, 29091, 26978, 37738, 65502, 12678,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     12678, 65534, 39787, 29026, 29091,
     29091, 31204, 29124, 31204, 29058, 27273, 27272, 31171, 29124, 29124,
     29124, 29123, 29091, 27043, 29124, 29124, 31172, 29124, 29091, 29123,
     33284, 31236, 31204, 29091, 29091, 29123, 31204, 31204, 29123, 29123,
     31204, 29124, 29124, 29156, 29124, 31236, 31204, 29123, 29189, 40049,
     29091, 29156, 31236, 27043, 29091, 27043, 26978, 39787, 65535, 12678,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     12710, 65535, 39884, 26946, 29091,
     29091, 31204, 29124, 29124, 29059, 25094, 27403, 29091, 29124, 29123,
     29123, 27011, 29123, 27011, 29124, 29123, 29124, 27043, 29091, 31171,
     31236, 29156, 31171, 29123, 29123, 29123, 29123, 29123, 31204, 31172,
     29156, 31204, 29156, 29156, 29124, 29124, 31204, 29123, 31433, 35757,
     31171, 29156, 31204, 29091, 31204, 29123, 24898, 39852, 65535, 12710,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     14791, 65535, 41899, 29058, 29091,
     29091, 29124, 31172, 29124, 29091, 24996, 29582, 29058, 29124, 29124,
     29091, 27043, 27043, 27011, 29123, 29123, 29124, 27075, 29091, 29123,
     31236, 29123, 31171, 27043, 29091, 29123, 29091, 29123, 31203, 29123,
     31236, 31236, 31204, 29156, 29123, 29156, 31204, 29091, 35757, 31433,
     31203, 29124, 31236, 29091, 31204, 29124, 24898, 39819, 65535, 14791,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2113,  65535, 39819, 29123, 29091,
     29091, 31204, 31204, 31172, 29091, 27011, 29647, 29058, 29091, 29124,
     27043, 27011, 29091, 27011, 29124, 29124, 29091, 29123, 29091, 29123,
     31236, 29124, 29123, 27011, 29091, 29123, 29123, 29123, 31171, 29123,
     31204, 31236, 31204, 29156, 31204, 31236, 29156, 29058, 40048, 29189,
     29124, 29156, 29156, 27043, 31204, 27043, 27043, 39787, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 41932, 31236, 29059,
     29091, 31204, 29091, 31172, 29091, 26978, 31662, 27011, 29123, 29091,
     27043, 27043, 27043, 27011, 29124, 29124, 27043, 29091, 29091, 29091,
     31204, 29123, 29091, 27011, 29091, 31203, 31171, 31204, 31204, 29123,
     31204, 31204, 29156, 29156, 29156, 31204, 31204, 26978, 40179, 27043,
     29124, 29156, 29124, 27043, 31204, 29123, 29156, 39884, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 44078, 31237, 26979,
     31171, 31171, 29123, 29091, 31171, 26946, 29549, 25028, 29123, 29091,
     29091, 27043, 27043, 26979, 29124, 31204, 29124, 29123, 29091, 29091,
     31204, 29123, 27011, 27043, 29091, 31171, 29091, 29123, 31203, 29123,
     31204, 31236, 29124, 29156, 31236, 29124, 31204, 24898, 42292, 24930,
     29124, 31204, 29123, 29091, 31204, 27043, 29189, 44046, 65534, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 44045, 31269, 26978,
     31204, 31171, 29123, 29091, 29091, 29059, 27338, 27207, 27011, 29123,
     29091, 27011, 27043, 27011, 27043, 29124, 29123, 29123, 29091, 27011,
     31204, 29123, 27043, 29091, 29091, 29123, 29091, 29123, 31204, 29091,
     31236, 29156, 31204, 29124, 31236, 29124, 31172, 22915, 42194, 26978,
     29156, 29124, 31204, 27011, 29123, 27011, 29222, 41965, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     33808, 31695, 46486, 40147,
     42292, 29516, 8485,  0,     0,     0,     65535, 41997, 31302, 26978,
     31171, 31172, 29124, 29091, 29124, 29091, 27174, 29451, 27011, 29091,
     29091, 27043, 27011, 27043, 29123, 29124, 27043, 29091, 29091, 29091,
     29123, 29123, 27011, 29059, 31171, 29091, 29091, 29123, 29123, 29091,
     29156, 31204, 31236, 29124, 31236, 31204, 29123, 27207, 37871, 29091,
     29124, 29123, 29123, 27011, 31204, 27011, 29189, 42030, 65535, 0,
     0,     0,     0,     29581, 40146, 35985, 35953, 44372, 33808, 14791,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     6307,  46518, 14759, 6372,  8517,  10565,
     10565, 8419,  40113, 2081,  0,     0,     65535, 44110, 31302, 26978,
     29123, 31171, 31171, 29059, 29123, 29091, 27044, 29581, 26978, 29091,
     29091, 27011, 27011, 27011, 29091, 27043, 29124, 29091, 27011, 29091,
     29091, 27011, 26978, 27011, 29091, 29123, 29091, 29123, 29091, 29123,
     31204, 31204, 31204, 29156, 31236, 29124, 29123, 33612, 31466, 29091,
     29124, 29124, 29124, 27011, 29123, 26978, 29222, 44111, 65535, 0,
     0,     0,     21162, 16805, 10598, 12743, 12743, 12710, 8484,  10565,
     21130, 0,     0,     0,     0},
    {0,     0,     0,     4226,  23211, 4259,  12743, 16936, 16937, 16937,
     16969, 14889, 21063, 14823, 0,     0,     65534, 46224, 31301, 26978,
     29091, 31171, 29123, 26979, 29123, 29091, 27011, 29647, 26978, 29091,
     29123, 27011, 27011, 27043, 27043, 29123, 27043, 29091, 26979, 29091,
     29091, 29123, 26979, 29091, 27011, 29091, 29091, 29123, 29123, 29123,
     29156, 29124, 31204, 29124, 31236, 31204, 27010, 37968, 29222, 29091,
     27043, 27043, 29091, 27011, 29091, 26978, 31270, 46256, 65535, 0,
     0,     0,     40113, 12710, 19082, 19050, 19050, 19050, 19082, 16936,
     6339,  21162, 0,     0,     0},
    {0,     0,     2145,  31727, 6372,  14856, 16969, 16969, 17001, 19050,
     19050, 19115, 23144, 33873, 0,     0,     65534, 48369, 33350, 26946,
     29091, 29091, 26978, 26913, 29058, 29059, 26978, 29647, 24898, 29091,
     29091, 27043, 27011, 27043, 27043, 29124, 27011, 29091, 29059, 29091,
     27011, 31171, 24930, 27011, 29059, 29091, 29091, 29123, 29091, 29091,
     29124, 29124, 29156, 29124, 29124, 31204, 26978, 42227, 24963, 27011,
     26978, 24897, 24930, 24931, 27011, 26978, 31270, 48369, 65535, 0,
     0,     0,     33807, 19082, 21195, 19115, 19115, 19115, 19115, 19114,
     16969, 6339,  35921, 0,     0},
    {0,     0,     27469, 12710, 14856, 19082, 19114, 19115, 21195, 21195,
     21195, 23341, 25257, 31728, 0,     0,     65534, 48401, 33350, 26978,
     29091, 26913, 37674, 48435, 46289, 33350, 26913, 29549, 24996, 29091,
     29091, 29091, 27043, 27011, 29091, 29124, 26979, 27043, 29059, 29091,
     27043, 29091, 26979, 27011, 29059, 29091, 29091, 29091, 29123, 29123,
     29124, 29124, 29156, 31204, 29156, 29091, 24898, 42292, 24832, 33382,
     48370, 46354, 33513, 24865, 27011, 26978, 31302, 48402, 65534, 0,
     0,     0,     35855, 21195, 25421, 23308, 23308, 23308, 23308, 21228,
     21228, 12743, 19049, 10597, 0},
    {0,     0,     42325, 12743, 25454, 25454, 25421, 25454, 25454, 25454,
     25486, 29680, 27370, 31728, 0,     10597, 65535, 48402, 33382, 29058,
     24865, 48501, 59294, 63455, 52858, 40244, 37740, 27273, 27174, 29059,
     29091, 27011, 29091, 27011, 27043, 29091, 27011, 27011, 27011, 27011,
     27011, 29091, 27011, 26979, 29059, 29091, 29091, 29091, 31171, 29123,
     29091, 29124, 29156, 31204, 29156, 29123, 24930, 40114, 37772, 59261,
     63422, 57116, 42325, 40081, 24833, 26978, 31334, 48402, 65535, 0,
     0,     0,     35888, 25421, 29712, 27599, 27599, 27567, 27567, 27567,
     29647, 27599, 8452,  27469, 0},
    {0,     2145,  27502, 25421, 33971, 33906, 31858, 33906, 33906, 33938,
     33938, 36084, 23209, 27534, 38034, 33808, 48598, 50515, 33382, 26913,
     35692, 40212, 55002, 61309, 52825, 46486, 57148, 33644, 27305, 27010,
     27011, 27011, 27011, 26979, 27011, 27043, 27011, 27011, 29059, 27011,
     27011, 29091, 26979, 26979, 29091, 27043, 29091, 29091, 29123, 31172,
     29123, 29124, 29123, 31204, 31204, 29091, 27141, 40146, 40179, 50744,
     61309, 54970, 48599, 50777, 48370, 24832, 31302, 50515, 46550, 31695,
     33808, 33873, 21096, 29680, 38164, 36051, 36051, 36019, 33971, 36019,
     36084, 42358, 12743, 29647, 0},
    {0,     16936, 19050, 42357, 57148, 57148, 57148, 57148, 57148, 57148,
     57149, 61374, 35952, 27468, 35985, 25356, 46485, 50515, 35462, 26945,
     29582, 31727, 54938, 59228, 57115, 59228, 61309, 46453, 29451, 26978,
     29091, 29091, 27011, 27011, 27011, 29091, 26979, 26979, 27011, 27011,
     27011, 29091, 26979, 26979, 29059, 29091, 29091, 29123, 31171, 29123,
     27043, 27076, 29124, 29124, 29123, 29059, 31465, 35986, 27469, 50744,
     59164, 59164, 61309, 59196, 61276, 24832, 33382, 48467, 46485, 19017,
     33808, 35921, 18983, 57116, 61342, 59261, 59229, 59229, 59229, 57149,
     57149, 61342, 19082, 38066, 0},
    {0,     14791, 10533, 55003, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 46387, 31694, 35954, 38099, 54970, 50482, 33382, 26946,
     21162, 19017, 50744, 52857, 59228, 54970, 54970, 48501, 29549, 26978,
     29091, 27011, 27011, 27011, 27011, 27043, 24931, 27011, 27011, 26979,
     27011, 29091, 27011, 26979, 29091, 29091, 29091, 29091, 31171, 29091,
     27043, 27043, 31204, 31172, 29123, 26978, 37902, 25356, 16904, 44373,
     52825, 57083, 59196, 50712, 61309, 24833, 33382, 48434, 55034, 42357,
     38067, 40245, 21031, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 21195, 29614, 0},
    {0,     12711, 12710, 48729, 59229, 59261, 61342, 61342, 61342, 61374,
     61374, 65535, 40113, 16936, 21163, 14824, 44372, 50515, 33350, 26945,
     16871, 8452,  23243, 46518, 44373, 33840, 50777, 42063, 27501, 24898,
     27011, 27011, 24931, 26979, 29091, 27011, 26979, 26979, 29059, 27011,
     26978, 27011, 27011, 24930, 29059, 29059, 29091, 29091, 29091, 29123,
     27043, 27043, 29123, 29091, 29123, 24897, 42227, 18854, 8517,  14791,
     42292, 48599, 35953, 48599, 50711, 22752, 33382, 50515, 38066, 32,
     4226,  4226,  12611, 63422, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 25421, 29615, 0},
    {0,     8452,  14824, 36051, 46648, 46584, 44535, 46584, 46584, 46616,
     46616, 50875, 27435, 16936, 25389, 21163, 46517, 50515, 33350, 26946,
     20868, 8550,  6371,  4258,  19049, 31727, 48632, 24931, 29582, 24931,
     27011, 27011, 26979, 24931, 27011, 27011, 26979, 26978, 26979, 27011,
     26978, 27011, 27011, 26978, 27011, 27011, 29091, 29091, 29123, 29091,
     27043, 27011, 31171, 29091, 29091, 24898, 42260, 22785, 12710, 6339,
     27469, 6371,  29614, 46551, 31433, 24833, 31302, 50515, 44405, 16904,
     25388, 23276, 12677, 44470, 57149, 55036, 55035, 55035, 55003, 55003,
     55068, 61374, 19114, 29614, 0},
    {0,     0,     27534, 14856, 33938, 31825, 31793, 31793, 29713, 31793,
     29745, 36051, 25322, 33840, 6371,  25388, 57115, 48402, 31302, 26914,
     26978, 18788, 12711, 8484,  23275, 31727, 25094, 24865, 29484, 25094,
     29091, 27011, 24931, 27011, 26979, 27011, 24930, 27011, 26978, 26979,
     26978, 27011, 26979, 26978, 27011, 26979, 29091, 29091, 29123, 29091,
     27043, 27011, 31172, 27043, 27011, 24898, 42292, 24865, 18755, 14791,
     29615, 10597, 29614, 27338, 22753, 24866, 31302, 48434, 57115, 19049,
     16936, 6339,  29549, 31760, 40245, 38132, 36084, 36051, 36051, 36084,
     38164, 40277, 8484,  25356, 0},
    {0,     0,     31727, 4226,  19115, 23341, 23341, 23341, 23341, 23341,
     23309, 25454, 23209, 31695, 0,     0,     65534, 48402, 33350, 26914,
     29059, 26946, 20738, 18755, 21032, 29451, 29026, 29026, 27272, 27305,
     27010, 27011, 26979, 27011, 26979, 27011, 24931, 26979, 27011, 26979,
     26978, 27011, 26978, 24930, 26978, 27011, 29091, 29091, 29091, 29091,
     27011, 29091, 31171, 27043, 27011, 25028, 40081, 26946, 29058, 25061,
     29516, 18723, 20770, 24866, 24930, 26946, 31302, 48402, 65535, 0,
     0,     0,     31662, 23308, 27567, 27534, 27534, 27535, 25486, 25454,
     25486, 14856, 12678, 12710, 0},
    {0,     0,     6307,  27501, 6339,  19115, 21228, 21228, 23308, 21228,
     21228, 23341, 19016, 31760, 0,     0,     65534, 48369, 31302, 26914,
     27011, 26978, 26946, 26978, 25029, 33775, 26945, 27011, 25061, 29484,
     29026, 24931, 24930, 26979, 24898, 27011, 26979, 26978, 27011, 24930,
     24898, 27011, 27011, 26978, 24930, 27011, 29091, 29091, 27043, 27043,
     27011, 27043, 29091, 27011, 29059, 31433, 35724, 26978, 26978, 31433,
     31433, 29026, 27011, 24899, 24930, 24898, 31302, 48370, 65535, 0,
     0,     0,     27468, 23276, 25486, 25454, 25454, 25454, 25421, 25421,
     19082, 2145,  35986, 0,     0},
    {0,     0,     0,     14791, 21162, 6371,  19050, 21163, 19115, 19115,
     21163, 19082, 16805, 14791, 0,     0,     65534, 48369, 31269, 26914,
     26978, 26978, 26946, 26978, 24931, 33840, 24865, 27011, 24898, 31695,
     26946, 27011, 24898, 26978, 24898, 27011, 26979, 26978, 27011, 26979,
     24930, 27011, 27011, 27011, 26979, 27011, 27043, 29091, 29091, 27011,
     27011, 29059, 29091, 24931, 26978, 37870, 29288, 26978, 26978, 33645,
     29222, 29059, 27011, 24898, 26979, 26946, 31269, 46321, 65534, 0,
     0,     0,     29549, 12743, 23309, 23308, 23308, 23308, 23308, 16969,
     4226,  23210, 32,    0,     0},
    {0,     0,     0,     0,     10597, 25388, 4259,  10565, 12743, 12711,
     12710, 8419,  21096, 12710, 0,     0,     65534, 46289, 31237, 24866,
     26979, 26978, 24898, 26978, 26946, 35921, 24898, 26979, 26978, 29647,
     26946, 27011, 24898, 26978, 22818, 24930, 26979, 24898, 26979, 26979,
     26978, 27011, 29059, 26978, 26978, 27011, 29091, 27011, 29091, 27043,
     24963, 27011, 27011, 27011, 26946, 40114, 24963, 27011, 26978, 35822,
     27076, 27011, 27011, 24898, 24898, 26946, 31269, 46289, 65535, 0,
     0,     0,     31727, 10466, 10630, 16936, 16936, 14856, 8484,  6339,
     27501, 32,    0,     0,     0},
    {0,     0,     0,     0,     0,     4226,  27501, 21130, 21130, 23276,
     27502, 25355, 19049, 0,     0,     0,     65534, 46256, 29157, 24866,
     26979, 26978, 24866, 24898, 24833, 33840, 24931, 26979, 26978, 29582,
     24931, 27010, 24898, 26978, 24898, 26978, 24898, 24930, 27011, 26978,
     24898, 27010, 29059, 27011, 27011, 26979, 29059, 29059, 27043, 29091,
     24931, 26979, 27011, 24931, 24897, 44373, 22753, 27011, 26946, 35888,
     26978, 27011, 24931, 24898, 24898, 24866, 31237, 44176, 65535, 0,
     0,     0,     6372,  31727, 21129, 12710, 8484,  19049, 23243, 27501,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     2113,  8484,  2113,
     4226,  4259,  0,     0,     0,     0,     65534, 44143, 29157, 26946,
     26978, 26978, 22818, 24898, 26913, 33743, 25029, 26978, 29058, 29516,
     25029, 26978, 22850, 24898, 24898, 26978, 24930, 26978, 29059, 26978,
     24898, 29059, 29059, 26978, 27011, 27011, 29059, 27011, 27011, 29059,
     24931, 26979, 27011, 27011, 24898, 42227, 22785, 29091, 24865, 37969,
     26913, 27011, 27011, 22850, 24898, 24898, 29189, 42063, 65535, 0,
     0,     0,     0,     0,     6372,  2113,  2113,  8484,  2145,  0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 41998, 29157, 24866,
     26946, 29026, 22818, 24898, 28994, 29451, 25159, 24898, 29058, 27273,
     27272, 26946, 22818, 24930, 24898, 26979, 24898, 24898, 26978, 26978,
     24898, 26978, 26978, 27011, 27011, 26979, 27011, 27011, 26979, 27011,
     24931, 24898, 27011, 24930, 27044, 40114, 24865, 27011, 26979, 35855,
     26946, 26979, 27011, 22818, 24898, 24898, 29157, 41966, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 41965, 29157, 26946,
     26978, 26978, 24898, 26978, 26946, 27305, 29451, 24898, 29059, 25061,
     29451, 22817, 22818, 24898, 24898, 24930, 26978, 24898, 26978, 26978,
     24898, 26978, 29059, 27010, 26978, 29059, 27011, 27011, 26978, 27011,
     24930, 24930, 24930, 26978, 29288, 35789, 24865, 26946, 27109, 33677,
     26978, 26979, 29059, 24898, 24898, 24866, 29189, 39885, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65535, 41933, 29157, 26946,
     26978, 26979, 26946, 26979, 26978, 25062, 31662, 24865, 27011, 22883,
     31662, 22785, 24898, 26978, 22818, 26979, 26946, 24898, 24898, 24898,
     24898, 24898, 29026, 27011, 26979, 29059, 27011, 29059, 24930, 27011,
     24898, 26979, 26979, 26978, 35725, 29353, 24898, 26978, 29288, 31498,
     26946, 27011, 26979, 24898, 22818, 24866, 29157, 39853, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 41965, 29124, 26946,
     26946, 26978, 24898, 26979, 26978, 22883, 33840, 22785, 29059, 22785,
     31727, 22785, 24898, 24898, 24866, 24898, 26946, 24898, 24898, 24898,
     24898, 26978, 26978, 26978, 27010, 26979, 26978, 27011, 24930, 26979,
     24898, 29059, 26979, 26913, 40048, 25029, 26978, 26946, 33611, 27174,
     26946, 24930, 24930, 22818, 22818, 24866, 27076, 41933, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 41932, 27044, 26946,
     26978, 26946, 24898, 29059, 26978, 22818, 35986, 22753, 26979, 24865,
     31695, 22850, 22818, 24898, 22818, 24898, 26978, 24898, 24898, 24898,
     24898, 24898, 26978, 26978, 26978, 26978, 26978, 26979, 26978, 26978,
     22818, 29059, 26978, 22753, 42260, 22818, 26978, 24865, 35790, 24964,
     26946, 26978, 26978, 22818, 24898, 24866, 27012, 39852, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     65534, 39787, 24931, 26946,
     26978, 26978, 24898, 26978, 26978, 22785, 35921, 22818, 26979, 26946,
     29517, 22916, 22818, 24898, 22818, 24930, 26946, 24898, 24898, 26946,
     24866, 24898, 26978, 26978, 26978, 26978, 26978, 26978, 26978, 24930,
     24866, 26979, 26978, 22818, 42260, 24833, 26979, 24865, 35855, 24931,
     24898, 24898, 24898, 22818, 22818, 24866, 24931, 37707, 65535, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     33,    65535, 35594, 24866, 26946,
     26978, 24866, 26946, 24898, 26978, 24833, 33808, 22883, 24898, 26946,
     29418, 25127, 22785, 24898, 22786, 24898, 24898, 24898, 26946, 24898,
     24866, 22818, 26946, 26978, 26946, 26978, 24898, 26978, 24930, 26978,
     24866, 26978, 26946, 24931, 40114, 24833, 27011, 24865, 37968, 24833,
     24898, 24898, 24898, 24866, 22818, 24866, 22818, 35594, 65535, 33,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     14791, 65535, 37674, 22785, 26946,
     26946, 26946, 26978, 26946, 24898, 24865, 31629, 25094, 26946, 24898,
     27142, 27370, 22785, 24898, 22818, 24898, 24866, 24898, 24898, 24898,
     24866, 24866, 24898, 26978, 26946, 26978, 26978, 24898, 26978, 24898,
     24866, 26978, 22785, 27174, 37871, 24833, 26979, 22785, 38001, 24833,
     24898, 24898, 26978, 22818, 22818, 24866, 20673, 35626, 65535, 14791,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     4226,  40147, 31695, 23275, 27469,
     25356, 0,     0,     0,     0,     12678, 65535, 37739, 22753, 24866,
     24898, 26946, 26978, 24898, 26978, 26914, 27306, 29386, 26946, 26978,
     24964, 29549, 22753, 24866, 20738, 24898, 24898, 24898, 26978, 26946,
     22818, 22786, 24898, 24898, 24898, 26978, 24898, 24930, 24898, 24898,
     24898, 26978, 20705, 33579, 31466, 24865, 24898, 24931, 35822, 26913,
     24898, 22818, 26946, 24866, 22818, 22818, 20672, 37739, 65535, 12678,
     0,     0,     0,     0,     0,     23243, 19049, 25324, 0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     6307,  29582, 29549, 10598, 19082, 19082, 17002,
     12645, 35920, 2081,  0,     0,     12710, 65535, 37707, 22753, 24866,
     26946, 26946, 26978, 26946, 26946, 24866, 23014, 31597, 26913, 26979,
     22786, 33808, 20672, 22818, 20738, 24898, 24898, 24866, 26978, 24898,
     24866, 22786, 24898, 24898, 24898, 24898, 26978, 26978, 22818, 24898,
     24866, 24898, 22753, 37936, 27109, 24866, 26946, 27142, 33612, 24833,
     24898, 22818, 24898, 22786, 22786, 22786, 22720, 37674, 65535, 12710,
     0,     0,     0,     25388, 35888, 21162, 23243, 23243, 35889, 16936,
     4226,  0,     0,     0,     0},
    {0,     0,     8452,  33808, 8484,  27567, 42423, 44471, 44471, 44504,
     36052, 23209, 10565, 0,     0,     12678, 65534, 35562, 22753, 24866,
     26946, 26946, 26946, 26946, 26946, 24866, 22916, 33807, 22785, 26979,
     22753, 31727, 20705, 24866, 20738, 24898, 24898, 24866, 26946, 24866,
     22818, 24866, 24866, 24898, 24898, 24898, 24898, 26978, 24898, 24898,
     24866, 24898, 20672, 42195, 22850, 26946, 24865, 31433, 29288, 24866,
     24866, 24866, 24866, 24898, 22786, 22786, 22720, 33481, 65534, 12678,
     0,     0,     4194,  25290, 19082, 40245, 38164, 38197, 27567, 8517,
     19017, 23243, 0,     0,     0},
    {0,     0,     38034, 8484,  38164, 46616, 46584, 46584, 46584, 46584,
     50875, 33807, 2113,  0,     0,     12678, 65501, 35561, 22753, 24866,
     26946, 26946, 24898, 24866, 26978, 24866, 20737, 35954, 22753, 26946,
     22753, 31629, 22851, 22786, 20706, 24866, 24866, 24866, 24898, 24866,
     22818, 20738, 24866, 24898, 24898, 24898, 24898, 24898, 24898, 22786,
     24866, 24898, 20672, 44372, 22752, 26978, 24833, 35757, 24996, 24898,
     22818, 24866, 24866, 24866, 22786, 22786, 20672, 33481, 65501, 12678,
     0,     0,     29647, 23209, 48762, 46616, 46616, 46584, 46616, 46584,
     19114, 12646, 23275, 0,     0},
    {0,     19050, 14823, 38132, 50810, 48729, 48729, 50777, 50810, 50809,
     55068, 35887, 2146,  0,     0,     10597, 65436, 35561, 22721, 24866,
     26946, 26914, 22752, 22720, 26914, 24898, 22753, 36018, 20672, 26978,
     22785, 27338, 23014, 22753, 20705, 22818, 24866, 22786, 24866, 24866,
     22818, 20738, 24866, 24898, 24898, 24866, 24898, 24866, 24898, 22786,
     22818, 24866, 22819, 40146, 22752, 26946, 24833, 37936, 22786, 24898,
     22785, 22752, 20672, 22753, 22786, 22753, 20640, 33481, 65436, 10565,
     0,     0,     29615, 29613, 55068, 50809, 50809, 48729, 48729, 48729,
     48761, 17001, 25356, 4194,  0},
    {0,     38099, 27534, 55068, 52955, 52955, 52955, 52955, 52955, 52955,
     59294, 35855, 19082, 0,     0,     23275, 61210, 33416, 22721, 22786,
     24800, 27011, 42031, 42031, 31238, 22720, 24833, 35921, 22818, 26978,
     22785, 25127, 27338, 20673, 22786, 22786, 24898, 22786, 24866, 24866,
     22818, 20705, 24866, 24866, 24866, 22818, 24898, 22818, 24898, 22786,
     22786, 24866, 25029, 37903, 24833, 26946, 22753, 35888, 22785, 24800,
     27012, 42031, 42063, 27077, 20608, 22753, 20672, 31336, 59130, 33872,
     0,     0,     27534, 31661, 59262, 52955, 52923, 52923, 52922, 52922,
     52922, 50810, 8517,  25356, 0},
    {4193,  27469, 46584, 61374, 59229, 59229, 59229, 59229, 59229, 59229,
     63487, 31661, 10598, 16904, 31695, 33840, 56952, 31335, 22753, 22720,
     39820, 61374, 63455, 57083, 42357, 35790, 20544, 33808, 22916, 24866,
     24866, 22883, 29549, 20672, 20706, 22786, 26946, 22786, 24898, 24866,
     22818, 20705, 22818, 22818, 24898, 24866, 24898, 24898, 24898, 22786,
     24866, 24833, 31466, 31499, 24865, 24898, 22720, 38001, 20576, 37772,
     61342, 63455, 57116, 42358, 35790, 18528, 20672, 29255, 56920, 44405,
     25388, 21130, 38034, 29549, 63487, 59229, 59229, 57181, 57181, 57149,
     57181, 61407, 29647, 40212, 0},
    {19017, 16904, 55036, 65535, 63455, 63455, 65535, 65535, 65535, 65535,
     65535, 37968, 35953, 44405, 42260, 50711, 52693, 33448, 20640, 29157,
     42358, 52825, 61309, 54938, 46486, 52890, 41901, 29386, 25159, 26914,
     24866, 20738, 31727, 20640, 22786, 22786, 26946, 24834, 22786, 24866,
     22818, 20705, 24866, 22786, 24866, 22818, 24866, 24866, 22818, 22786,
     22786, 22753, 37871, 27110, 24865, 24866, 22753, 37968, 27044, 42325,
     52825, 61309, 54970, 46486, 52890, 39885, 16448, 33448, 52694, 50744,
     38099, 31695, 27534, 31694, 65535, 63455, 63454, 63422, 63422, 63422,
     63422, 65535, 46551, 21163, 0},
    {16968, 8452,  57181, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 42193, 29581, 29647, 29647, 46550, 50515, 31335, 20640, 31532,
     27534, 52857, 59196, 57083, 59196, 59228, 59098, 22981, 31532, 24833,
     24898, 22721, 33808, 20673, 20705, 22786, 26914, 22786, 22818, 24866,
     22786, 20705, 22818, 22786, 24866, 22786, 24866, 24866, 24866, 22786,
     22786, 22752, 40114, 22851, 26914, 24833, 24964, 35692, 27338, 29614,
     50776, 59196, 57083, 59228, 59196, 59130, 16416, 31303, 50548, 33872,
     14823, 23275, 21129, 42259, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 48664, 12678, 19017},
    {16904, 8420,  44471, 46616, 50809, 50810, 50842, 52890, 52922, 52922,
     57181, 33774, 21162, 25389, 19082, 42259, 50515, 29190, 20640, 23210,
     16936, 46550, 52857, 59196, 59164, 52857, 61341, 18657, 33775, 24833,
     24866, 22720, 31695, 20770, 22753, 20705, 24866, 22786, 22818, 24866,
     24866, 20705, 22786, 22786, 24866, 22786, 24866, 24866, 24866, 22786,
     22786, 20640, 44340, 20672, 24866, 24833, 27207, 31433, 23145, 16969,
     46518, 52857, 59164, 59228, 50744, 59164, 18560, 29157, 50483, 48663,
     27566, 31695, 25355, 40113, 65535, 65535, 65535, 65535, 65535, 65535,
     65535, 65535, 44438, 8452,  16936},
    {19049, 10565, 23341, 25486, 25421, 23341, 23341, 23341, 23341, 23341,
     25454, 16870, 16871, 16936, 16969, 48631, 48402, 29125, 22720, 20967,
     8517,  19049, 48599, 48631, 35985, 48631, 50679, 18560, 35953, 22720,
     22818, 22721, 29484, 22949, 22753, 20705, 20705, 22786, 22786, 22786,
     22786, 20705, 20738, 24834, 22785, 24834, 24866, 22786, 22818, 22786,
     22785, 20705, 42260, 20640, 24866, 22753, 33611, 25029, 18886, 10565,
     19017, 46486, 50744, 38066, 48664, 50679, 16448, 27077, 48402, 40179,
     4193,  6307,  8452,  21097, 44471, 42358, 42358, 40277, 40277, 40277,
     40278, 44503, 33873, 10598, 16936},
    {2081,  19049, 10630, 14856, 14824, 14823, 14791, 14791, 14823, 14791,
     14824, 18983, 27469, 31727, 16936, 54969, 46256, 26979, 22753, 20738,
     10630, 6371,  6307,  16936, 29614, 46551, 29353, 20608, 36051, 20672,
     22786, 22753, 27240, 27305, 20673, 20705, 22785, 20705, 24834, 22786,
     22786, 20705, 22786, 22785, 22785, 22785, 24866, 20737, 22786, 22785,
     22753, 27044, 35888, 22720, 24866, 20672, 37935, 22786, 20770, 10630,
     8452,  19049, 8484,  31695, 48632, 29321, 18528, 22851, 46225, 46485,
     16937, 33840, 27501, 12645, 19082, 19082, 17002, 17002, 17002, 19050,
     19082, 21228, 14856, 21163, 0},
    {0,     33808, 4258,  10598, 10598, 10598, 10598, 10597, 10597, 10598,
     10598, 18983, 33,    0,     0,     65535, 44111, 24899, 22753, 22753,
     18756, 12743, 8452,  21162, 35986, 27338, 20640, 22720, 36018, 22753,
     22786, 22753, 22916, 29549, 20640, 20705, 22785, 20705, 22786, 22753,
     22785, 22753, 24833, 22753, 24833, 22785, 24834, 22786, 22786, 24834,
     20641, 29353, 31564, 22752, 24866, 22720, 38001, 20672, 24833, 16675,
     14856, 25356, 10598, 31760, 29419, 16448, 20673, 22851, 42031, 65535,
     6339,  0,     29647, 14725, 12711, 10630, 12678, 10630, 10630, 10630,
     12678, 12710, 6307,  29614, 0},
    {0,     14823, 12678, 8452,  10597, 10597, 10597, 10597, 10597, 10597,
     10597, 18918, 2146,  0,     0,     65535, 37739, 24899, 20705, 22785,
     24833, 16577, 16675, 18919, 35856, 18496, 24834, 22720, 33808, 22884,
     22785, 20705, 18657, 33840, 18560, 18625, 22785, 20705, 22785, 24834,
     20738, 12514, 10466, 12514, 18690, 24834, 24833, 22785, 22786, 22785,
     18560, 35790, 27143, 24833, 24834, 22720, 35921, 22688, 22785, 20673,
     22981, 23145, 14627, 14497, 18560, 20673, 20673, 22818, 35659, 65535,
     0,     0,     29615, 12612, 10598, 10597, 10598, 10597, 10597, 10597,
     10597, 8485,  8452,  16936, 0},
    {0,     0,     29582, 8452,  8452,  10597, 10597, 10597, 10597, 10597,
     10565, 16805, 4226,  0,     0,     65535, 35594, 22786, 20673, 24833,
     22786, 22785, 22753, 18691, 40147, 20608, 22785, 22720, 31597, 23014,
     22753, 20705, 18592, 35953, 18528, 20673, 22785, 20705, 22786, 10434,
     6274,  6273,  4160,  6241,  6274,  8354,  20770, 24833, 22785, 24834,
     20608, 40114, 20771, 22785, 24866, 20640, 38001, 22720, 22786, 22721,
     29321, 25029, 20672, 22753, 20673, 20673, 20673, 22754, 35562, 65535,
     0,     0,     31727, 10531, 10598, 10597, 10597, 10597, 10597, 10597,
     10565, 4194,  33840, 0,     0},
    {0,     0,     0,     27469, 14791, 6339,  8484,  8517,  8485,  8484,
     8452,  18950, 10565, 0,     16904, 65535, 33448, 22721, 20705, 22785,
     24834, 22785, 22753, 16577, 40179, 18592, 22786, 22721, 27305, 27306,
     22720, 20705, 20608, 35888, 18658, 18625, 22785, 20706, 8354,  6274,
     8321,  8386,  33808, 14693, 6241,  8322,  6274,  20738, 22785, 24834,
     18528, 42260, 18593, 24833, 24833, 20706, 35855, 22688, 22785, 22720,
     29419, 24932, 20705, 22753, 20673, 20673, 20673, 20640, 31400, 65535,
     19017, 0,     19049, 14692, 8484,  10565, 10565, 10565, 10565, 8452,
     4226,  29582, 4194,  0,     0},
    {0,     0,     0,     0,     14791, 23275, 19049, 14823, 16904, 23276,
     27403, 33808, 0,     0,     16904, 65535, 33449, 20608, 22753, 22785,
     22785, 20705, 22785, 16512, 40212, 18625, 22753, 22753, 22982, 31629,
     20640, 20705, 18560, 31629, 20868, 18593, 22753, 10434, 8322,  8354,
     6241,  16839, 25389, 23243, 6209,  8322,  8322,  6306,  22786, 22785,
     18560, 44373, 16480, 24834, 24833, 25062, 31531, 22720, 22785, 20672,
     31532, 20705, 22753, 20705, 20673, 20673, 20673, 18528, 31401, 65535,
     16904, 0,     6307,  31694, 12645, 6371,  6339,  6339,  6339,  19017,
     29582, 6307,  0,     0,     0},
    {0,     0,     0,     0,     0,     0,     10565, 21130, 27469, 23243,
     4259,  0,     0,     0,     14823, 63355, 31368, 20640, 22753, 22753,
     22785, 22785, 22753, 16448, 40212, 20738, 22753, 24801, 20803, 35888,
     20640, 22753, 18560, 27273, 27305, 18592, 16610, 6274,  8354,  8322,
     8322,  6273,  8452,  6274,  8321,  8321,  8321,  6273,  14530, 22753,
     20771, 40114, 16480, 22785, 22721, 29353, 27208, 22721, 22753, 22688,
     33710, 18560, 22753, 20673, 20673, 20673, 20673, 18560, 31336, 63356,
     14823, 0,     0,     8452,  25388, 31727, 25356, 35921, 21162, 12678,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     27468, 59097, 33416, 20640, 20705, 20705,
     22753, 22753, 22753, 16448, 38034, 22916, 22753, 22753, 18592, 38034,
     18560, 22753, 18592, 20901, 33677, 18560, 10402, 8322,  8322,  8322,
     8322,  8322,  8321,  8321,  8321,  8322,  8322,  8321,  6274,  22721,
     27207, 33645, 18560, 22785, 20640, 33677, 22884, 22753, 22785, 22688,
     31662, 18592, 22753, 20673, 20673, 20673, 20673, 18560, 31336, 59097,
     27468, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     46518, 52629, 31335, 22688, 20673, 22753,
     22753, 22753, 22753, 18528, 33775, 25095, 22721, 24833, 18528, 38099,
     18592, 20673, 20673, 16545, 35921, 16480, 6274,  8322,  8322,  8322,
     8322,  8322,  8321,  8321,  8322,  8322,  8322,  8322,  6274,  16512,
     35693, 27208, 18592, 22785, 20640, 37969, 18560, 22753, 22753, 20673,
     33678, 20640, 22753, 20673, 18625, 20673, 18593, 18528, 31336, 50580,
     46518, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     44405, 48370, 31335, 20608, 20673, 22753,
     22753, 22753, 22753, 16480, 31564, 27338, 22688, 24833, 20608, 33873,
     18658, 20673, 18625, 16512, 38099, 12352, 6274,  8322,  8322,  8322,
     8321,  8321,  8322,  8322,  8322,  8322,  8322,  8322,  6273,  10304,
     42162, 18691, 18593, 22753, 20608, 38033, 18528, 20673, 22753, 20706,
     31564, 20640, 22753, 18593, 20673, 20673, 20641, 16480, 29255, 46322,
     44405, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     42292, 46289, 24964, 20640, 20673, 22753,
     22753, 22753, 20673, 18560, 27305, 31564, 20608, 22753, 20640, 31695,
     22917, 20673, 18625, 16448, 38066, 10337, 6274,  8322,  8321,  8321,
     8321,  8322,  8322,  8322,  8322,  8322,  8321,  8321,  6273,  8224,
     44340, 16512, 18593, 22753, 20673, 37936, 18560, 20673, 20673, 22851,
     29386, 18592, 20705, 20673, 18625, 18593, 20641, 18560, 22852, 44177,
     50711, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     65535, 42031, 22786, 20641, 18593, 22721,
     22753, 22753, 22753, 16512, 25062, 35856, 20608, 22753, 20640, 29451,
     25160, 20640, 20673, 16448, 35823, 12580, 6241,  8321,  8321,  8322,
     8322,  8322,  8322,  8322,  8321,  8321,  8321,  8321,  8321,  4096,
     46421, 16448, 18625, 22753, 20705, 35856, 18560, 20673, 20673, 25029,
     27240, 20640, 22753, 20641, 18593, 18593, 18593, 18560, 20706, 42031,
     65535, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     65535, 37740, 18625, 20641, 18593, 20673,
     22753, 20705, 20673, 18593, 20836, 38066, 18528, 22753, 20640, 25128,
     29452, 18560, 18593, 16480, 29451, 18984, 6241,  8322,  8322,  8322,
     8322,  8322,  8322,  8321,  8321,  8321,  8321,  8321,  6241,  6241,
     44307, 16448, 18593, 20673, 22852, 33677, 20608, 20673, 20640, 27208,
     22981, 18592, 22753, 18593, 18593, 18593, 18593, 18560, 18593, 35692,
     65535, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     65535, 29255, 16512, 20641, 18593, 20673,
     22753, 22753, 20673, 18593, 20706, 40212, 18528, 22753, 20673, 20836,
     33775, 18528, 20673, 16512, 22982, 27469, 6208,  8322,  8322,  8322,
     8322,  8322,  8321,  8321,  8321,  8321,  8321,  6273,  6241,  14758,
     37871, 16480, 18593, 20640, 27240, 29353, 20640, 20673, 20640, 29418,
     20771, 20641, 22753, 18593, 18593, 18593, 18593, 18593, 16480, 27175,
     65535, 4259,  0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     2113,  65535, 29223, 16448, 18593, 20641, 20673,
     20673, 22721, 20673, 18593, 20641, 40212, 16480, 22753, 20673, 18626,
     38033, 16480, 18593, 16512, 18658, 33873, 4128,  8321,  8322,  8322,
     8321,  8321,  8321,  8321,  8321,  6273,  6273,  8321,  6208,  25323,
     29321, 18560, 20641, 20608, 31499, 25029, 20640, 20673, 22688, 29484,
     18658, 20673, 20673, 18593, 18593, 18593, 18593, 18560, 16448, 27175,
     65535, 12711, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     65535, 44144, 31400, 16448, 18593, 20673,
     20673, 20673, 20673, 20673, 18560, 40244, 18658, 24801, 20673, 16512,
     35986, 16512, 20641, 16545, 14368, 40212, 4128,  8322,  8321,  8321,
     8321,  8321,  8321,  8321,  6273,  8321,  8321,  8321,  4128,  35921,
     22884, 18592, 18593, 20608, 35790, 20738, 20673, 20673, 20608, 31597,
     18592, 20673, 20673, 18593, 16545, 18561, 16512, 16416, 31401, 42096,
     65535, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     19049, 65535, 39886, 31401, 16448, 20641,
     20673, 20673, 18593, 20673, 20576, 38034, 18723, 22721, 20673, 16448,
     38066, 16545, 20641, 16545, 14368, 40212, 4128,  8321,  8321,  8321,
     8321,  8321,  6273,  6273,  8321,  8321,  8321,  8321,  4096,  42292,
     14432, 20641, 18593, 18528, 38001, 16480, 20673, 18593, 18560, 33710,
     16480, 20673, 20673, 18593, 18593, 18560, 16416, 33449, 39918, 65535,
     19049, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     23210, 65535, 37837, 31368, 16448,
     20673, 20673, 20641, 20673, 20608, 35888, 23014, 20640, 18593, 18528,
     33808, 20803, 20640, 18593, 16448, 35953, 8387,  6241,  8321,  8321,
     6273,  6273,  8321,  8321,  8321,  8321,  8321,  6273,  4096,  42325,
     14400, 20673, 18593, 20608, 38001, 16448, 20673, 20673, 18560, 33710,
     16480, 20641, 18593, 18593, 18593, 14368, 29288, 37838, 65535, 23211,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     23243, 65535, 37772, 29223,
     18528, 20673, 18593, 20641, 20608, 31597, 27273, 20608, 18593, 18528,
     29517, 23015, 18560, 16513, 16480, 27501, 16871, 6241,  6273,  6273,
     6273,  8321,  8321,  8321,  8321,  6273,  6273,  6241,  4160,  42292,
     12320, 18593, 18593, 20673, 35856, 18528, 22721, 18593, 20641, 31565,
     16480, 18593, 20641, 18561, 16448, 27175, 35692, 65535, 23243, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     23243, 65535, 33514,
     27077, 18528, 18561, 20641, 20608, 27306, 29484, 20608, 20641, 18560,
     27305, 27338, 18560, 18593, 16480, 16936, 27436, 4128,  6273,  8321,
     8321,  8321,  8321,  8321,  6273,  6241,  6241,  6241,  12612, 33873,
     12352, 20641, 18592, 20739, 35790, 18528, 20673, 18592, 22819, 29419,
     18528, 20641, 18593, 16448, 24997, 31466, 65535, 23243, 0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     46518, 65534,
     33449, 20706, 18560, 20641, 20608, 25127, 33775, 18528, 20641, 18560,
     22982, 31629, 16480, 18593, 16480, 8419,  35921, 4128,  8321,  8321,
     8321,  8321,  6273,  6241,  6241,  6273,  6273,  6208,  23210, 23243,
     14432, 20673, 20640, 25030, 31499, 18528, 20673, 18592, 24997, 27273,
     16480, 18593, 20640, 16545, 33449, 65534, 46518, 0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     50744,
     56920, 33481, 16416, 18593, 20608, 20836, 38066, 16448, 18593, 20640,
     18691, 35921, 16448, 18593, 16513, 4160,  42260, 4096,  8321,  6273,
     6273,  6241,  6241,  6273,  6273,  6273,  6273,  4128,  33775, 12645,
     14464, 20673, 18560, 29386, 27175, 18560, 22721, 18560, 29255, 22982,
     18560, 18560, 16416, 31433, 56952, 50743, 0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     4226,
     65535, 39951, 31336, 14368, 18560, 18626, 40245, 14336, 18561, 20641,
     16513, 38098, 16448, 18593, 16513, 2048,  42325, 4160,  6241,  6241,
     6241,  6273,  6273,  6241,  6241,  6241,  6241,  2048,  42292, 4193,
     14465, 18593, 18528, 33677, 22852, 18592, 20673, 18528, 31466, 18723,
     16512, 14368, 31368, 39951, 65535, 4226,  0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     14759, 65535, 31433, 20771, 18528, 16512, 42357, 14368, 18561, 20641,
     16448, 38099, 16512, 18560, 16513, 2048,  40146, 8354,  6241,  6241,
     6241,  6241,  6241,  6241,  6241,  6241,  6241,  4096,  44405, 0,
     14465, 20673, 18528, 35856, 18625, 18593, 18593, 16480, 31532, 16578,
     16480, 20771, 31433, 65535, 14791, 0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     44372, 54904, 29321, 16416, 14400, 42325, 14432, 18560, 20641,
     16448, 35953, 18658, 16480, 14465, 4128,  31662, 14758, 6208,  6241,
     6241,  6241,  6241,  6241,  6241,  6241,  6241,  4161,  42292, 2048,
     14497, 20641, 16448, 38034, 16448, 18593, 18593, 16448, 31564, 16512,
     14400, 29320, 56952, 44372, 0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     65535, 35724, 27110, 12288, 40179, 16610, 16480, 18593,
     16416, 31694, 22949, 16480, 14465, 4128,  23210, 25323, 4128,  6241,
     6241,  6241,  6241,  6241,  6241,  6241,  6209,  8419,  38033, 2048,
     12417, 20641, 16480, 38001, 16448, 18593, 18593, 14400, 33710, 14368,
     25029, 35725, 65535, 0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     27501, 61275, 29288, 14336, 35921, 18789, 16480, 16513,
     16448, 27371, 25193, 14400, 12385, 4161,  12645, 33808, 4096,  6241,
     6241,  6241,  6241,  6241,  6241,  6241,  4128,  19016, 27469, 2048,
     12417, 18592, 18658, 35823, 16448, 18593, 18593, 14368, 31630, 14368,
     27240, 59195, 27501, 0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     54937, 37870, 20836, 31597, 25160, 16480, 16512,
     14400, 23079, 29516, 14400, 12385, 4193,  6241,  42260, 2048,  6241,
     6241,  6241,  6241,  6241,  6241,  6241,  2048,  29614, 16839, 4128,
     10369, 18560, 20836, 31564, 16448, 18593, 16513, 12320, 31630, 22916,
     37838, 65534, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     14823, 57082, 27143, 25225, 29452, 12320, 16513,
     14432, 18756, 35888, 14368, 12385, 4193,  2048,  44405, 4128,  6241,
     6241,  6241,  6241,  6241,  6241,  6241,  2048,  38066, 8354,  4160,
     10337, 16480, 25160, 27273, 16480, 16545, 16512, 14465, 29451, 27207,
     57082, 14791, 0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     61276, 31498, 29451, 31662, 12288, 16512,
     16480, 16545, 38099, 12288, 10337, 4193,  2048,  42292, 6273,  4161,
     6241,  6241,  6209,  6209,  6209,  6209,  2048,  46486, 2048,  4161,
     8289,  16480, 31564, 20837, 16512, 16513, 16512, 16643, 33709, 31499,
     61276, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     33807, 48533, 25225, 35953, 12288, 14432,
     16480, 14432, 40180, 12320, 10337, 6241,  2048,  33840, 12645, 4128,
     6209,  6209,  6209,  6209,  6241,  6209,  4160,  42292, 2048,  4161,
     8289,  16448, 33710, 18658, 16512, 16512, 16480, 20902, 29483, 48500,
     33807, 0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     8452,  61276, 23047, 40179, 10240, 14464,
     14464, 14368, 40179, 12417, 8289,  4193,  4096,  25323, 23210, 4096,
     6241,  6241,  6241,  6241,  6241,  4160,  8387,  38066, 2048,  4161,
     6241,  16448, 37969, 14432, 16513, 16512, 14400, 25160, 27273, 61276,
     8452,  0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     54937, 27273, 42325, 10240, 14432,
     14432, 14368, 35921, 16676, 6208,  4193,  4128,  14790, 33807, 2048,
     6241,  6241,  6241,  6241,  6241,  4128,  16871, 29581, 2048,  4161,
     4161,  14368, 40114, 14368, 16513, 16513, 14368, 29451, 29419, 54937,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     44372, 40016, 40244, 10272, 14432,
     14464, 14368, 31629, 23047, 4128,  6209,  4161,  8354,  42260, 2048,
     6241,  6241,  6241,  6241,  6241,  2048,  27469, 18984, 4096,  4161,
     4160,  16513, 35888, 14368, 16513, 16513, 14400, 29419, 42129, 44372,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     27501, 50678, 40147, 12417, 14432,
     14432, 14400, 27273, 27403, 4096,  6209,  6209,  2048,  46486, 2048,
     6209,  6241,  6241,  6241,  6241,  2048,  35953, 10499, 4128,  4161,
     4128,  16675, 33710, 16448, 16513, 18560, 14368, 27339, 52791, 27501,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     10597, 59162, 35888, 16643, 14432,
     14432, 14432, 18821, 33808, 2048,  6209,  6209,  2048,  44373, 6241,
     4160,  6241,  6209,  6241,  6241,  2048,  44373, 4128,  4160,  6209,
     4128,  18886, 29419, 16480, 16513, 16513, 14368, 29484, 59195, 10597,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     61308, 33808, 20967, 14400,
     16512, 14432, 16578, 38099, 2048,  6209,  6241,  2048,  35953, 12613,
     4128,  4128,  4128,  4096,  6209,  2048,  44405, 2048,  6209,  6209,
     4096,  23210, 25062, 18560, 16513, 16513, 12288, 33743, 63356, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2081,  65469, 31565, 25258, 14400,
     16512, 16480, 14432, 44405, 2048,  6241,  6209,  4096,  27469, 21129,
     2048,  12645, 44405, 23210, 2048,  8354,  40146, 2048,  6209,  6209,
     2048,  29614, 18691, 18560, 16513, 16512, 14368, 33742, 65469, 2081,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2081,  65534, 25193, 31662, 14368,
     16480, 16512, 14368, 42325, 4160,  6209,  6209,  4128,  16904, 31695,
     2048,  10532, 19049, 14791, 4096,  16806, 31695, 2048,  6209,  6209,
     2048,  33808, 16545, 18592, 18593, 16512, 16578, 29516, 65502, 2081,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2081,  65535, 23047, 35953, 14368,
     16480, 16512, 12288, 40147, 8419,  6208,  6241,  4160,  10467, 42259,
     2048,  4160,  4193,  4128,  4096,  25323, 21097, 4128,  6241,  6209,
     2048,  35953, 14400, 20641, 18593, 16480, 20869, 29419, 65534, 2081,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2081,  65535, 23079, 40179, 12288,
     16512, 16513, 12288, 35823, 14726, 4128,  6241,  6209,  4160,  44438,
     2048,  6209,  6209,  6209,  2048,  35921, 12580, 4128,  6209,  6209,
     0,     40114, 16416, 18593, 18593, 16480, 25128, 29418, 65535, 2081,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2081,  63356, 20901, 42325, 8192,
     12288, 12288, 8192,  25226, 23177, 0,     2048,  2048,  0,     44405,
     2048,  2048,  2048,  2048,  0,     40147, 4096,  2048,  2048,  2048,
     8192,  35758, 12288, 14336, 14336, 10240, 27306, 25095, 63355, 2081,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     32,    63355, 54936, 35986, 57049,
     63323, 61275, 63355, 46485, 46485, 63355, 63323, 63323, 61274, 48598,
     54936, 63355, 63323, 63355, 61210, 48631, 59097, 63355, 63323, 63355,
     54936, 48598, 63323, 61275, 61274, 63323, 46485, 56984, 63355, 32,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     32,    65501, 63323, 35953, 63387,
     65534, 65533, 65534, 50678, 46518, 65534, 65534, 65534, 65533, 48598,
     59162, 65534, 65534, 65534, 65533, 48599, 65468, 65534, 65534, 65534,
     59162, 50743, 65534, 65533, 65533, 65534, 48566, 63323, 65533, 0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2081,  44371, 35920, 40179, 38000,
     42258, 42226, 42258, 38033, 40146, 42258, 42226, 42226, 40145, 46486,
     38033, 42258, 42226, 42258, 40113, 46486, 38065, 42258, 42226, 42258,
     38065, 42259, 42226, 42226, 42226, 42226, 40146, 38000, 44371, 2081,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2145,  29484, 8288,  46551, 8353,
     12579, 12579, 12515, 23210, 31662, 12514, 12580, 14628, 10434, 42292,
     14693, 12547, 12579, 14628, 10402, 44372, 10466, 12579, 12580, 12547,
     16806, 31629, 10466, 12547, 12547, 10434, 31662, 8321,  27436, 2145,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4194,  42194, 8192,  46551, 10368,
     14595, 14595, 12482, 23177, 31629, 12482, 14627, 14627, 12449, 42292,
     16740, 14595, 14595, 14627, 12417, 44373, 12482, 14595, 14627, 14595,
     18853, 31629, 12482, 14595, 14595, 12449, 31662, 8256,  42194, 4194,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25257, 31630, 14562, 16675, 16675, 12481, 44340,
     16773, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16675,
     18854, 31662, 14562, 16643, 16675, 14529, 33710, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25257, 31662, 14562, 16675, 16675, 12481, 44372,
     16740, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16675,
     18854, 31662, 14562, 16675, 16643, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25258, 31629, 14562, 16675, 16675, 12481, 44340,
     16740, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16675,
     18854, 31662, 14562, 16643, 16643, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16643, 14562, 25258, 31629, 14562, 16675, 16675, 12481, 44340,
     16740, 16643, 16675, 16675, 12449, 44373, 12514, 16675, 16675, 16643,
     18854, 31661, 14562, 16675, 14595, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25258, 31662, 14562, 16675, 16675, 12481, 44340,
     16740, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16675,
     18854, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25258, 31630, 14562, 16675, 16675, 12481, 44340,
     16740, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16643,
     18854, 31661, 14530, 16675, 16643, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25257, 31662, 14562, 16675, 16675, 12481, 44372,
     16772, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16675,
     18854, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48632, 12417,
     16643, 16675, 14562, 25258, 33742, 14562, 16675, 16675, 12481, 44372,
     16740, 16643, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16643,
     18854, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46583, 12417,
     16643, 16675, 14562, 25258, 31662, 14562, 16675, 16675, 12481, 44372,
     16740, 16643, 16675, 16675, 12449, 44405, 14562, 16675, 16675, 16675,
     18854, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46583, 12417,
     16675, 16675, 14562, 25258, 31662, 14562, 16675, 16675, 12481, 44372,
     16740, 14595, 16675, 16675, 12449, 44373, 14562, 16675, 16675, 16643,
     18854, 31662, 14530, 14627, 16643, 14529, 31662, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16675, 16675, 14562, 25257, 33710, 14562, 16675, 16675, 12481, 44372,
     16740, 14595, 16675, 16675, 12449, 44405, 14562, 16675, 16675, 16643,
     18886, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48533, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46583, 12416,
     16643, 16675, 14562, 25258, 31630, 14562, 16675, 16675, 12481, 44340,
     16740, 16643, 16675, 16675, 12449, 44405, 14562, 16675, 16675, 16643,
     18886, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46551, 12417,
     16643, 16675, 14562, 25258, 31630, 14562, 16675, 16675, 12481, 44373,
     16740, 16643, 16675, 16675, 12449, 44405, 14562, 16675, 16675, 16643,
     18886, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 10369,
     16643, 14627, 14562, 25258, 33710, 14562, 16675, 16675, 12481, 44372,
     16740, 14595, 16675, 16675, 12449, 44405, 14530, 16675, 16675, 16643,
     18886, 31662, 14530, 16675, 16643, 14529, 31662, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  46584, 10369,
     16643, 16675, 14562, 25258, 31662, 14562, 16675, 16675, 12449, 44372,
     16740, 16643, 16675, 16675, 12449, 44405, 14562, 16675, 16675, 16643,
     18854, 31662, 14530, 16675, 16643, 14529, 31694, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 10369,
     16643, 16675, 14562, 25258, 31662, 14562, 16675, 16675, 12449, 44373,
     16740, 16643, 16675, 16675, 12449, 44405, 14530, 16675, 16675, 16643,
     18886, 33710, 14530, 16643, 16643, 14529, 33710, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 12417,
     16643, 16675, 14562, 25258, 33742, 14562, 16675, 16675, 12449, 44373,
     16740, 16643, 16675, 16675, 12449, 46453, 12482, 16675, 16675, 16643,
     18854, 31662, 14530, 16643, 14595, 14529, 33710, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4194,  48533, 8192,  48632, 12416,
     16643, 16643, 14562, 25257, 31662, 14562, 16675, 16675, 12449, 44372,
     16740, 14595, 16675, 16675, 12449, 46453, 12482, 16675, 16675, 14595,
     18853, 31662, 14530, 14595, 14595, 12481, 31694, 8224,  48501, 4194,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2145,  48533, 8192,  46583, 10336,
     14562, 14594, 14529, 25257, 31662, 14529, 16643, 16643, 12449, 44372,
     16740, 14562, 16643, 16643, 12417, 44405, 12482, 14594, 16643, 14562,
     18853, 31661, 12449, 14562, 14562, 12449, 31662, 8224,  48533, 2146,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     31727, 38033, 31661, 44373, 31694,
     35920, 35920, 35920, 35920, 38098, 35919, 35920, 35920, 33806, 46518,
     33807, 35920, 35920, 35920, 33774, 46550, 33774, 35920, 35920, 35920,
     33839, 40179, 35887, 35920, 35920, 35887, 40179, 31694, 38000, 31695,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     25356, 42194, 4096,  48664, 8192,
     10369, 10369, 10336, 23145, 29549, 10336, 12449, 12449, 8256,  42292,
     14595, 10369, 12449, 12449, 8224,  44373, 10336, 12449, 12449, 10369,
     16740, 31597, 8256,  10369, 10369, 8256,  31630, 4096,  42162, 23308,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     2113,  48533, 8192,  46584, 10368,
     16643, 16675, 14562, 25258, 31662, 14562, 16675, 16675, 12449, 44373,
     16741, 16643, 16675, 16675, 12449, 46453, 14562, 16675, 16675, 16643,
     18885, 31662, 14530, 16643, 14595, 14529, 33742, 8224,  48533, 2113,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 10368,
     16643, 16675, 14562, 25258, 33742, 14562, 16675, 16675, 12449, 44373,
     16740, 16643, 16675, 16675, 12449, 46453, 12514, 16675, 16675, 16643,
     18854, 31662, 14530, 14595, 14595, 12481, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 10368,
     16643, 16643, 14562, 25258, 31662, 14562, 16675, 16675, 12449, 44373,
     16740, 16643, 16675, 16675, 12449, 46453, 12482, 16675, 16675, 16643,
     18854, 31662, 14530, 16643, 14595, 12481, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 10368,
     14595, 14595, 14562, 25258, 33710, 14530, 16675, 16675, 12449, 44373,
     16772, 16643, 16675, 16675, 12449, 46453, 12482, 16675, 16675, 16643,
     18854, 31662, 14530, 14595, 14594, 12481, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48533, 8192,  48664, 10368,
     14595, 16675, 14562, 25258, 33742, 14530, 16675, 16675, 12449, 44373,
     16740, 16643, 16675, 16675, 12449, 46485, 12482, 16675, 16675, 16643,
     18854, 31662, 14530, 14595, 14594, 12449, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48501, 8192,  48664, 10368,
     16643, 16643, 14562, 25258, 31662, 14562, 16643, 16675, 12449, 44373,
     16740, 14595, 16675, 16675, 12449, 46454, 12482, 16675, 16675, 16643,
     18853, 33710, 12482, 16643, 14594, 12449, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48501, 8192,  46583, 10368,
     16643, 14595, 14562, 25258, 31662, 14562, 16675, 16675, 12449, 44373,
     16740, 14595, 16675, 16675, 12449, 46453, 12482, 16675, 16675, 16643,
     18886, 33710, 12482, 14595, 14594, 12449, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48501, 8192,  48664, 10368,
     14595, 14595, 14562, 25258, 33742, 14562, 16675, 16675, 12449, 44373,
     16740, 14595, 16675, 16675, 12449, 46486, 12482, 16675, 16675, 14595,
     18886, 33710, 14530, 14595, 14594, 12449, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48501, 8192,  48664, 10368,
     14595, 16643, 14562, 25258, 31662, 14562, 16675, 16675, 12449, 44373,
     16740, 16643, 16675, 16675, 12449, 46486, 12482, 16675, 16675, 14595,
     18885, 33710, 12482, 14595, 14594, 12449, 33742, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48501, 8192,  48664, 10368,
     14595, 16643, 14562, 25258, 33710, 14562, 16675, 16675, 12449, 44405,
     16740, 14595, 16675, 16675, 12449, 46453, 12482, 16675, 16675, 14595,
     18885, 33710, 12481, 14595, 14595, 12449, 33743, 8224,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4226,  48501, 6144,  48697, 10336,
     14594, 14595, 14562, 25258, 33742, 14530, 16675, 16675, 12449, 44373,
     16740, 14595, 16675, 16643, 10369, 46453, 12482, 14595, 16643, 14594,
     18853, 33710, 12481, 14594, 14562, 12449, 33742, 8192,  48501, 4226,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},
    {0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4194,  50614, 10304, 48696, 14562,
     18788, 16740, 16707, 27371, 33775, 16675, 18820, 18820, 14594, 46453,
     18886, 16740, 18788, 18820, 14562, 46518, 16675, 18788, 18820, 16740,
     20999, 33775, 16675, 16740, 16708, 16643, 35855, 10368, 48566, 4194,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0},

};

#ifndef PUSHBUTTONS_H
#define PUSHBUTTONS_H

#define KEYS_BASE 0xFF200050

struct pushbuttonStruct {
  volatile unsigned long int data;
  volatile unsigned long int unused;
  volatile unsigned long int interruptMask;
  volatile unsigned long int edgeCapture;
};

#endif
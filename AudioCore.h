#ifndef AUDIOCORE_H
#define AUDIOCORE_H

#define AUDIO_BASE 0xFF203040

struct audioCoreStruct {
  // Control Register
  volatile unsigned long int control;
  /*
    CONTROL REGISTER COMPONENTS
    Bit	Name	Description
    0	RE	Read Interrupt enable
    1	WE	Write Interrupt enable
    2	CR	Clear both left and right read FIFOs
    3	CW	Clear both left and right write FIFOs
    8	RI	Indicates that a read interrupt is pending
    9	WI	Indicates that a write interrupt is pending

  */

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

#endif  // AUDIOCORE_H
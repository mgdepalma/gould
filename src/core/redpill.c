/*
Red Pill... or how to detect VMM using (almost) one CPU instruction
Joanna Rutkowska
http://invisiblethings.org/
November 2004

The heart of this code is actually the SIDT instruction
(encoded as 0F010D[addr]), which stores the contents of the interrupt
descriptor table register (IDTR) in the destination operand, which is
actually a memory location. What is special and interesting about SIDT
instruction is that, it can be executed in non privileged mode (ring3)
but it returns the contents of the sensitive register, used internally
by operating system.
*/

#include <stdio.h>

int swallow_redpill ()
{
  unsigned char m[2+4], rpill[] = "\x0f\x01\x0d\x00\x00\x00\x00\xc3";
  *((unsigned*)&rpill[3]) = (unsigned)m;
  ((void(*)())&rpill)();
  return (m[5]>0xd0) ? 1 : 0;
}

int main(int argc, char *argv[])
{
  return swallow_redpill ();
}

# Mips32 Assembler and Emulator
This is a very limited mips assembler and emulator  
It supports comments, labels, hex and decimal immediates, and a tiny subset of mips instructions  
It does not currently support embedding static data with the program, only immediates  
The emulator doesn't currently emulate branch delay slots  
Definitely still WIP, but making progress  

## Instruction Syntax
It doesn't support commas or the $ register prefix  
A common instruction, addiu, looks like this:  
```addiu a0 zero 0```  
or this:  
```addiu v0 zero 0x4000```  

## Assembler Invocation
```./asm test.asm test.bin```  
-- input: test.asm  
-- output: test.bin  

## Emulator Invocation
```./emu test.bin```  
-- input: test.bin  

## Testing the Emulator and Assembler
If all is working well, the emulator should leave an exit code of 10  
```
./asm test.asm test.bin
./emu test.bin
echo $?
```

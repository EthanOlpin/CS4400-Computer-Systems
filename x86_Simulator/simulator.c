/*
 * Author: Daniel Kopta
 * Updated by: Erin Parker
 * CS 4400, University of Utah
 *
 * Simulator handout
 * A simple x86-like processor simulator.
 * Read in a binary file that encodes instructions to execute.
 * Simulate a processor by executing instructions one at a time and appropriately 
 * updating register and memory contents.
 *
 * Some code and pseudo code has been provided as a starting point.
 *
 * Completed by: Ethan Olpin (u1018382)
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "instruction.h"

// Forward declarations for helper functions
unsigned int get_file_size(int file_descriptor);
unsigned int* load_file(int file_descriptor, unsigned int size);
instruction_t* decode_instructions(unsigned int* bytes, unsigned int num_instructions);
unsigned int execute_instruction(unsigned int program_counter, instruction_t* instructions, 
				 int* registers, unsigned char* memory);
void print_instructions(instruction_t* instructions, unsigned int num_instructions);
void error_exit(const char* message);

//Additional declarations implemented by Ethan
void write_to_memory(unsigned char* mem, int addr, int val);
int read_from_memory(unsigned char* mem, int addr);

// 17 registers
#define NUM_REGS 17
// 1024-byte stack
#define STACK_SIZE 1024

int main(int argc, char** argv)
{
  // Make sure we have enough arguments
  if(argc < 2)
    error_exit("must provide an argument specifying a binary file to execute");

  // Open the binary file
  int file_descriptor = open(argv[1], O_RDONLY);
  if (file_descriptor == -1) 
    error_exit("unable to open input file");

  // Get the size of the file
  unsigned int file_size = get_file_size(file_descriptor);
  // Make sure the file size is a multiple of 4 bytes
  // since machine code instructions are 4 bytes each
  if(file_size % 4 != 0)
    error_exit("invalid input file");

  // Load the file into memory
  // We use an unsigned int array to represent the raw bytes
  // We could use any 4-byte integer type
  unsigned int* instruction_bytes = load_file(file_descriptor, file_size);
  close(file_descriptor);

  unsigned int num_instructions = file_size / 4;


  /****************************************/
  /**** Begin code to modify/implement ****/
  /****************************************/

  // Allocate and decode instructions (left for you to fill in)
  instruction_t* instructions = decode_instructions(instruction_bytes, num_instructions);
  
  // Optionally print the decoded instructions for debugging
  // Will not work until you implement decode_instructions
  // Do not call this function in your submitted final version
  // print_instructions(instructions, num_instructions);


  // Once you have completed Part 1 (decoding instructions), uncomment the below block

  // Allocate and initialize registers
  int* registers = (int*)malloc(sizeof(int) * NUM_REGS);
  int i;
  for(i = 0; i < 6; i++)
    registers[i] = 0;
  //Unlike other regs %esp is initialized to the size of the stack.
  registers[6] = STACK_SIZE;

  for(i = 7; i < 17; i++)
    registers[i] = 0;

  // Stack memory is byte-addressed, so it must be a 1-byte type
  unsigned char* memory = (char*)malloc(sizeof(char) * STACK_SIZE);

  // Run the simulation
  unsigned int program_counter = 0;

  // program_counter is a byte address, so we must multiply num_instructions by 4 
  // to get the address past the last instruction
  while(program_counter != num_instructions * 4)
  {
    program_counter = execute_instruction(program_counter, instructions, registers, memory);
  }
  
  return 0;
}

/*
 * Decodes the array of raw instruction bytes into an array of instruction_t
 * Each raw instruction is encoded as a 4-byte unsigned int
*/
instruction_t* decode_instructions(unsigned int* bytes, unsigned int num_instructions)
{
  instruction_t* retval = calloc(num_instructions, sizeof(instruction_t));

  /*Masks used to extract instruction info*/
  const int reg1_mask = 0x07C00000;
  const int reg2_mask = 0x003E0000;
  const int immediate_mask = 0x0000FFFF;
  
  int i;
  for(i = 0; i < num_instructions; i++){
    retval[i].opcode = bytes[i] >> 27;
    retval[i].first_register = (bytes[i] & reg1_mask) >> 22;
    retval[i].second_register = (bytes[i] & reg2_mask) >> 17;
    retval[i].immediate = (bytes[i] & immediate_mask);
  }     
  return retval;
}


/*
 * Executes a single instruction and returns the next program counter
*/
unsigned int execute_instruction(unsigned int program_counter, instruction_t* instructions, int* registers, unsigned char* memory)
{
  // program_counter is a byte address, but instructions are 4 bytes each
  // divide by 4 to get the index into the instructions array
  instruction_t instr = instructions[program_counter / 4];

  //Masks used for setting and detecting condition codes.
  int cf_mask = 0x1;
  int zf_mask = 0x40;
  int sf_mask = 0x80;
  int of_mask = 0x800;
  int sf_or_of = sf_mask | of_mask;

  //Commonly used registers
  int* reg1 = &registers[instr.first_register];
  int* reg2 = &registers[instr.second_register];
  int* eflags = &registers[16];
  int* esp = &registers[6];

  int i;
  int temp;

  //printf(">>> %s", instr.opcode);
  switch(instr.opcode)
  {
  case subl:
    *reg1 = *reg1 - instr.immediate;
    break;
  case addl_reg_reg:
    *reg2 = *reg2 + *reg1;
    break;
  case addl_imm_reg: 
    *reg1 = *reg1 + instr.immediate;
    break;
  case imull: 
    *reg2 = *reg1 * *reg2;
    break;
  case shrl:
    //Casts to unsigned before shifting to prevent
    //padding with the signed bit.
    *reg1 = (int)(((unsigned)*reg1) >> 1);
    break;
  case movl_reg_reg: 
    *reg2 = *reg1;
    break;
  case movl_deref_reg:
    *reg2 = read_from_memory(memory, *reg1 + instr.immediate);
    break;
  case movl_reg_deref:
    write_to_memory(memory, *reg2 + instr.immediate, *reg1);
    break;
  case movl_imm_reg: 
    *reg1 = instr.immediate;
    break;
  case cmpl:
    *eflags = *eflags & 0;
    //CF
    if((unsigned int)*reg2 < (unsigned int)*reg1){
      *eflags = *eflags | cf_mask;
    }
    //ZF
    if(*reg1 == *reg2){
      *eflags = *eflags | zf_mask;
    }
    //SF
    if((*reg2-*reg1) < 0){
      *eflags = *eflags | sf_mask;
    }
    //OF
    temp = *reg2 - *reg1;
    if((*reg2 > 0 && *reg1 < 0 && temp < 0) || (*reg2 < 0 && *reg1 > 0 && temp > 0)){
      *eflags = (*eflags | of_mask);
    }
    break;
  case je: 
    if(*eflags & zf_mask){
      return program_counter + instr.immediate + 4;
    }
    break;
  case jl:
    //SF flag XOR OF flag
    if(((sf_or_of & *eflags) == sf_mask) || ((sf_or_of & *eflags) == of_mask)){
      return program_counter + instr.immediate + 4;
    }
    break;
  case jle:
    //(SF flag XOR OF flag) OR (ZF flag)
    if((((sf_or_of & *eflags) == sf_mask) || ((sf_or_of & *eflags) == of_mask)) || ((*eflags & zf_mask) == zf_mask)){
      return program_counter + instr.immediate + 4;
    }
    break;
  case jge: 
    //!(SF flag XOR OF flag)
    if(!(((sf_or_of & *eflags) == sf_mask) || ((sf_or_of & *eflags) == of_mask))){
      return program_counter + instr.immediate + 4;
    }
    break;
  case jbe:
    if((*eflags & cf_mask) || (*eflags & zf_mask)){
      return program_counter + instr.immediate + 4;
    }
    break;
  case call:
    *esp = *esp - 4;
    temp = program_counter + 4;
    write_to_memory(memory, *esp, temp);
  case jmp:
    return program_counter + instr.immediate + 4;
    break;
  case ret: 
    if(*esp == 1024){
      exit(0);
    }
    else{
      program_counter = read_from_memory(memory, *esp);
      *esp = *esp + 4;
      return program_counter;
    }
    break;
  case pushl: 
    *esp = *esp - 4;
    write_to_memory(memory, *esp, *reg1);
    break;
  case popl:
    *reg1 = read_from_memory(memory, *esp);
    *esp = *esp + 4;
    break;
  case printr:
    printf("%d (0x%x)\n", *reg1, *reg1);
    break;
  case readr:
    scanf("%d", &(*reg1));
    break;
  }

  // program_counter + 4 represents the subsequent instruction
  return program_counter + 4;
}

//Writes the four bytes of val to four consecutive bytes of mem,
//starting at mem[addr].
void write_to_memory(unsigned char* mem, int addr, int val){
  int i;
  for(i = 0; i < 4; i++){
    mem[addr + i] = val >> (8*i);
  }
}

//Reads four consecutive bytes from mem, starting at mem[addr],
//returns these bytes as an int.
int read_from_memory(unsigned char* mem, int addr){
  return ((mem[addr + 3] << 24)
           | (mem[addr + 2] << 16)
	   | (mem[addr + 1] << 8)
	   | (mem[addr]));
}

/*********************************************/
/****  DO NOT MODIFY THE FUNCTIONS BELOW  ****/
/*********************************************/

/*
 * Returns the file size in bytes of the file referred to by the given descriptor
*/
unsigned int get_file_size(int file_descriptor)
{
  struct stat file_stat;
  fstat(file_descriptor, &file_stat);
  return file_stat.st_size;
}

/*
 * Loads the raw bytes of a file into an array of 4-byte units
*/
unsigned int* load_file(int file_descriptor, unsigned int size)
{
  unsigned int* raw_instruction_bytes = (unsigned int*)malloc(size);
  if(raw_instruction_bytes == NULL)
    error_exit("unable to allocate memory for instruction bytes (something went really wrong)");

  int num_read = read(file_descriptor, raw_instruction_bytes, size);

  if(num_read != size)
    error_exit("unable to read file (something went really wrong)");

  return raw_instruction_bytes;
}

/*
 * Prints the opcode, register IDs, and immediate of every instruction, 
 * assuming they have been decoded into the instructions array
*/
void print_instructions(instruction_t* instructions, unsigned int num_instructions)
{
  printf("instructions: \n");
  unsigned int i;
  for(i = 0; i < num_instructions; i++)
  {
    printf("op: %d, reg1: %d, reg2: %d, imm: %d\n", 
	   instructions[i].opcode,
	   instructions[i].first_register,
	   instructions[i].second_register,
	   instructions[i].immediate);
  }
  printf("--------------\n");
}

/*
 * Prints an error and then exits the program with status 1
*/
void error_exit(const char* message)
{
  printf("Error: %s\n", message);
  exit(1);
}

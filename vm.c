/*
Assignment: HW4 - Complete Parser and Code Generator for PL/0 (with Procedures, Call, and Else)

Author(s): Yevgeniy Ignatov

Language: C (only)

To Compile:

    Scanner:
        gcc -O2 -std=c11 -o lex lex.c
    Parser/Code Generator:
        gcc -O2 -std=c11 -o parsercodegen_complete parsercodegen_complete.c
    Virtual Machine:
        gcc -O2 -std=c11 -o vm vm.c

To Execute (on Eustis):
        ./lex <input_file.txt>
        ./parsercodegen_complete
        ./vm elf.txt
where:
    <input_file.txt> is the path to the PL/0 source program

Notes:
    - lex.c accepts ONE command-line argument (input PL/0 source file)
    - parsercodegen_complete.c accepts NO command-line arguments
    - Input filename is hard-coded in parsercodegen.c
    - Implements recursive-descent parser for PL/0 grammar
    - Supports procedures, call statements, and if-then-else
    - Generates PM/0 assembly code (see Appendix A for ISA)
    - VM must support EVEN instruction (OPR 0 11)
    - All development and testing performed on Eustis

Class: COP3402 - System Software - Fall 2025
Instructor: Dr. Jie Lin
Due Date: Friday, November 21, 2025 at 11:59 PM ET
*/


#include <stdio.h>
#include <string.h>


#define PAS_SIZE 500

// Define opcodes
typedef enum {
    LIT = 1,
    OPR = 2,
    LOD = 3,
    STO = 4,
    CAL = 5,
    INC = 6,
    JMP = 7,
    JPC = 8,
    SYS = 9
} opcode;

int pas[PAS_SIZE] = {0}; // process address space all initialized to 0

// Function to find base L levels down
int base(int BP, int L){
    int arb = BP;   // arb = activation record base
    while (L-- > 0) arb = pas[arb]; // follow static link
    return arb;
}
 // Print current state of the VM
void print_state(int OP, int L, int M, int PC, int BP, int SP, int textEnd) {
    char left[5]; // Buffer to hold instruction name
    
    // Decode OP and M
    if(OP == OPR){
        switch(M){
            case 0: 
                strcpy(left, "RTN"); 
                break;
            case 1: {
                strcpy(left, "ADD"); 
                break;
            }
            case 2: {
                strcpy(left, "SUB"); 
                break;
            }
            case 3: {
                strcpy(left, "MUL"); 
                break;
            }
            case 4: {
                strcpy(left, "DIV"); 
                break;
            }
            case 5: {
                strcpy(left, "EQL"); 
                break;
            }
            case 6: {
                strcpy(left, "NEQ"); 
                break;
            }
            case 7: {
                strcpy(left, "LSS"); 
                break;
            }
            case 8: {
                strcpy(left, "LEQ"); 
                break;
            }

            case 9: {
                strcpy(left, "GTR"); 
                break;
            }
            case 10: {
                strcpy(left, "GEQ"); 
                break;
            }
            case 11: {
                strcpy(left, "EVEN"); 
                break;
            }
            default: {
                strcpy(left, "OPR"); 
                break;
            }
        }
    }
    else{
        switch(OP){
            case LIT: {
                strcpy(left, "LIT"); 
                break;
            }
            case LOD: {
                strcpy(left, "LOD"); 
                break;
            }
            case STO: {
                strcpy(left, "STO"); 
                break;
            }
            case CAL: {
                strcpy(left, "CAL"); 
                break;
            }
            case INC: {
                strcpy(left, "INC"); 
                break;
            }
            case JMP: {
                strcpy(left, "JMP"); 
                break;
            }
            case JPC: {
                strcpy(left, "JPC"); 
                break;
            }
            case SYS: {
                strcpy(left, "SYS"); 
                break;
            }
            default: {
                strcpy(left, "UNK"); 
                break;
            }
        }
    }

    int bars[20];           // to hold indices of activation record bars
    int nbars = 0;          // number of bars found

    // Find activation record bars
    int p = BP;                                     // Start from current frame base
    while (p >= 0 && p < PAS_SIZE && nbars < 20) { 
        bars[nbars++] = p;                          // Store bar position
        int next = pas[p - 1];
        if (next <= 0 || next >= PAS_SIZE) {        // Invalid next base
            break;
        }                                  
        if (next == p) break;
        p = next;                                   // Move to next frame base
    }

    // Print current instruction and registers
    printf(" %4s      %3d   %3d   %3d   %3d   %3d\t", left, L, M, PC, BP, SP);

    // Print stack contents with bars
    int bottom = textEnd - 1;                       // last data cell under text
    for (int i = bottom; i >= SP; i--) {
        

        for (int k = 0; k < nbars; k++) {           // Check if a bar should be printed here and print it
            if (i == bars[k] && bars[k] < bottom) { //
                printf(" | "); 
                break; 
            }
        }   
        printf(" %2d ", pas[i]);                     // Print stack value

    }
    printf("\n");
}

int main (int argc, char **argv){
    // Exit if improper argument
    if (argc != 2) {
        printf("Error improper argument\n");
        return 1;
    }

    // Open file
    FILE *fp = fopen(argv[1], "r");
    
    // Exit program if file not found
    if (fp == NULL) {
        printf("Error file not found\n");
        return 1;
    }

    int addr = PAS_SIZE - 1;        // Starting address at top of memory
    int line_number = 0;            // Count number of lines in file
    
    // Read instructions from file into pas
    while(1){
        int OP, L, M;
        int r = fscanf(fp, "%d %d %d", &OP, &L, &M); // Read 3 integers from file into OP, L, M
        if (r == EOF)                                // r holds number of items successfully scanned (unless)
            break;
        
        // Exit if improper format or file too big
        if (r != 3) {                
            printf("Error improper format\n");
            return 1;
        }
        if (addr - 2 < 0) {
            printf("Error file too big\n");
            return 1;
        }

        // Store instructions 
        pas[addr--] = OP;
        pas[addr--] = L;
        pas[addr--] = M;
        
        line_number++;
    }

    // Close file
    fclose(fp);
    
    // Exit if empty file
    if(line_number == 0){
        printf("Empty file\n");
        return 0;
    }
    

    int textEnd = addr + 1;             // Address of last memory cell used by text segment (for printing)
    int PC = PAS_SIZE - 1;              // Program counter starts at the last address
    int SP = textEnd;                   // Stack pointer starts below text segment
    int BP = SP - 1;                    // Base pointer


    // Print table header and initial values
    printf("            L     M    PC    BP    SP    STACK\n");
    printf("Initial values:       %d   %d   %d\n", PC, BP, SP);


    // Main Loop
    while(1){
        // Test for PC out of bounds
        if (PC < textEnd + 2 || PC > PAS_SIZE - 1){
            printf("Error: PC out of bounds\n");
            return 1;
        }
        int OP = pas[PC];           // Current opcode
        int L = pas[PC - 1];        // Current L
        int M = pas[PC - 2];        // Current M
        int nextPC = PC - 3;        // Next program counter
        
        // Execute instruction based on OP
        switch(OP){
            case LIT: {          // Pushes a constant value M onto the stack
                if (SP - 1 < 0){
                    printf("Error: Stack overflow\n");
                    return 1;
                }
                SP--;               // Decrement stack pointer
                pas[SP] = M;        // Push M onto stack
                break;
            }
            case OPR: {                      // Perform operation specified by M
                switch(M){
                    case 0: {                   // Return from subroutine
                        int RA = pas[BP - 2];   // Return address
                        int DL = pas[BP - 1];   // Dynamic link
                        SP = BP + 1;            // Restore stack pointer
                        BP = DL;                // Restore base pointer
                        nextPC = RA;            // Jump to return address
                        break;
                    }
                    // Addition
                    case 1: {
                        int a = pas[SP + 1];    
                        int b = pas[SP];
                        pas[SP + 1] = a + b;
                        SP++;
                        break;
                    }
                    // Subtraction
                    case 2: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = a - b;
                        SP++;  
                        break;
                    }
                    // Multiplication
                    case 3: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = a * b;
                        SP++;  
                        break;
                    }
                    // Division
                    case 4: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        // Check for division by zero
                        if (b == 0){
                            printf("Error: Division by zero\n");
                            return 1;
                        }
                        pas[SP + 1] = a / b;
                        SP++;  
                        break;
                    }
                    // = case
                    case 5: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = (a == b);
                        SP++;  
                        break;
                    }
                    // != case
                    case 6: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = (a != b);
                        SP++;  
                        break;
                    }
                    // < case
                    case 7: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = (a < b);
                        SP++;  
                        break;
                    }
                    /// <= case
                    case 8: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = (a <= b);
                        SP++;  
                        break;
                    }
                    // > case
                    case 9: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = (a > b);
                        SP++;  
                        break;
                    }
                    // >= case
                    case 10: {
                        int a = pas[SP + 1];
                        int b = pas[SP];
                        pas[SP + 1] = (a >= b);
                        SP++;  
                        break;
                    }
                    // Even case
                    case 11: {
                        int a = pas[SP];
                        pas[SP] = (a % 2 == 0); // 1 if even, 0 if odd
                        break;
                    }
                    // Invalid OPR
                    default:{
                        printf("Error unknown OPR");
                        return 1;
                    }

                }
                break;

            }
            // Load value at offset M in L levels down onto stack
            case LOD: {
                
                // Find base L levels down and calculate target index
                int b = base(BP, L);
                int index = b - M;
                
                // Check for stack overflow and invalid memory access
                if (SP - 1 < 0 || index < 0 || index >= PAS_SIZE){
                    printf("Error Stack overflow\n");
                    return 1;
                }
                
                SP--;
                pas[SP] = pas[index];
                
                break;
            }
            // Store top stack value at offset M in L levels down
            case STO: {
                
                int value = pas[SP];
                SP++;

                // Find base L levels down and calculate target index
                int b = base(BP, L);
                int index = b - M;
                
                // Check for invalid memory access
                if (index < 0 || index >= PAS_SIZE){
                    printf("Error Invalid memory access STO\n");
                    return 1;
                }
                
                // Store value
                pas[index] = value;
                break;
            }
            // Call procedure at M in L levels down
            case CAL: {

                // Check for stack overflow
                if (SP - 3 < 0){
                    printf("Error Stack overflow\n");
                    return 1;
                }

                pas[SP - 1] = base(BP, L);    // Static link
                pas[SP - 2] = BP;             // Dynamic link
                pas[SP - 3] = PC - 3;         // Return address
                BP = SP - 1;                  // New base pointer
                int target = (PAS_SIZE - 1) - M;
                
                // Check for valid target
                if (target < textEnd + 2 || target > PAS_SIZE - 1){
                    printf("Error target out of bounds\n");
                    return 1;
                }
               
                nextPC = target;
                break;
            }
            // Allocate M locals
            case INC: {   
                
                // Check for stack overflow
                if (SP - M < 0){
                    printf("Error: Stack overflow\n");
                    return 1;
                }
                

                SP -= M;
                int locals = M - 3;
                if (locals < 0) locals = 0;
                
                // Initialize local variables to 0
                for (int i = 0; i < locals; i++){
                    pas[SP + i] = 0;
                }
                break;
            }
            // Jump
            case JMP: {
                int target = (PAS_SIZE - 1) - M;                    // Calculate target address
                if (target < textEnd+2 || target > PAS_SIZE - 1){   // Check for valid jump target
                    printf("Error: Jump target out of bounds\n");
                    return 1;
                }
                nextPC = target; 
                break;
            }
            // Jump conditional
            case JPC: {
                int c = pas[SP];
                SP++;
                if (c == 0){
                    int target = (PAS_SIZE - 1) - M;                    // Calculate target address
                    if (target < textEnd + 2 || target > PAS_SIZE - 1){ // Check for valid jump target
                        printf("Error: Jump target out of bounds\n");
                        return 1;
                    }
                    nextPC = target; 
                }
                break;
            }
            // System call
            case SYS: {
                if (M == 1){                                // Print top stack value
                    int output = pas[SP];
                    SP++;
                    printf("Output result is: %d\n", output);
                }
                
                else if (M == 2){                           // Read input to top of stack
                    int input;
                    printf("Please enter an integer: ");
                    if (scanf("%d", &input) != 1){          // Check for only 1 integer input
                        printf("Error: Invalid input\n");
                        return 1;
                    }
                    SP--;
                    pas[SP] = input;
                }
                else if (M == 3){                           // Stop program and exit
                    print_state(OP, L, M, nextPC, BP, SP, textEnd);
                    return 0;
                }
                else{
                    printf("Error: Unknown SYS M=%d\n", M); // Invalid M for SYS
                    return 1;
                }
                break;
            }
            default: {
                printf("Error: Unknown OP=%d\n", OP);       // Invalid OP
                return 1; 
            }  
        }
        PC = nextPC;
        print_state(OP, L, M, PC, BP, SP, textEnd);         // Print current state

        if (SP < 0 || SP >= PAS_SIZE) {                     // Check for stack pointer out of bounds
            printf("Error: Stack pointer out of bounds\n");
            return 1;
        }
    }

    return 0;

}
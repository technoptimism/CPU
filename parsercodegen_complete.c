/*
Author(s): Yevgeniy Ignatov

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
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Constants
#define MAX_TABLE_SIZE 500
#define MAX_IDENT_LEN 11
#define MAX_NUM_LEN 5

// Token types
typedef enum {
skipsym = 1 ,                            // Skip
identsym = 2,                            // Identifier
numbersym = 3,                           // Number
plussym = 4,                             // +
minussym = 5,                            // -
multsym = 6,                             // *
slashsym = 7,                            // /
eqsym = 8,                               // =
neqsym = 9,                              // <>
lessym = 10,                             // <
leqsym = 11,                             // <=
gtrsym = 12,                             // >
geqsym = 13,                             // >=
lparentsym = 14,                         // (
rparentsym = 15,                         // )
commasym = 16,                           // ,
semicolonsym = 17,                       // ;
periodsym = 18,                          // .
becomessym = 19,                         // :=
beginsym = 20,                           // begin
endsym = 21,                             // end
ifsym = 22,                              // if
fisym = 23,                              // fi
thensym = 24,                            // then
whilesym = 25,                           // while
dosym = 26,                              // do
callsym = 27,                            // call
constsym = 28,                           // const
varsym = 29,                             // var
procsym = 30,                            // procedure        
writesym = 31,                           // write
readsym = 32,                            // read
elsesym = 33,                            // else
evensym  = 34                            // even
}TokenType;

// PM/0 OP Codes
typedef enum {
    LIT = 1,                             // Load constant
    OPR = 2,                             // Operation
    LOD = 3,                             // Load variable
    STO = 4,                             // Store variable
    CAL = 5,                             // Call procedure
    INC = 6,                             // Allocate
    JMP = 7,                             // Jump
    JPC = 8,                             // Jump conditional
    SYS = 9                              // System call
}OpCode;

// OPR operations
#define OPR_RTN 0                        // Return
#define OPR_ADD 1                        // Addition
#define OPR_SUB 2                        // Subtraction
#define OPR_MUL 3                        // Multiplication
#define OPR_DIV 4                        // Division
#define OPR_EQL 5                        // Equal
#define OPR_NEQ 6                        // Not equal
#define OPR_LSS 7                        // Less than
#define OPR_LEQ 8                        // Less than or equal
#define OPR_GTR 9                        // Greater than
#define OPR_GEQ 10                       // Greater than or equal
#define OPR_EVEN 11                      // Even


// Symbol types
typedef enum {
    CONST = 1,                           // Constant
    VAR = 2,                             // Variable
    PROC = 3                             // Procedure
}SymbolType;

// Symbol table entry
typedef struct {
    SymbolType kind;                     // Type of symbol
    char name[MAX_IDENT_LEN + 1];        // Name
    int val;                             // For constants
    int level;                             // Lexical level
    int addr;                            // For variables and procedures
    int mark;                            // Marked or unmarked
}Symbol;


// Instruction struct
typedef struct {
    OpCode op;                           // Operation code
    int l;                               // Lexicographical level
    int m;                               // Modifier
}Instruction;


// Global variables
Symbol symbol_table[MAX_TABLE_SIZE];     // Symbol table
Instruction code[MAX_TABLE_SIZE];        // Generated code array
TokenType token;                         // Current token
FILE *token_file;                        // File for tokens
int current_level = 0;                   // Current lexical level
int number;                              // Current value 
int code_len = 0;                        // Length of generated code
int symbol_count = 0;                    // Number of symbols
char identifier[MAX_IDENT_LEN + 1];      // Current identifier


// Function prototypes
const char* opcode_to_string(OpCode op);
int symbol_table_insert(SymbolType kind, const char *name, int val, int lvl, int addr);
int symbol_table_lookup(const char *name);
int var_declaration();
void const_declaration();
void block();
void program();
void statement();
void condition();
void expression();
void term();
void factor();
void error(const char *msg);
void get_token();
void emit(OpCode op, int l, int m);
void print_assembly();
void print_symbol_table();
void mark_level(int lvl);

// main function
int main(){
    
    // Open tokens.txt for reading
    token_file = fopen("tokens.txt", "r");
    if (!token_file) {
        printf("Error opening tokens.txt\n");
        return 1;
    }

    // Get first token and start parsing program
    get_token();
    program();

    // Mark symbols at global level as used
    mark_level(0);

    // Print assembly code and symbol table
    print_assembly();
    print_symbol_table();  

    // close token file
    fclose(token_file);

    return 0;

}

// Error handling function
void error(const char *msg) {
    // Print error message to terminal
    printf("Error: %s\n", msg);

    // Write error to elf.txt and exit
    FILE *out = fopen("elf.txt", "w");
    if (out) {
        fprintf(out, "Error: %s\n", msg);
        fclose(out);
    }
    // Exit program
    exit(1);
}

// Read next token from tokens.txt function
void get_token() {

    // Check for missing period at end of program
    if (fscanf(token_file, "%d", (int *)&token) != 1) {
        token = 0;     
        return;
    }

    // Cast read integer to TokenType
    token = (TokenType)token;

    // Error if skipsym is detected
    if (token == skipsym) {
        error("Scanning error detected by lexer (skipsym present)");
    }

    // Error if identifier expected but not found
    if (token == identsym){
        if (fscanf(token_file, "%s", identifier) != 1) {
            error("const, var, read, procedure, and call keywords must be followed by identifier");
        }
    }
    //Error if number expected but not found
    else if (token == numbersym){
        if (fscanf(token_file, "%d", &number) != 1) {
            error("arithmetic equations must contain operands, parentheses, numbers, or symbols");
        }
    }
}

// Emit instruction to assembly code array
void emit(OpCode op, int l, int m) {

    code[code_len].op = op;              // Store operation
    code[code_len].l = l;                // Store lexicographical level
    code[code_len].m = m;                // Store modifier
    code_len++;                          // Increment code length

}

// Insert function
int symbol_table_insert(SymbolType kind, const char *name, int val, int lvl, int addr) {

    symbol_table[symbol_count].kind = kind;         // Set symbol kind
    strcpy(symbol_table[symbol_count].name, name);  // Set symbol name
    symbol_table[symbol_count].val = val;           // Set symbol value
    symbol_table[symbol_count].level = lvl;           // Set symbol level
    symbol_table[symbol_count].addr = addr;         // Set symbol address 
    symbol_table[symbol_count].mark = 0;            // Default to unmarked

    // Increment symbol count and return index
    return symbol_count++;
}

// Lookup function
int symbol_table_lookup(const char *name) {
    
    // Iterate backwards to find most recent declaration
    for (int i = symbol_count - 1; i >= 0; i--) {
        if (!symbol_table[i].mark && strcmp(symbol_table[i].name, name) == 0) {
            return i;
        }
    }
    
    // Return -1 if not found
    return -1;
}

// Const declaration function
void const_declaration() {
    
    // Temporary variables
    char name[MAX_IDENT_LEN + 1];

    do {
        get_token();

        // Error if identifier not found
        if (token != identsym) {
            error("const, var, read, procedure, and call keywords must be followed by identifier");
        } 

        // Copy identifier to name ensuring null-termination
        strcpy(name, identifier);           
        name[MAX_IDENT_LEN] = '\0';

        // Check for duplicate declarations
        for (int i = symbol_count - 1; i >= 0; i--) {
            if (!symbol_table[i].mark && symbol_table[i].level == current_level && strcmp(symbol_table[i].name, name) == 0) {
                error("symbol name has already been declared");
            }
        }

        get_token();

        // Expect '=' after identifier
        if (token != eqsym) {
            error("constants must be assigned with =");
        }

        get_token();

        // Expect number after '='
        if (token != numbersym) {
            error("constants must be assigned an integer value");
        }

        // Insert into symbol table
        symbol_table_insert(CONST, name, number, current_level, 0);

        get_token();
    }
    // Handle multiple const declarations
    while (token == commasym);

    // Expect semicolon at end of const declarations
    if (token != semicolonsym) {
        error("constant and variable declarations must be followed by a semicolon");
    }

    get_token();


}

// Var declaration function
int var_declaration() {
    int num_vars = 0;
    do{
        // Increment variable count for each variable declared
        num_vars++;

        get_token();
        
        // Error if identifier not found
        if (token != identsym) {
            error("const, var, read, procedure, and call keywords must be followed by identifier");
        } 

        // Check for duplicate declarations
        for (int i = symbol_count - 1; i >= 0; i--) {
            if (!symbol_table[i].mark && symbol_table[i].level == current_level && strcmp(symbol_table[i].name, identifier) == 0) {
                error("symbol name has already been declared");
            }
        }

        // Insert into symbol table
        symbol_table_insert(VAR, identifier, 0, current_level, num_vars + 2);

        get_token();
    }
    // Handle multiple var declarations
    while(token == commasym);

    // Expect semicolon at end of var declarations
    if (token != semicolonsym) {
        error("constant and variable declarations must be followed by a semicolon");
    }
    get_token();
    
    return num_vars;
    // Emit INC instruction to allocate space
    //emit(INC, 0, num_vars); 
}

// Block function
void block(){
    
    // Initialize number of variables
    int num_vars = 0;

    // Emit JMP instruction to jump over declarations
    int jmp_addr = code_len;
    emit(JMP, 0, 0); 

    // Handle declarations
    if (token == constsym) {
        const_declaration();
    }   
    if (token == varsym) {
        num_vars = var_declaration();
    }

    while (token == procsym) {
        get_token();

        // Expect identifier after procedure
        if (token != identsym) {
            error("const, var, read, procedure, and call keywords must be followed by identifier");
        }

        // Check for duplicate declarations
        for (int i = symbol_count - 1; i >= 0; i--) {
            if (!symbol_table[i].mark && symbol_table[i].level == current_level && strcmp(symbol_table[i].name, identifier) == 0) {
                error("symbol name has already been declared");
            }
        }

        // Insert procedure into symbol table
        int pidx = symbol_table_insert(PROC, identifier, 0, current_level, code_len);

        get_token();

        // Expect semicolon after procedure declaration
        if (token != semicolonsym) {
            error("procedure declaration must be followed by a semicolon");
        }

        get_token();


        // Procedure entry address is current code length in cells
        symbol_table[pidx].addr = code_len * 3;

        // Increment lexical level
        current_level++;

        // Parse procedure block
        block();

        // Decrement lexical level back to caller level
        current_level--;

        // Expect semicolon after procedure block
        if (token != semicolonsym) {
            error("procedure declaration must be followed by a semicolon");
        }

        get_token();

        // Emit OPR return instruction
        emit(OPR, 0, OPR_RTN);
    }

    // Emit INC instruction to allocate space for variables and
    code[jmp_addr].m = code_len * 3;
    emit(INC, 0, 3 + num_vars);
    
    statement();

    // Mark symbols in this block as used
    if (current_level > 0){
        mark_level(current_level);
    }


}

// Initial program parse function
void program(){
    
    // Start parsing
    block();

    // Check for period
    if (token != periodsym) {
        error("program must end with period");
    }

    // Emit HALT instruction
    emit(SYS, 0, 3);
}

// Statement function
void statement(){

    // Variables for statement processing
    int sym_index, jmp_idx , jpc_indx, loop_addr;
    
    // Assignment statement
    if (token == callsym){
        get_token();

        // Expect identifier after call
        if (token != identsym) {
            error("const, var, read, procedure, and call keywords must be followed by identifier");
        }

        // Lookup identifier in symbol table
        sym_index = symbol_table_lookup(identifier);

        // Error if not found
        if (sym_index == -1) {
            error("undeclared identifier");
        }

        // Identifier must be a procedure
        if (symbol_table[sym_index].kind != PROC) {
            error("call statement may only target procedures");
        }

        get_token();
        
        // Calculate L for CAL instruction
        int L = current_level - symbol_table[sym_index].level;
        
        // Emit CAL instruction
        emit(CAL, L, symbol_table[sym_index].addr);
    }
    else if (token == identsym) {

        // Lookup identifier in symbol table
        sym_index = symbol_table_lookup(identifier);

        // Error if not found
        if (sym_index == -1) {
            error("undeclared identifier");
        }

        // Identifier must be a variable
        if (symbol_table[sym_index].kind != VAR) {
            error("only variable values may be altered");
        }

        get_token();

        // Expect := for assignment
        if (token != becomessym) {
            error("assignment statements must use :=");
        }

        get_token();
        expression();
        
        // Calculate L for STO instruction and emit STO
        int L = current_level - symbol_table[sym_index].level;
        emit(STO, L, symbol_table[sym_index].addr);
    }
    // Begin...end block
    else if (token == beginsym) {
        
        // Loop through statements until end
        do {
            get_token();
            statement();
        } while (token == semicolonsym);

        // Expect 'end' keyword
        if (token != endsym) {
            error("begin must be followed by end");
        }

        get_token();
    }
    // If statement
    else if (token == ifsym) {
        
        get_token();
        condition();
        
        // Save index for JPC backpatching and emit temporary JPC
        jpc_indx = code_len;        
        emit(JPC, 0, 0);

        // Expect 'then' keyword
        if (token != thensym) {
            error("if must be followed by then");
        }

        get_token();
        statement();

        // Check for 'else' clause
        if (token != elsesym) {
            error("if statement must include else clause");
        }
        
        // Save index for JMP backpatching and emit temporary JMP
        jmp_idx = code_len;
        emit(JMP, 0, 0);

        // Backpatch JPC to point to else clause
        code[jpc_indx].m = 3 * code_len;

        get_token();
        statement();

        // Check for 'fi' to close if statement
        if (token != fisym) {
            error("else must be followed by fi");
        }
        get_token();
        code[jmp_idx].m = 3 * code_len;
    }
    // While statement
    else if (token == whilesym) {
        get_token();
        
        // Save loop start address
        loop_addr = code_len;
                
        condition();

        // Expect 'do' keyword
        if (token != dosym) {
            error("while must be followed by do");
        }

        get_token();

        // Save JPC index and emit temporary JPC
        int index = code_len;
        emit(JPC, 0, 0);

        statement();

        // Emit JMP back to loop start and backpatch JPC to exit loop
        emit(JMP, 0, 3 * loop_addr);
        code[index].m = 3 * code_len;
    }
    // Read statement
    else if (token == readsym) {

        get_token();

        // Expect identifier after read
        if (token != identsym) {
            error("const, var, read, procedure, and call keywords must be followed by identifier");
        }

        // Lookup identifier in symbol table
        sym_index = symbol_table_lookup(identifier);

        // Error if not found
        if (sym_index == -1) {
            error("undeclared identifier");
        }

        // Identifier must be a variable
        if (symbol_table[sym_index].kind != VAR) {
            error("only variable values may be altered");
        }

        get_token();
        
        int L = current_level - symbol_table[sym_index].level;
        // Emit SYS read instruction and STO to store value
        emit(SYS, 0, 2);
        emit(STO, L, symbol_table[sym_index].addr);
        

    }
    // Write statement
    else if (token == writesym) {

        get_token();
        expression();

        // Emit SYS write instruction
        emit(SYS, 0, 1);
    }
}

// Condition function
void condition() {

    // Even condition
    if (token == evensym) {

        get_token();
        expression();
        
        // Emit OPR even instruction 
        emit(OPR, 0, OPR_EVEN);
    } 
    // Relational condition
    else {

        expression();

        // Handle relational operators
        switch (token) {
            // Equal
            case eqsym:
                get_token();
                expression();
                emit(OPR, 0, OPR_EQL);
                break;
            // Not equal - emit OPR not equal instruction
            case neqsym:
                get_token();    
                expression();   
                emit(OPR, 0, OPR_NEQ);
                break;
            // Less than - emit OPR less than instruction
            case lessym:
                get_token();
                expression();
                emit(OPR, 0, OPR_LSS);
                break;
            // Less than or equal - emit OPR less than or equal instruction
            case leqsym:
                get_token();
                expression();
                emit(OPR, 0, OPR_LEQ);
                break;
            // Greater than - emit OPR greater than instruction
            case gtrsym:
                get_token();
                expression();  
                emit(OPR, 0, OPR_GTR);
                break;
            // Greater than or equal - emit OPR greater than or equal instruction
            case geqsym:
                get_token();
                expression();
                emit(OPR, 0, OPR_GEQ);
                break;
            // Default case for invalid operator - error message
            default:
                error("condition must contain comparison operator");
        }
    }
}

// Expression function
void expression() {

    // Variable for storing operator token
    TokenType stored_token;

    term();

    while (token == plussym || token == minussym) {
        
        // Store operator token
        stored_token = token;
        
        get_token();
        
        term();

        // Emit appropriate OPR instruction based on operator
        if (stored_token == plussym) {
            emit(OPR, 0, OPR_ADD);
        } 
        else {
            emit(OPR, 0, OPR_SUB);
        }
    }
}

// Term function
void term() {

    factor();


    // Loop as long as next token is * or /
    while (token == multsym || token == slashsym) {
        
        // Emit appropriate OPR instruction based on operator
        if (token == multsym) {
            get_token();
            factor();
            emit(OPR, 0, OPR_MUL);
        } 
        else {
            get_token();
            factor();
            emit(OPR, 0, OPR_DIV);
        }
    }
}

// Factor function
void factor(){

    // Temporary variable for symbol index
    int sym_index;

    // Identifier
    if (token == identsym) {

        // Lookup identifier in symbol table
        sym_index = symbol_table_lookup(identifier);
        
        // Error if not found
        if (sym_index == -1) {
            error("undeclared identifier");
        }
        
        // Load constant or variable value
        if (symbol_table[sym_index].kind == CONST) {
            
            // Emit LIT instruction for constant
            emit(LIT, 0, symbol_table[sym_index].val);
        } 
        else {
            
            // Emit LOD instruction for variable
            int L = current_level - symbol_table[sym_index].level;
            emit(LOD, L, symbol_table[sym_index].addr);
        }

        get_token();
    } 
    // Number
    else if (token == numbersym) {
        
        // Load number constant
        emit(LIT, 0, number);

        get_token();
    } 
    // Parenthesized expression
    else if (token == lparentsym) {

        get_token();
        expression();

        // Expect right parenthesis
        if (token != rparentsym) {
            error("right parenthesis must follow left parenthesis");
        }

        get_token();
    } 
    // Invalid factor error
    else {
        error("arithmetic equations must contain operands, parentheses, numbers, or symbols");
    }
}

// Helper function to convert OpCode to string for printing
const char* opcode_to_string(OpCode op) {
    switch(op) {
        case LIT: return "LIT";
        case OPR: return "OPR";
        case LOD: return "LOD";
        case STO: return "STO";
        case CAL: return "CAL";
        case INC: return "INC";
        case JMP: return "JMP";
        case JPC: return "JPC";
        case SYS: return "SYS";
        default: return "???";
    }
}

// Print generated assembly code function
void print_assembly() {

    // Open assembly.txt for writing
    FILE *out = fopen("elf.txt", "w");
    if (!out) {
        printf("Error opening assembly.txt\n");
        return;
    }
    
    // Print header for terminal in the format Line | OP | L | M (no header in file)
    printf("\nAssembly Code:\n\n");
    printf("%-6s%-6s%-6s%-6s\n", "Line", "OP", "L", "M");

    // Print each instruction to terminal and file
    for (int i = 0; i < code_len; i++) {
        
        // Print instruction to terminal
        printf("%-6d%-6s%-6d%-6d\n",
            i,
            opcode_to_string(code[i].op),
            code[i].l,
            code[i].m
        );

        // Write instruction to file in the format OP L M
        fprintf(out, "%d %d %d\n",
            code[i].op,
            code[i].l,
            code[i].m
        );
    }
    // Newline at end
    printf("\n");
    fprintf(out, "\n");
    
    // Close assembly output file
    fclose(out);
}

// Print symbol table function
void print_symbol_table() {
    
    // Print symbol table header
    printf("Symbol Table:\n\n");

    // Print table columns in the format Kind | Name | Value | Level | Address | Mark
    printf("%-5s| %-12s| %-6s| %-6s| %8s | %-7s\n",
        "Kind",
        "Name",
        "Value",
        "Level",
        "Address",
        "Mark"
    );

    // Divider line
    printf("----------------------------------------------------\n");

    // Print each symbol in the format Kind | Name | Value | Level | Address | Mark
    for (int i = 0; i < symbol_count; i++) {
        printf("%4d | %11s | %5d | %5d |  %7d | %4d\n", 
               symbol_table[i].kind, 
               symbol_table[i].name,
               symbol_table[i].val,
               symbol_table[i].level, 
               symbol_table[i].addr,
               symbol_table[i].mark
            );
    }

    // Newline at end
    printf("\n");
}

// Mark symbols at a given lexical level as used
void mark_level(int lvl) {
    for (int i = symbol_count - 1; i >= 0; --i) {
        if (symbol_table[i].level == lvl) {
            symbol_table[i].mark = 1;
        }
    }
}

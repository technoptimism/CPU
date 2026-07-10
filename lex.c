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
#define MAX_IDENT_LEN 11
#define MAX_NUM_LEN 5
#define MAX_TOKENS 500
#define num_reserved 15

// Token types
typedef enum {
skipsym = 1 ,            // Skip
identsym = 2,            // Identifier
numbersym = 3,           // Number
plussym = 4,             // +
minussym = 5,            // -
multsym = 6,             // *
slashsym = 7,            // /
eqsym = 8,               // =
neqsym = 9,              // <>
lessym = 10,             // <
leqsym = 11,             // <=
gtrsym = 12,             // >
geqsym = 13,             // >=
lparentsym = 14,         // (
rparentsym = 15,         // )
commasym = 16,           // ,
semicolonsym = 17,       // ;
periodsym = 18,          // .
becomessym = 19,         // :=
beginsym = 20,           // begin
endsym = 21,             // end
ifsym = 22,              // if
fisym = 23,              // fi
thensym = 24,            // then
whilesym = 25,           // while
dosym = 26,              // do
callsym = 27,            // call
constsym = 28,           // const
varsym = 29,             // var
procsym = 30,            // procedure        
writesym = 31,           // write
readsym = 32,            // read
elsesym = 33,            // else
evensym  = 34            // even
}TokenType;

// Struct to hold token and its lexeme
typedef struct {
    char lexeme[25];   // Lexeme string
    TokenType token;
    char error_msg[50];
}Token;

// Struct to hold list of tokens
typedef struct{
    Token tokens[MAX_TOKENS];
    int count;
}TokenList;

// Structure to hold reserved words and their tokens
typedef struct {
    char *word;
    TokenType token;
}ReservedWord;

// List of reserved words and their corresponding tokens
ReservedWord RESERVED[] = {             
    {"begin", beginsym},
    {"end", endsym},
    {"if", ifsym},
    {"fi", fisym},
    {"then", thensym},
    {"while", whilesym},
    {"do", dosym},
    {"call", callsym},
    {"const", constsym},
    {"var", varsym},
    {"procedure", procsym},
    {"write", writesym},
    {"read", readsym},
    {"else", elsesym},
    {"even", evensym}
};


// Function prototypes
int push_token(TokenType t);
int is_invisible(char c);
TokenType get_reserved_token(const char *word);
void init_token_list(TokenList *list);
void add_token(TokenList *list, const char *lexeme, TokenType token);
void add_error_token(TokenList *list, const char *lexeme, const char *error_msg);
void print_source_program(const char *filename);
void print_lexeme_table(TokenList *list);
void print_token_list(TokenList *list);
void scan_file(const char *filename, TokenList *list);


//----------------------------- Main ----------------------------
int main(int argc, char **argv){
    
    // Check for proper number of arguments
    if (argc != 2) {
        printf( "Usage: %s <source file>\n", argv[0] );
        return 1;
    }
    
    // Initialize token list
    TokenList tokens;
    init_token_list(&tokens);

    // print the source program
    print_source_program(argv[1]);

    // Scan the file and populate the token list
    scan_file(argv[1], &tokens);

    // Print the lexeme table
    print_lexeme_table(&tokens);

    // Print the token list
    print_token_list(&tokens);

    return 0;
}



//----------------------------- Functions ----------------------------

// Check if character is invisible (whitespace)
int is_invisible(char c){
    return c == ' ' || c == '\r' || c == '\t' || c == '\n';
}

// Initialize the token list by setting initial count to 0
void init_token_list(TokenList *list) {
    list->count = 0;
}

// Add a token to the token list
void add_token(TokenList *list, const char *lexeme, TokenType token){

    // Error check for token list overflow
    if (list->count >= MAX_TOKENS) {
        printf( "Token list limit exceeded\n" );
        return;
    }
    
    // Copy lexeme and token type into the list
    strcpy(list->tokens[list->count].lexeme, lexeme);
    list->tokens[list->count].token = token;
    
    // Set error message to empty
    list->tokens[list->count].error_msg[0] = '\0';
    
    // Increment token count
    list->count++;
}

// Add an error token to the token list
void add_error_token(TokenList *list, const char *lexeme, const char *error_msg) {
    if (list->count >= MAX_TOKENS) {
        printf("Token list limit exceeded\n");
        return;
    }
    // Copy lexeme and set token type to 'skipsym' for errors
    strcpy(list->tokens[list->count].lexeme, lexeme);
    list->tokens[list->count].token = skipsym;
    
    // Copy the error message
    strcpy(list->tokens[list->count].error_msg, error_msg);
    
    // Increment token count
    list->count++;
}

// Scan fileand populate the token list
void scan_file(const char *filename, TokenList *list){

    // Open the file
    FILE *file = fopen(filename, "r");
    // Error check for file opening
    if (!file) {
        printf( "Error opening file\n" );
        return;
    }
    
   
    char buffer[50];                    // Buffer to hold lexemes
    int pos = 0;                        // Current position in buffer 
    int c;                              // Current character 
    
    // Read characters until EOF
    while ((c = fgetc(file)) != EOF) {
        // Skip invisible characters
        if (is_invisible(c)) {
            continue;
        }

        // Comment handling
        if (c == '/') {
            // Check for /*
            int next_c = fgetc(file);
            if (next_c == '*') {
                int prev = 0;
                // Now inside a comment; skip until closing */ or EOF
                while ((c = fgetc(file)) != EOF){
                    if (prev == '*' && c == '/') {
                        break;
                    }
                    // Update previous character
                    prev = c;
                }
                continue;
            }
            // If no '*' found, then it's a division operator
            else {
                // Push the next character back to stream
                ungetc(next_c, file);
                add_token(list, "/", slashsym);                              // Add / token
                continue;
            }
        }
        // Identifier and reserved word handling
        else if (isalpha(c)) {
            
            buffer[0] = c;
            pos = 1;

            // Read the rest of the identifier
            while ((c = fgetc(file)) != EOF && (isalnum(c))) {
                    buffer[pos++] = c;
            }
            // Null-terminate the string
            buffer[pos] = '\0';
            
            if (c != EOF) {
                // Push back the last read character
                ungetc(c, file);
            }

            // Check identifier length and add token
            if (pos > MAX_IDENT_LEN) {
                add_error_token(list, buffer, "Identifier too long");       // Add error token
            }
            else {
                TokenType token = get_reserved_token(buffer);
                add_token(list, buffer, token);                             // Add identifier or reserved word token
            }
        }
        // Number handling
        else if (isdigit(c)) {
            buffer[0] = c;
            pos = 1;
            // Read the rest of number
            while ((c = fgetc(file)) != EOF && isdigit(c)) {
                buffer[pos++] = c;

            }
            // Null-terminate the string
            buffer[pos] = '\0';
            
            // Push the last read character back to stream
            ungetc(c, file);

            // Check for number length and add token
            if (pos > MAX_NUM_LEN) {
                add_error_token(list, buffer, "Number too long");   // Add error token
            }
            else {
                add_token(list, buffer, numbersym);                 // Add number token
            }
        }

        // Special symbols handling
        else {
            switch (c) {
                // Arithmetic operators
                case '+':
                    add_token(list, "+", plussym);                  // Add + token
                    break;
                case '-':
                    add_token(list, "-", minussym);                 // Add - token
                    break;
                case '*':
                    add_token(list, "*", multsym);                  // Add * token
                    break;
                case '=':
                    add_token(list, "=", eqsym);                    // Add = token
                    break;
                case '<': {
                    int next_c = fgetc(file);
                    // Check for <= or <>
                    if (next_c == '>') {   
                        add_token(list, "<>", neqsym);              // Add <> token  
                    }
                    else if (next_c == '=') {
                        add_token(list, "<=", leqsym);              // Add <= token
                    }

                    // Single <
                    else {
                        add_token(list, "<", lessym);               // Add < token
                        if (next_c != EOF) {
                            // Push the next character back if not EOF
                            ungetc(next_c, file);
                        }
                    }
                    break;
                }
                case '>': {
                    int next_c = fgetc(file);
                    //check for >=
                    if (next_c == '=') {
                        add_token(list, ">=", geqsym);              // Add >= token
                    }
                    // Single >
                    else {
                        add_token(list, ">", gtrsym);               // Add > token
                        if (next_c != EOF) {
                            // Push the next character back if not EOF
                            ungetc(next_c, file);
                        }
                    }
                    break;
                }
                // Parentheses, commas, semicolons, periods
                case '(':
                    add_token(list, "(", lparentsym);               // Add left parenthesis token
                    break;
                case ')':
                    add_token(list, ")", rparentsym);               // Add right parenthesis token
                    break;
                case ',':
                    add_token(list, ",", commasym);                 // Add comma token
                    break;
                case ';':
                    add_token(list, ";", semicolonsym);             // Add semicolon token
                    break;
                case '.':
                    add_token(list, ".", periodsym);                // Add period token 
                    break;
                case ':': {
                    // Read next character to check for :=  
                    int next_c = fgetc(file);               
                    if (next_c == '=') {
                        add_token(list, ":=", becomessym);          // Add := token
                    }
                    // Invalid character
                    else {
                        // Construct invalid character string
                        char invalid_char[2] = {c, '\0'};
                        
                        if (next_c != EOF) {
                            // Push the next character back if not EOF
                            ungetc(next_c, file);
                        }
                        
                        // Add error token
                        add_error_token(list, invalid_char, "Invalid character");  // Add error token
                    }
                    break;
                }
                // Default case for invalid characters
                default: {
                    // Construct invalid character string
                    char invalid_char[2] = {c, '\0'};
                    
                    // Add error token
                    add_error_token(list, invalid_char, "Invalid symbol");       // Add error token
                }
            }
    
        }
    }

    // Close the file
    fclose(file);
}

// Print the source program
void print_source_program(const char *filename){
    // Open the file
    FILE *file = fopen(filename, "r");

    // Error check
    if (!file) {
        printf( "Error opening file\n" );
        return;
    }

    printf("Source Program:\n\n");
    char ch;

    // Read and print each character until EOF
    while ((ch = fgetc(file)) != EOF) {
        putchar(ch);
    }
    
    // New lines for formatting
    printf("\n\n");
    
    // Close the file
    fclose(file);
}

// Print the lexeme table
void print_lexeme_table(TokenList *list){
    // Print table header
    printf("Lexeme Table:\n\n");
    printf("Lexeme\t\tToken Type\n");

    // Iterate through tokens and print them
    for (int i = 0; i < list->count; i++) {
        Token *t = &list->tokens[i];
        
        if (t->token == skipsym && t->error_msg[0] != '\0') {
            // Error token
            printf("%-10s\t%s\n", t->lexeme, t->error_msg);
        } 
        else {
            // Regular token
            printf("%-10s\t%d\n", t->lexeme, t->token);
        }
    }
    printf("\n");
}

// Print the token list
void print_token_list(TokenList *list){
    
    // Open/create tokenlist.txt for writing
    FILE *token_output = fopen("tokens.txt", "w");
    if (token_output == NULL) {
        printf("Error opening tokenlist.txt for writing\n");
        return;
    }
    

    printf("Token List:\n\n");
    for (int i = 0; i < list->count; i++) {
        // Print token type
        printf("%d ", list->tokens[i].token);
        
        // Write token type to file
        fprintf(token_output, "%d ", list->tokens[i].token);

        // Print lexeme for identifiers and numbers
        if (list->tokens[i].token == identsym || list->tokens[i].token == numbersym) {
            printf("%s ", list->tokens[i].lexeme);
            fprintf(token_output, "%s ", list->tokens[i].lexeme);
        }
        fprintf(token_output, "\n");
    }
    printf("\n");
    

    // Close token output file
    fclose(token_output);
}

// Get the token type for a reserved word or identifier
TokenType get_reserved_token(const char *word){
    for (int i = 0; i < num_reserved; i++) {
        // Iterate through reserved words and compare
        if (strcmp(word, RESERVED[i].word) == 0) {
            return RESERVED[i].token;
        }
    }
    // Not a reserved word, it's an identifier
    return identsym;
}

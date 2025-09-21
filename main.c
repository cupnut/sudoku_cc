#include "stdio.h"
#include "string.h"
#include "stdint.h"
#include "stdlib.h"
#include "assert.h"
#include "ctype.h"

#define pop_arg(args, argi) (assert(argi > 0), --argi, *args++)

#define INPUT_BUFFER_SIZE 128
#define SUDOKU_DIGIT_NUM 9
#define SUDOKU_DIGIT_MAX 45

enum targetbox_e {
  X33,
  CAGE_CONTAINED,
  CAGE_OVERLAPPED,
  INVALID
};

typedef struct {
  char* buffer;
  size_t size;
  size_t cap;
} strbuilder_t;

typedef struct {
  int overlaps;
  int allowDups;
  int* digitUseCounts;
} cageparams_t;

size_t arglen(char* str, size_t strlen, int start) {
  if(!str || strlen == 0) return 0;

  size_t len = 0;
  while(start < strlen && str[start] != ' ' && str[start++] != '\n') ++len; 
  return len;
}

int getint(char* str, size_t len) {
  if(len == 0 || !str) return 0;

  int number = 0;
  int polarity = 1;
  int index = 0;

  if(str[index] == '-') { 
    if(len <= 1) return 0;
    polarity = -1;
    ++index;
  }

  while(index < len && isdigit(str[index])) {
    int digit = str[index++] - '0'; // convert digit-char to numeric value
    number = number * 10 + digit;
  }

  return number * polarity;
}

void write_combination(int depth, int* currentCombination, strbuilder_t* sb) {
  char lineBuffer[64] = {0};
  int offset = 0;

  // write digits to line seperated by whitespaces
  for(int i = 0; i < depth; ++i) {
    int written = snprintf(
      lineBuffer + offset, 
      sizeof(lineBuffer) - offset,
      "%d%s",
      currentCombination[i], (i == depth - 1) ? "" : " "
    );
    offset += written;
  }
  // append new line symbol
  snprintf(lineBuffer + offset, sizeof(lineBuffer) - offset, "\n");

  // append line to output string
  size_t lenline = strlen(lineBuffer);
  memcpy(sb->buffer + sb->size, lineBuffer, lenline + 1); // +1 to copy null terminator
  sb->size += lenline;
}

void find_combinations(int total, int spots, int startDigit, int digitsInUse, int* currentCombination, int depth, strbuilder_t* sb, cageparams_t* cp, int recursionStart) {
  // base case: successfully found a combination of the correct size that sums to the total
  if(total == 0 && depth == spots) {
    write_combination(depth, currentCombination, sb);
    return;
  }

  // base case: recursive Failure
  if(total < 0 || depth >= spots) return;

  // recursion step
  for(int i = startDigit; i <= SUDOKU_DIGIT_NUM; ++i) {
    if(i > total) return;

    // A 1 indicates that a digit can *not* be used. Therefore, XOR
    int isAvail = ((digitsInUse & (1 << (i - 1))) ^ (1 << (i - 1)));
    
    if(isAvail) { 
      if(cp->overlaps) {
        // A digit can only be used twice in a *standard* cage
        if(cp->digitUseCounts[i] >= 2) {
          continue;
        }
        ++cp->digitUseCounts[i];
      }
      
      currentCombination[depth] = i;

      int nextDigit = cp->allowDups ? i : i + 1;

      find_combinations(total - i, spots, nextDigit, digitsInUse, currentCombination, depth + 1, sb, cp, 0);

      if(cp->overlaps && recursionStart) {
        memset(cp->digitUseCounts, 0, SUDOKU_DIGIT_NUM);
      }
    }
  }
}

char* handle_cage(int boxTotal, int numSpots, int digitsInUse, int overlapsBoxes, int allowDuplicates) {
  enum {
    BUFFERSIZE = 512
  };
  static char outBuffer[BUFFERSIZE];
  static int digitUseCounts[SUDOKU_DIGIT_NUM];
  static int currentCombination[SUDOKU_DIGIT_NUM];

  // reset buffers
  memset(outBuffer, 0, BUFFERSIZE);
  memset(digitUseCounts, 0, SUDOKU_DIGIT_NUM);
  memset(currentCombination, 0, SUDOKU_DIGIT_NUM);

  strbuilder_t sb = { .buffer = outBuffer, .size = 0, .cap = BUFFERSIZE };
  cageparams_t cp = { .overlaps = overlapsBoxes, .allowDups = allowDuplicates, .digitUseCounts = digitUseCounts};

  find_combinations(boxTotal, numSpots, 1, digitsInUse, currentCombination, 0, &sb, &cp, 1);

  if(sb.size == 0) return "\0";
  return outBuffer;
}

int parse_targetbox(char* str) {
  if(strcmp(str, "cc") == 0) return CAGE_CONTAINED;
  if(strcmp(str, "co") == 0) return CAGE_OVERLAPPED;
  if(strcmp(str, "x3") == 0) return X33;
  return INVALID;
}

int main(int argc, char** argv) {
  int quitSignal = 0;

  char inputBuffer[INPUT_BUFFER_SIZE];

  char errstrBuff[INPUT_BUFFER_SIZE];
  errstrBuff[INPUT_BUFFER_SIZE - 1] = '\0';
  char* errstrp = errstrBuff;

  while(!quitSignal) {

    printf("> ");

    if(fgets(inputBuffer, INPUT_BUFFER_SIZE, stdin) != NULL){
      size_t bytesRead = strlen(inputBuffer);

      if(bytesRead > 0 && inputBuffer[bytesRead - 1] == '\n') {
        int it = 0;
        int ordinal = 0;
        int targetBox = INVALID;
        int boxTotal = 0; // cages's sum of its digits
        int boxSpots = 0; // number of digits in  the cage
        int digitsInUse = 0;

        while(it < bytesRead) {
          size_t argLength = arglen(inputBuffer, bytesRead, it);
          char* argStart = &inputBuffer[it];
          
          switch(ordinal) {
            case 0: {
              char* argterminate = argStart + argLength; 
              *argterminate = '\0';
              targetBox = parse_targetbox(argStart);
              *argterminate = ' ';
            } break;

            case 1: {
              if(targetBox == INVALID) {
                errstrp = "Invalid Box\0";
                goto erronousEnd;
              }
              boxTotal = getint(argStart, argLength);

              if(boxTotal < 0) {
                errstrp = "Invalid Box Total\0";
                goto erronousEnd;
              }
            } break;

            case 2: {
              boxSpots = getint(argStart, argLength);

              if(boxSpots < 0 || boxSpots > SUDOKU_DIGIT_NUM) {
                errstrp = "Invalid Number of Spots Available in the Cage\0";
                goto erronousEnd;
              }
            } break;

            default: {
              int digit = getint(argStart, argLength);
              if(digit > 0 && digit <= SUDOKU_DIGIT_NUM) digitsInUse |= 1 << digit;
            }
          }
          
          // Advance index to next arg. +1 for whitespace
          it += (argLength + 1); 
          ++ordinal;
        }

        if(ordinal < 2) {
          errstrp = "Invalid Number of Arguments\0";
          goto erronousEnd;
        }

        char* outstr;
        switch(targetBox) {
          case X33: {
            outstr = "Not supported yet\n\0";
          } break;
          case CAGE_CONTAINED: {
            outstr = handle_cage(boxTotal, boxSpots, digitsInUse, 0, 0);
          } break;
          case CAGE_OVERLAPPED: {
            outstr = handle_cage(boxTotal, boxSpots, digitsInUse, 1, 1);
          } break;
        }
        printf("%s", outstr);
      }
    }
    else quitSignal = 1;

    continue;

  erronousEnd: 
    printf("%s\n", errstrp);
  }
}
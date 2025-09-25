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
  int numDupsAllowed;
  int* digitUseCounts;
} cageparams_t;

size_t arglen(char* str, size_t strlen, int start) {
  if(!str || strlen == 0) return 0;

  size_t len = 0;
  while(start < strlen && str[start] != ' ' && str[start++] != '\n') ++len; 
  return len;
}

int getint(char* str, size_t len) {
  if(len == 0 || !str) return -1;

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

    int asIndex = i - 1;
    // A 1 indicates that a digit can *not* be used. Therefore, XOR
    int isAvail = ((digitsInUse & (1 << (asIndex))) ^ (1 << (asIndex)));
    
    if(isAvail) { 
      if(cp->overlaps) {
        // A digit can only be used twice in a *standard* cage
        if(cp->digitUseCounts[asIndex] >= cp->numDupsAllowed) {
          continue;
        }
        ++cp->digitUseCounts[asIndex];
      }
      
      currentCombination[depth] = i;

      int nextDigit = cp->allowDups ? i : i + 1;

      find_combinations(total - i, spots, nextDigit, digitsInUse, currentCombination, depth + 1, sb, cp, 0);

      if(cp->overlaps)
        --cp->digitUseCounts[asIndex];
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
  memset(digitUseCounts, 0, sizeof(digitUseCounts));
  memset(currentCombination, 0, sizeof(currentCombination));

  strbuilder_t sb = { .buffer = outBuffer, .size = 0, .cap = BUFFERSIZE };
  cageparams_t cp = { .overlaps = overlapsBoxes, .allowDups = allowDuplicates, .numDupsAllowed = 2, .digitUseCounts = digitUseCounts};

  find_combinations(boxTotal, numSpots, 1, digitsInUse, currentCombination, 0, &sb, &cp, 1);

  if(sb.size == 0) return "\0";
  return outBuffer;
}

char* handle_x33(int x33TotalCageValue) {
  enum {
    BUFFERSIZE = 32
  };
  static char outBuffer[BUFFERSIZE];
  sprintf(outBuffer, "%d\n\0", x33TotalCageValue - SUDOKU_DIGIT_MAX);
  return outBuffer;
}

int parse_targetbox(char* str) {
  if(strcmp(str, "cc") == 0) return CAGE_CONTAINED;
  if(strcmp(str, "co") == 0) return CAGE_OVERLAPPED;
  if(strcmp(str, "x3") == 0) return X33;
  return INVALID;
}

int parsecmd_x33(int it, size_t bytesRead, char* inputBuffer) {
  if(it >= bytesRead) {
    printf("Invalid Input\n");
    return -1;
  }

  size_t argLength = arglen(inputBuffer, bytesRead, it);
  int numread = getint(inputBuffer + it, argLength);

  if(numread == -1) {
    printf("x33 cmd: missing number as seccond argument");
  }

  return numread;
}

struct parsecmd_cage_t { 
  int success;
  int boxTotal;
  int boxSpots;
  int digitsInUse;
} parsecmd_cage(int it, size_t bytesRead, char* inputBuffer) {
  int ordinal = 0;

  struct parsecmd_cage_t pct = {
    .success = 0,
    .boxTotal = 0, // cages's sum of its digits
    .boxSpots = 0, // number of digits in  the cage
    .digitsInUse = 0
  };

  while(it < bytesRead) {
    size_t argLength = arglen(inputBuffer, bytesRead, it);
    char* argStart = inputBuffer + it;

    switch(ordinal) {
      case 0: 
      {
        pct.boxTotal = getint(argStart, argLength);
        if(pct.boxTotal < 0) {
          printf("Invalid Box Total\n");
          return pct;
        }
      } break;

      case 1: 
      {
        pct.boxSpots = getint(argStart, argLength);

        if(pct.boxSpots < 0 || pct.boxSpots > SUDOKU_DIGIT_NUM) {
          printf("Invalid Number of Spots Available in the Cage\n");
          return pct;
        }
      } break;

      default: 
      {
        int digit = getint(argStart, argLength);
        if(digit > 0 && digit <= SUDOKU_DIGIT_NUM) pct.digitsInUse |= 1 << digit;
      }
    }

    it += (argLength + 1);
    ++ordinal;
  }

  if(ordinal < 1) {
    printf("Invalid Number of Arguments\n");
    return pct;
  }

  pct.success = 1;
  return pct;
}

int main(int argc, char **argv)
{
  int quitSignal = 0;

  char inputBuffer[INPUT_BUFFER_SIZE];

  char errstrBuff[INPUT_BUFFER_SIZE];
  errstrBuff[INPUT_BUFFER_SIZE - 1] = '\0';
  char *errstrp = errstrBuff;

  while(!quitSignal) {
    printf("> ");

    if(fgets(inputBuffer, INPUT_BUFFER_SIZE, stdin) != NULL) {
      size_t bytesRead = strlen(inputBuffer);

      if(bytesRead > 0 && inputBuffer[bytesRead - 1] == '\n') {
        int it = 0;

        if(it >= bytesRead) {
          printf("Invalid Input\n");
          break;
        }

        size_t argLength = arglen(inputBuffer, bytesRead, it);
        char *argStart = &inputBuffer[it];
        char *argterminate = argStart + argLength;
        int targetBox = INVALID;

        *argterminate = 0;
        targetBox = parse_targetbox(argStart);
        *argterminate = ' ';

        it += (argLength + 1);

        char *outstr = NULL;
        switch (targetBox) {
          case X33:
          {
            int x33Total = parsecmd_x33(it, bytesRead, inputBuffer);
            if(x33Total == -1) break;

            outstr = handle_x33(x33Total);
          } break;

          case CAGE_CONTAINED: 
          {
            struct parsecmd_cage_t pct = parsecmd_cage(it, bytesRead, inputBuffer);
            if (!pct.success) break;

            outstr = handle_cage(pct.boxTotal, pct.boxSpots, pct.digitsInUse, 0, 0);
          } break;

          case CAGE_OVERLAPPED:
          {
            struct parsecmd_cage_t pct = parsecmd_cage(it, bytesRead, inputBuffer);
            if (!pct.success) break;

            outstr = handle_cage(pct.boxTotal, pct.boxSpots, pct.digitsInUse, 1, 1);
          } break;  

          default:
          {
            printf("Invalid cmd\n");
          }
        }

        if (outstr) printf(outstr);
      }
      else
        quitSignal = 1;
    }
  }
}
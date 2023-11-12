#include "types.h"
#include "param.h"

struct history
{
    char bufferArr[MAX_HISTORY][INPUT_BUF]; 
    uint lengthsArr[MAX_HISTORY];           
    uint lastIndex;                 
    int memCommand;              
    int current;                 
};

struct history history_buffer;

void eraseLine(void);
void oldBuffer(void);
void eraseInput();
void copyBuffer(char *bufToPrintOnScreen, uint length);
void copyInput(char *bufToSaveInInput, uint length);
void saveCommand();
int getFromHistory(char *buffer, int historyId);
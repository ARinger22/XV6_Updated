#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "get_history.h"

int getFromHistory(char *buffer, int historyId)
{
    // historyId != index of command in history_buffer.bufferArr
    if (historyId < 0 || historyId > MAX_HISTORY - 1)
        return 2;
    if (historyId >= history_buffer.memCommand)
        return 1;
    memset(buffer, '\0', INPUT_BUF);
    int tempIndex = (history_buffer.lastIndex + historyId) % MAX_HISTORY;
    memmove(buffer, history_buffer.bufferArr[tempIndex], history_buffer.lengthsArr[tempIndex]);
    return 0;
}
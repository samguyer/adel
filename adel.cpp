
#include <adel.h>

// ----------------------------------------------------------------------
// Global storage for the stack and state

Astate adel_stack[1 << MAX_DEPTH];

uint16_t adel_current;


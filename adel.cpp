
#include <adel.h>

// ----------------------------------------------------------------------
// Global storage for the stack and state

// -- Stack: pointers to activation records indexed by "current"
void * AdelStack::stack[1 << stack_depth];

// -- Activation records: mirrors stack, but saves the pointers
void * AdelStack::ars[1 << stack_depth];

// -- Current function (index into stack)
int AdelStack::current;

// -- Helper methods
void * AdelStack::init_ar(int index, int size_in_bytes)
{
  void * ar = ars[index];
  if (ar == 0) {
    ar = malloc(size_in_bytes);
    ars[index] = ar;
  }
  memset(ar, 0, size_in_bytes);
  stack[index] = ar;
  return ar;
}



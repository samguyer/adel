
#include <stdint.h>

/** Adel multi-stack
 *
 * Stack of function states. Since Adel is emulating concurrency, the
 *  stack is not a linear data structure, but a tree of currently running
 *  functions. This implementation limits the tree to a binary tree, so
 *  that we can use a heap representation. This restriction means that we
 *  must use a fork-join model of parallelism. */

template<int stack_depth>
class AdelStack
{
  // -- Stack: pointers to activation records indexed by "current"
  static void * stack[1 << stack_depth];

  // -- Activation records: mirrors stack, but saves the pointers
  static void * ars[1 << stack_depth];

  // -- Current function (index into stack)
  static int current;

  // -- Helper methods
  static void * init_ar(int index, int size_in_bytes)
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
};

/** adel status
 * 
 *  All Adel functions return an enum that indicates whether the routine is
 *  done or has more work to do.
 */
class adel
public:
  typedef enum { NONE, DONE, CONT } _status;
  
private:
  _status m_status;
  
public:
  adel(_status s) : m_status(s) {}
  adel() : m_status(NONE) {}
  adel(const adel& other) : m_status(other.m_status) {}
  explicit adel(bool b) : m_status(NONE) {}
  
  bool done() const { return m_status == DONE; }
  bool cont() const { return m_status == CONT; }
};

// ------------------------------------------------------------
//   Internal macros

/** Child function index
 *  Using the heap structure, this is really fast. */
#define achild(c) ((a_my_index << 1) + c)

/** Initialize
 */
#define ainit(c) (AdelStack::stack[c]->line = 0)

/** my(v)
 *
 *  Get a local variable from the activation record */
#define my(v) (a_me->v)

// ------------------------------------------------------------
//   End-user macros

/** aonce
 *
 *  Use astart in the Arduino loop function to initiate the Adel function f
 *  (and run all Adel functions below it).
 */
#define aonce( f )				\
  AdelStack::current = 0;				\
  f;

/** aforever
 *
 *  Run the top adel function over and over.
 */
#define aforever( f )					\
  AdelStack::current = 0;					\
  adel f_status = f;					\
  if (f_status.done()) { ainit(0); }

/** abegin
 *
 * Always add abegin and aend to every adel function. Any local variables
 * that need to persist in this function should be declared immediately
 * after abegin.
 */
#define abegin					\
  int a_my_index = AdelStack::current;		\
  struct _adel_ar { 

/** afirst:
 * 
 * Use afirst: to mark the first statement of the Adel function.
 */
#define afirst								\
    uint16_t line;							\
    uint16_t wait;							\
  } * a_me = (_adel_ar *) AdelStack::stack[a_my_index];			\
  if (a_me == 0)							\
    a_me = (_adel_ar *) AdelStack::init_ar(a_my_index, sizeof(_adel_ar));	\
  adel f_status, g_status;						\
  switch (my(line)) {							\
  case 0

/** aend
 *
 * Must be the last thing in the Adel function.
 */
#define aend						\
  case ADEL_FINALLY: ;					\
  }							\
  my(line) = ADEL_FINALLY;				\
  return Adel::DONE;

/** afinally
 *
 *  Optionally, end with afinally to do some action whenever the function
 *  returns (for any reason)
#define afinally( f )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
  case ADEL_FINALLY:					\
    adel_current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return Adel::CONT;
*/

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)					\
  my(line) = __LINE__;					\
  my(wait) = millis() + t;				\
  case __LINE__:					\
  if (millis() < my(wait)) return Adel::CONT;

/** andthen
 *
 *  Semantics: execute f synchronously, until it is done (returns false)
 *  Example use:
 *     andthen( turn_on_light() );
 *     andthen( turn_off_light() );
 */
#define andthen( f )					\
  my(line) = __LINE__;					\
    ainit(achild(1));					\
 case __LINE__:						\
    adel_current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return Adel::CONT;

/** awaituntil
 *  Wait asynchronously for a condition to become true. Note that this
 *  condition CANNOT be an adel function.
 */
#define awaituntil( c )					\
  my(line) = __LINE__;					\
 case __LINE__:						\
    if ( ! ( c ) ) return Adel::CONT

/** aforatmost
 *
 *  Semantics: do f until it completes, or until the timeout
 */
#define aforatmost( t, f )				\
  my(line) = __LINE__;					\
  my(wait) = millis() + t;				\
  case __LINE__:					\
    f_status = f;					\
    if (f_status.cont() && millis() < my(wait)) return Adel::CONT;

/** aboth
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
 */
#define aboth( f , g )					\
  my(line) = __LINE__;					\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: {					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (f_status.cont() || g_status.cont())		\
      return Adel::CONT;   }

/** adountil
 *
 *  Semantics: execute f until g completes.
 */
#define adountil( f , g )				\
  my(line) = __LINE__;					\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: 					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (g_status.cont()) return Adel::CONT;

/** auntileither
 *
 *  Semantics: execute c and f asynchronously until either one of them
 *  finishes (contrast with aboth). This construct behaves like a
 *  conditional statement: it should be followed by a true statement and
 *  optional false statement, which are executed depending on whether the
 *  first function finished first or the second one did. Example use:
 *
 *     auntileither( button(), flash_led() ) { 
 *       // button finished first 
 *     } else {
 *       // light finished first
 *     }
 */
#define auntileither( f , g )				\
  my(line) = __LINE__;					\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: 					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (f_status.cont() && g_status.cont()) return Adel::CONT;	\
    if (f_status.done())

/** areturn
 * 
 *  Semantics: leave the function immediately, and communicate to the
 *  caller that it is done.
 */
#define areturn				   \
  my(line) = ADEL_FINALLY;		   \
    return Adel::CONT;

// ------------------------------------------------------------
//  Version 2 of the library

#ifdef ADEL_V2

/** Adel state
 *
 * This class holds all the runtime information for a single function
 * invocation. The "line" is the current location in the function; the
 * "wait" is the time we're waiting for (in a delay); the "i" is 
 * a loop variable for use in afor. */
class Astate
{
public:
  uint16_t line;
  uint16_t wait;
  uint16_t i;
  Astate() : line(0), wait(0), i(0) {}
};

/** Adel stack
 *  Stack of function states. Since Adel is emulating concurrency, the
 *  stack is not a linear data structure, but a tree of currently running
 *  functions. This implementation limits the tree to a binary tree, so
 *  that we can use a heap representation. This restriction means that we
 *  must use a fork-join model of parallelism. */

#ifndef ADEL_DEPTH
#define ADEL_DEPTH 5
#endif

extern Astate adel_stack[1 << MAX_DEPTH];

/** Current function */
extern uint16_t adel_current;

/** Child function index
 * Using the heap structure, this is really fast. */
#define achild(c) ((a_my_index << 1) + c)

/** Start a new async function call */
#define ainit_child(c) adel_stack[achild(c)].line = 0

#define ADEL_FINALLY 99999

/** adel status
 * 
 *  All Adel functions return an enum that indicates whether the routine is
 *  done or has more work to do.
 */
class Adel
{
public:
  typedef enum { NONE, DONE, CONT } _status;
  
private:
  _status m_status;
  
public:
  Adel(_status s) : m_status(s) {}
  Adel() : m_status(NONE) {}
  Adel(const Adel& other) : m_status(other.m_status) {}
  explicit Adel(bool b) : m_status(NONE) {}
  
  bool done() const { return m_status == DONE; }
  bool cont() const { return m_status == CONT; }
};

/** aonce
 *
 *  Use astart in the Arduino loop function to initiate the Adel function f
 *  (and run all Adel functions below it).
 */
#define aonce( f )  \
  adel_current = 0; \
  f;

/** aforever
 *
 *  Run the top adel function over and over.
 */
#define aforever( f )					\
  adel_current = 0;					\
  Adel f_status = f;					\
  if (f_status.done()) { adel_stack[0].line = 0; }

/** abegin
 *
 * Always add abegin and aend to every adel function
 */
#define abegin						\
  Adel f_status, g_status;				\
  int a_my_index = adel_current;			\
  Astate& a_me = adel_stack[a_my_index];		\
  switch (a_me.line) {					\
  case 0:

#define aend						\
  case ADEL_FINALLY: ;					\
  }							\
  a_me.line = ADEL_FINALLY;				\
  return Adel::DONE;

/** afinally
 *
 *  Optionally, end with afinally to do some action whenever the function
 *  returns (for any reason)
 */
#define afinally( f )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
  case ADEL_FINALLY:					\
    adel_current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return Adel::CONT;

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)					\
    a_me.line = __LINE__;				\
    a_me.wait = millis() + t;				\
  case __LINE__:					\
  if (millis() < a_me.wait) return Adel::CONT;

/** andthen
 *
 *  Semantics: execute f synchronously, until it is done (returns false)
 *  Example use:
 *     andthen( turn_on_light() );
 *     andthen( turn_off_light() );
 */
#define andthen( f )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
 case __LINE__:						\
    adel_current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return Adel::CONT;

/** awaituntil
 *  Wait asynchronously for a condition to become true. Note that this
 *  condition CANNOT be an adel function.
 */
#define awaituntil( c )					\
    a_me.line = __LINE__;				\
 case __LINE__:						\
    if ( ! ( c ) ) return Adel::CONT

/** aforatmost
 *
 *  Semantics: do f until it completes, or until the timeout
 */
#define aforatmost( t, f )				\
    a_me.line = __LINE__;				\
    a_me.wait = millis() + t;				\
  case __LINE__:					\
    f_status = f;					\
    if (f_status.cont() && millis() < a_me.wait) return Adel::CONT;

/** aboth
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
 */
#define aboth( f , g )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
    ainit_child(2);					\
  case __LINE__: {					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (f_status.cont() || g_status.cont())		\
      return Adel::CONT;   }

/** adountil
 *
 *  Semantics: execute f until g completes.
 */
#define adountil( f , g )				\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
    ainit_child(2);					\
  case __LINE__: 					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (g_status.cont()) return Adel::CONT;

/** auntileither
 *
 *  Semantics: execute c and f asynchronously until either one of them
 *  finishes (contrast with aboth). This construct behaves like a
 *  conditional statement: it should be followed by a true statement and
 *  optional false statement, which are executed depending on whether the
 *  first function finished first or the second one did. Example use:
 *
 *     auntileither( button(), flash_led() ) { 
 *       // button finished first 
 *     } else {
 *       // light finished first
 *     }
 */
#define auntileither( f , g )				\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
    ainit_child(2);					\
  case __LINE__: 					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (f_status.cont() && g_status.cont()) return Adel::CONT;	\
    if (f_status.done())

/** afor_i
 * 
 * Adel-friendly for loop. The issue is that the Adel execution model makes
 * it hard to have local variables, like the loop control variable. This
 * version of a for loop is more like a Fortran for loop: it just gives you
 * a variables that ranges over sequence of values. The name of the loop
 * variable is baked into it.
 */
#define afor_i( start, end, step )			\
    a_me.line = __LINE__;				\
    a_me.i = start;					\
  case __LINE__:					\
    for (int i = a_me.i; i <= end; a_me.i = (i = (i + step)))

/** areturn
 * 
 *  Semantics: leave the function immediately, and communicate to the
 *  caller that it is done.
 */
#define areturn				   \
    a_me.line = ADEL_FINALLY;		   \
    return Adel::CONT;

#endif

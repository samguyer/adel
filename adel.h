#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

#ifndef ADEL_V3
#define ADEL_V3

/** Adel runtime
 *
 * Stack of function states. Since Adel is emulating concurrency, the
 *  stack is not a linear data structure, but a tree of currently running
 *  functions. This implementation limits the tree to a binary tree, so
 *  that we can use a heap representation. This restriction means that we
 *  must use a fork-join model of parallelism. */

struct AdelAR
{
  uint16_t line;
  uint32_t wait;
};

#define ADEL_FINALLY 0xFFFF

template<int stack_depth>
class AdelRuntime
{
public:
  // -- Stack: pointers to activation records indexed by "current"
  AdelAR * stack[1 << stack_depth];

  // -- Current function (index into stack)
  int current;

  // -- Constructor: set everything to null
  AdelRuntime() : current(0)
  {
    for (int i = 0; i < (1 << stack_depth); i++) {
      stack[i] = 0;
    }
  }
    
  // -- Helper method to initialize an activation record
  //    for the first time (note: uses malloc)
  AdelAR * init_ar(int index, int size_in_bytes)
  {
    AdelAR * ar = (AdelAR *) malloc(size_in_bytes);
    ar->line = 0;
    ar->wait = 0;
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
{
public:
  typedef enum { NONE, DONE, CONT, YIELD } _status;
  
private:
  _status m_status;
  
public:
  adel(_status s) : m_status(s) {}
  adel() : m_status(NONE) {}
  adel(const adel& other) : m_status(other.m_status) {}
  explicit adel(bool b) : m_status(NONE) {}
  
  bool done() const { return m_status == DONE; }
  bool cont() const { return m_status == CONT; }
  bool yield() const { return m_status == YIELD; }
};

// ------------------------------------------------------------
//   Internal macros

/** Child function index
 *  Using the heap structure, this is really fast. */
#define achild(c) ((a_my_index << 1) + c)

/** Initialize
 */
#define ainit(c) { AdelAR * ar = myRuntime.stack[c]; if (ar) ar->line = 0; }

/** my(v)
 *
 *  Get a local variable from the activation record */
#define my(v) (a_me->v)

/** acall(res, c, f)
 *
 * Call an Adel function and capture the result status. */
#define acall(res, c, f)			\
  myRuntime.current = achild(c);		\
  res = f;

#ifdef ADEL_DEBUG
#define adel_debug(m, index, func, line)	\
  Serial.print(m);				\
  Serial.print(" in ");				\
  Serial.print(func);				\
  Serial.print("[");				\
  Serial.print(index);				\
  Serial.print("]:");				\
  Serial.println(line)
#else
#define adel_debug(m, index, func, line)  ;
#endif

// ------------------------------------------------------------
//   End-user macros

/** adel_setup(N)
 *
 * Set up the Adel runtime with a stack depth of N
 */
#define adel_setup(N) AdelRuntime<N> myRuntime;

/** aonce
 *
 *  Use astart in the Arduino loop function to initiate the Adel function f
 *  (and run all Adel functions below it).
 */
#define aonce( f )				\
  myRuntime.current = 0;			\
  f;

/** aforever
 *
 *  Run the top adel function over and over.
 */
#define aforever( f )				\
  myRuntime.current = 0;			\
  adel f_status = f;				\
  if (f_status.done()) { ainit(0); }

/** abegin
 *
 * Always add abegin and aend to every adel function. Any local variables
 * that need to persist in this function should be declared immediately
 * after abegin.
 */
#define abegin	struct LocalAdelAR : public AdelAR {

/** asteps:
 * 
 * Use asteps: to mark the starting point
 */
#define asteps								\
  };									\
  int a_my_index = myRuntime.current;					\
  LocalAdelAR * a_me = (LocalAdelAR *) myRuntime.stack[a_my_index];	\
  if (a_me == 0) {							\
    adel_debug("abegin", a_my_index, __FUNCTION__, __LINE__);		\
    a_me = (LocalAdelAR *) myRuntime.init_ar(a_my_index,sizeof(LocalAdelAR));	\
  }									\
  adel f_status, g_status;						\
  switch (my(line)) {							\
  case 0

/** aend
 *
 * Must be the last thing in the Adel function.
 */
#define aend								\
  case ADEL_FINALLY: ;							\
  }									\
  adel_debug("aend", a_my_index, __FUNCTION__, __LINE__);		\
  return adel::DONE;

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)							\
    my(line) = __LINE__;						\
    my(wait) = millis() + t;						\
 case __LINE__:								\
    if (millis() < my(wait)) return adel::CONT;

/** ado
 *
 *  Semantics: execute f synchronously, until it is done (returns false)
 *  Example use:
 *     ado( turn_on_light() );
 *     ado( turn_off_light() );
 */
#define ado( f )						\
    my(line) = __LINE__;					\
    ainit(achild(1));						\
    adel_debug("ado", a_my_index, __FUNCTION__, __LINE__);	\
  case __LINE__:						\
    acall(f_status, 1, f);					\
    if ( f_status.cont() ) return adel::CONT;

/** awaituntil
 *  Wait asynchronously for a condition to become true. Note that this
 *  condition CANNOT be an adel function.
 */
#define awaituntil( c )							\
    my(line) = __LINE__;						\
    adel_debug("awaituntil", a_my_index, __FUNCTION__, __LINE__);	\
  case __LINE__:							\
    if ( ! ( c ) ) return adel::CONT

/** aforatmost
 *
 *  Semantics: do f until it completes, or until the timeout. The structure
 *  is set up so that it can be used as a control structure to test whether
 *  or not the timeout was reached: any code placed after the aforatmost is
 *  executed only when the timeout interrupts the function.
 *
 *    aforatmost( 100, f ) {
 *        // -- Timed out -- do something
 *    }
 */
#define aforatmost( t, f )				\
    my(line) = __LINE__;				\
    ainit(achild(1));					\
    my(wait) = millis() + t;				\
    adel_debug("aforatmost", a_my_index, __FUNCTION__, __LINE__);	\
  case __LINE__:					\
    acall(f_status, 1, f);				\
    if (f_status.cont() && millis() < my(wait))		\
      return adel::CONT;				\
    if ( ! f_status.done())

/** atogether
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
 */
#define atogether( f , g )					\
    my(line) = __LINE__;					\
    ainit(achild(1));						\
    ainit(achild(2));						\
    adel_debug("aboth", a_my_index, __FUNCTION__, __LINE__);	\
  case __LINE__:						\
   acall(f_status, 1, f);					\
   acall(g_status, 2, g);					\
   if (f_status.cont() || g_status.cont())			\
      return adel::CONT;

/** auntil
 *
 *  Semantics: execute g until f completes.
 */
#define auntil( f , g )							\
    my(line) = __LINE__;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_debug("adountil", a_my_index, __FUNCTION__, __LINE__);		\
  case __LINE__:							\
    acall(f_status, 1, f);						\
    acall(g_status, 2, g);						\
    if (f_status.cont()) return adel::CONT;

/** auntileither
 *
 *  Semantics: execute c and f asynchronously until either one of them
 *  finishes (contrast with aboth). This construct behaves like a
 *  conditional statement: it should be followed by a true statement and
 *  optional false statement, which are executed depending on whether the
 *  first function finished first or the second one did. Example use:
 *
 *     auntileither( button(), flash_led() ) { 
 *        // button finished first 
 *     } else {
 *        // light finished first
 *     }
 */
#define auntileither( f , g )						\
    my(line) = __LINE__;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_debug("auntileither", a_my_index, __FUNCTION__, __LINE__);	\
  case __LINE__:							\
    acall(f_status, 1, f);						\
    acall(g_status, 2, g);						\
    if (f_status.cont() && g_status.cont())				\
      return adel::CONT;						\
    if (f_status.done())

/** aforevery
 * 
 *  Use in combination with ayield to form a traditional coroutine with a
 *  producer (function f) and a consumer (function g). Each time f yields,
 *  g is executed.
 *
 *   aforevery( button(pin2), brighten(pin3) );
 *
 */
#define aforevery( f , g )						\
    my(line) = __LINE__;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_debug("aforevery", a_my_index, __FUNCTION__, __LINE__);	\
    my(wait) = 0;							\
  case __LINE__:							\
  if (my(wait) == 0) {							\
    acall(f_status, 1, f);						\
    if (f_status.cont()) return adel::CONT;				\
    if (f_status.yield()) {						\
      my(wait) = 1;							\
      return adel::CONT;						\
    }									\
  } else { 								\ 
    acall(g_status, 2, g);						\
    if (g_status.cont()) return adel::CONT;				\
    if (g_status.yield()) {						\
      my(wait) = 0;							\
      return adel::CONT;						\
    }									\
    if (g_status.done()) {						\
      ainit(achild(2));							\
      my(wait) = 0;							\
      return adel::CONT;						\
    }									\
  }

/** ayield
 *
 * Yield to the calling function. Use in combination with 
 */
#define ayield							\
    my(line) = __LINE__;					\
    adel_debug("ayield", a_my_index, __FUNCTION__, __LINE__);	\
    return adel::YIELD;						\
  case __LINE__: ;

/** afinish
 * 
 *  Semantics: leave the function immediately, and communicate to the
 *  caller that it is done.
 */
#define afinish							\
    my(line) = ADEL_FINALLY;					\
    adel_debug("afinish", a_my_index, __FUNCTION__, __LINE__);	\
    return adel::CONT;

#endif


#include <stdint.h>

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
  uint16_t wait;
};

#define ADEL_FINALLY 0xFFFF

template<int stack_depth>
class AdelRuntime
{
public:
  // -- Stack: pointers to activation records indexed by "current"
  AdelAR * stack[1 << stack_depth];

  // -- Activation records: mirrors stack, but saves the pointers
  AdelAR * ars[1 << stack_depth];

  // -- Current function (index into stack)
  int current;

  // -- Helper method to initialize an activation record
  //    for the first time (note: uses malloc)
  AdelAR * init_ar(int index, int size_in_bytes)
  {
    AdelAR * ar = ars[index];
    if (ar == 0) {
      ar = (AdelAR *) malloc(size_in_bytes);
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
#define ainit(c) (myRuntime.stack[c]->line = 0)

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
#define abegin						\
  int a_my_index = myRuntime.current;			\
  struct LocalAdelAR : public AdelAR {			\

/** afirst:
 * 
 * Use afirst: to mark the first statement of the Adel function.
 */
#define afirst								\
  } * a_me = (LocalAdelAR *) myRuntime.stack[a_my_index];			\
  if (a_me == 0)							\
    a_me = (LocalAdelAR *) myRuntime.init_ar(a_my_index, sizeof(LocalAdelAR));	\
  adel f_status, g_status;						\
  switch (my(line)) {							\
  case 0

/** aend
 *
 * Must be the last thing in the Adel function.
 */
#define aend						\
  default: ;						\
  }							\
  my(line) = ADEL_FINALLY;				\
  return adel::DONE;

/** afinally
 *
 *  Optionally, end with afinally to do some action whenever the function
 *  returns (for any reason)
#define afinally( f )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
  case ADEL_FINALLY:					\
    myRuntime.current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return adel::CONT;
*/

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)					\
    my(line) = __LINE__;				\
    my(wait) = millis() + t;				\
  case __LINE__:					\
    if (millis() < my(wait)) return adel::CONT;

/** andthen
 *
 *  Semantics: execute f synchronously, until it is done (returns false)
 *  Example use:
 *     andthen( turn_on_light() );
 *     andthen( turn_off_light() );
 */
#define andthen( f )					\
    my(line) = __LINE__;				\
    ainit(achild(1));					\
  case __LINE__:					\
    acall(f_status, 1, f);				\
    if ( f_status.cont() ) return adel::CONT;

/** awaituntil
 *  Wait asynchronously for a condition to become true. Note that this
 *  condition CANNOT be an adel function.
 */
#define awaituntil( c )					\
    my(line) = __LINE__;				\
  case __LINE__:					\
    if ( ! ( c ) ) return adel::CONT

/** aforatmost
 *
 *  Semantics: do f until it completes, or until the timeout
 */
#define aforatmost( t, f )				\
    my(line) = __LINE__;				\
    my(wait) = millis() + t;				\
  case __LINE__:					\
    acall(f_status, 1, f);				\
    if (f_status.cont() && millis() < my(wait))		\
      return adel::CONT;

/** aboth
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
 */
#define aboth( f , g )					\
    my(line) = __LINE__;				\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: 					\
    acall(f_status, 1, f);				\
    acall(g_status, 2, g);				\
    if (f_status.cont() || g_status.cont())		\
      return adel::CONT;

/** adountil
 *
 *  Semantics: execute f until g completes.
 */
#define adountil( f , g )				\
    my(line) = __LINE__;				\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: 					\
    acall(f_status, 1, f);				\
    acall(g_status, 2, g);				\
    if (g_status.cont()) return adel::CONT;

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
#define auntileither( f , g )				\
    my(line) = __LINE__;				\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: 					\
    acall(f_status, 1, f);				\
    acall(g_status, 2, g);				\
    if (f_status.cont() && g_status.cont())		\
      return adel::CONT;				\
    if (f_status.done())

/** aforevery
 * 
 * Use in combination with ayield to form a traditional coroutine with a
 * producer (the called function f) and a consumer (the code following the
 * aforevery construct). Each time f yields, the code block is invoked.
 *
 *   aforevery( button(pin2) ) {
 *      toggle_led(pin9);
 *   }
 *
 */
#define aforevery( f )					\
    my(line) = __LINE__;				\
    ainit(achild(1));					\
    ainit(achild(2));					\
  case __LINE__: 					\
    acall(f_status, 1, f);				\
    if (f_status.cont()) return adel::CONT;		\
    if ( ! f_status.done()) my(line) = __LINE__;	\
    if ( f_status.yield() )

/** ayield
 *
 * Yield to the calling function. Use in combination with 
 */
#define ayield				\
    my(line) = __LINE__;		\
    return adel::YIELD;	 		\
  case __LINE__:

/** afinish
 * 
 *  Semantics: leave the function immediately, and communicate to the
 *  caller that it is done.
 */
#define afinish				   \
    my(line) = ADEL_FINALLY;		   \
    return adel::CONT;

#endif

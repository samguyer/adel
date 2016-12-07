#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

#ifndef ADEL_V3
#define ADEL_V3

/** Adel runtime
 *
 *  Stack of function states. Since Adel is emulating concurrency, the
 *  stack is not a linear data structure, but a tree of currently running
 *  functions. My implementation limits the tree to a binary tree, so
 *  that we can use a heap representation. This restriction means that any
 *  given function can only spawn two concurrent functions at any given
 *  time. 
*/

#define ADEL_FINALLY 0xFFFF

#ifndef ADEL_STACK_DEPTH
#define ADEL_STACK_DEPTH 5
#endif

/** Adel activation record
 *
 *  Minimum activation record includes the "program counter" for this
 *  function, and the next wait time (for adelay).
 */
class AdelAR
{
 public:
  uint32_t val;
  uint16_t pc;
  AdelAR() : val(0), pc(0) {}
};

/** LocalAdelAR
 *
 *  This class is the key to supporting local variables in a natural
 *  way. The activation record for each Adel function is initialized with a
 *  lambda that captures the local variables declared above it, creating
 *  the effect of local variables that persist as the function makes progress.
 *
 *  All the templating is necessary to get around the fact that C++11/14 do
 *  not allow you to declare the type of a lambda.
 */
template<typename T>
class LocalAdelAR : public AdelAR
{
public:
  T run;
  
  LocalAdelAR(T the_lambda) : run(the_lambda) {}
};

class AdelRuntime
{
public:

  // -- Pointer to the current runtime object
  static AdelRuntime * curStack;
  
  // -- Stack: pointers to activation records indexed by "current"
  AdelAR * stack[1 << ADEL_STACK_DEPTH];

  // -- Current function (index into stack)
  int current;

  // -- Constructor: set everything to null
  AdelRuntime() : current(0)
  {
    for (int i = 0; i < (1 << ADEL_STACK_DEPTH); i++) {
      stack[i] = 0;
    }
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
  typedef enum { ANONE, ADONE, ACONT, AYIELD } _status;
  
private:
  _status m_status;

public:
  adel(_status s) : m_status(s) {}
  adel() : m_status(ANONE) {}
  adel(const adel& other) : m_status(other.m_status) {}
  explicit adel(bool b) : m_status(ANONE) {}
  
  bool done() const { return m_status == ADONE; }
  bool cont() const { return m_status == ACONT; }
  bool yield() const { return m_status == AYIELD; }
  bool notdone() const { return m_status == ACONT || m_status == AYIELD; }
};

// ------------------------------------------------------------
//   Internal macros

/** Child function index
 *  Using the heap structure, this is really fast. */
#define achild(c) ((a_my_index << 1) + c)

/** Initialize
 *  Set the "program counter" to zero.
 */
#define ainit(c) {				 \
  AdelAR * ar = AdelRuntime::curStack->stack[c]; \
  if (ar) delete ar;				 \
  AdelRuntime::curStack->stack[c] = 0; }

/** Parent activation record
 */
#define acallerar AdelRuntime::curStack->stack[(a_my_index-1) >> 1]

/** acall(res, c, f)
 *
 * Call an Adel function f and capture the result status. */
#define acall(res, c, f)			\
  AdelRuntime::curStack->current = achild(c);	\
  res = f;

#ifdef ADEL_DEBUG
#define adel_debug(m, index, line)		\
  Serial.print(m);				\
  Serial.print(" in ");				\
  Serial.print(afun);				\
  Serial.print("[");				\
  Serial.print(index);				\
  Serial.print("]:");				\
  Serial.println(line)
#else
#define adel_debug(m, index, pc)  ;
#endif

/** gensym
 *
 *  These macros allow us to construct identifier names using line
 *  numbers. For some reason g++ requires two levels of macros to get it to
 *  work as expected.
 */
#define agensym2(a,b) a##b
#define agensym(a,b) agensym2(a,b)

/** anextstep
 * 
 *  This macro encapsulates the representation of a program counter in
 *  Adel. Based on the __LINE__ directive. Multiplying by 10 gives us room
 *  to have several cases within a single macro by adding an offset.
 */

#define anextstep (__LINE__*10)
#define alaterstep(offset) (__LINE__*10 + offset)

// ------------------------------------------------------------
//   Top-level functions for use in Arduino loop()

/** aonce
 *
 *  Use astart in the Arduino loop function to initiate the Adel function f
 *  (and run all Adel functions below it).
 */
#define aonce( f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  AdelRuntime::curStack->current = 0;					\
  f;

/** aforever
 *
 *  Run the top adel function over and over.
 */
#define arepeat( f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  AdelRuntime::curStack->current = 0;					\
  adel agensym(f_status, __LINE__) = f;					\
  if (agensym(f_status, __LINE__).done()) { ainit(0); }

/** aevery
 *  
 *  Run this function every T milliseconds.
 */
#define aevery( T, f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  static uint32_t agensym(anexttime,__LINE__) = millis() + T;		\
  AdelRuntime::curStack->current = 0;					\
  adel agensym(f_status, __LINE__) = f;					\
  if (agensym(f_status, __LINE__).done() && agensym(anexttime,__LINE__) < millis()) {	\
    ainit(0); }

// ------------------------------------------------------------
//   Function prologue and epilogue

/** abegin
 *
 * Always add abegin and aend to every adel function. Note that the lambda
 * is defined in such a way that all local variables above it are captured
 * and later copied into the LocalAdelAR. 
 */
#define abegin								\
  static const char * afun = __FUNCTION__;				\
  uint32_t adel_ramp_start;						\
  uint32_t adel_wait;							\
  uint8_t  adel_condition;						\
  auto adel_body = [=](uint16_t& adel_pc, int a_my_index) mutable {	\
    adel f_status, g_status;						\
    bool a_skipahead = false;						\
    switch (adel_pc) {							\
   case 0

/** aend
 *
 * Must be the last thing in the Adel function. This macro first wraps up
 * the big switch statement and closes the lambda. Then it drops back into
 * the code that it executed every time through the function. First, check
 * to see if the activation record needs to be initialized, then invoke the
 * lambda.
 */
#define aend								\
        case ADEL_FINALLY: ;						\
      }									\
      adel_debug("aend", a_my_index, __LINE__);				\
      adel_pc = ADEL_FINALLY;						\
      return adel::ADONE;						\
    };									\
  int a_my_index = AdelRuntime::curStack->current;			\
  AdelAR * a_ar = AdelRuntime::curStack->stack[a_my_index];		\
  LocalAdelAR<decltype(adel_body)> * a_this_ar = 0;			\
  if (a_ar == 0) {							\
    a_this_ar = new LocalAdelAR<decltype(adel_body)>(adel_body);	\
    AdelRuntime::curStack->stack[a_my_index] = a_this_ar;		\
  } else								\
    a_this_ar = (LocalAdelAR<decltype(adel_body)> *) a_ar;		\
  if (a_this_ar->pc == 0) { adel_debug("abegin", a_my_index, __LINE__);	} \
  adel result = a_this_ar->run(a_this_ar->pc, a_my_index);		\
  return result;

// ------------------------------------------------------------
//   General Adel functions

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)							\
    adel_pc = anextstep;						\
    adel_wait = millis() + t;						\
    adel_debug("adelay", a_my_index, __LINE__);				\
 case anextstep:							\
    if (millis() < adel_wait) return adel::ACONT;

/** andthen
 *
 *  Semantics: execute f synchronously, until it is done (returns DONE)
 *  Example use:
 *     andthen( turn_on_light() );
 *     andthen( turn_off_light() );
 */
#define andthen( f )							\
    adel_pc = anextstep;						\
    ainit(achild(1));							\
    adel_debug("andthen", a_my_index, __LINE__);			\
  case anextstep:							\
    acall(f_status, 1, f);						\
    if ( f_status.notdone() ) return adel::ACONT

/** await
 *  Wait asynchronously for a condition to become true. Note that this
 *  condition CANNOT be an adel function.
 */
#define await( c )							\
    adel_pc = anextstep;						\
    adel_debug("await", a_my_index, __LINE__);				\
  case anextstep:							\
    if ( ! ( c ) ) return adel::ACONT

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
#define aforatmost( t, f )						\
    adel_pc = anextstep;						\
    ainit(achild(1));							\
    adel_wait = millis() + t;						\
    adel_debug("aforatmost", a_my_index, __LINE__);			\
  case anextstep:							\
    acall(f_status, 1, f);						\
    if (f_status.notdone() && millis() < adel_wait) return adel::ACONT;	\
    adel_condition = f_status.done();					\
    adel_pc = alaterstep(1);						\
  case alaterstep(1):							\
    if ( ! adel_condition)
    
/** atogether
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
 */
#define atogether( f , g )						\
    adel_pc = anextstep;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_debug("atogether", a_my_index, __LINE__);			\
  case anextstep:							\
    acall(f_status, 1, f);						\
    acall(g_status, 2, g);						\
    if (f_status.notdone() || g_status.notdone()) return adel::ACONT;

/** OLD auntil
 *
 *  Semantics: execute g until f completes.
 */
/*
#define auntil( f , g )							\
    adel_pc = anextstep;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_debug("auntil", a_my_index, __LINE__);		\
  case anextstep:								\
    acall(f_status, 1, f);						\
    acall(g_status, 2, g);						\
    if (f_status.notdone()) return adel::ACONT;
*/

/** auntil
 *
 *  Semantics: execute f and g until either one of them finishes (contrast
 *  with atogether). This construct behaves like a conditional statement:
 *  it can be followed by a true statement and optional false statement,
 *  which are executed depending on whether the first function finished
 *  first or the second one did. Example use:
 *
 *     auntil( button(), flash_led() ) { 
 *        // button finished first 
 *     } else {
 *        // light finished first
 *     }
 */
#define auntil( f , g )							\
    adel_pc = anextstep;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_debug("auntil", a_my_index, __LINE__);				\
  case anextstep:							\
    acall(f_status, 1, f);						\
    acall(g_status, 2, g);						\
    if (f_status.notdone() && g_status.notdone()) return adel::ACONT;	\
    adel_condition = f_status.done();					\
    adel_pc = alaterstep(1);						\
  case alaterstep(1):							\
    if (adel_condition)

/** ramp
 *
 *  Execute execute the body for T milliseconds; each time it is executed,
 *  v will be set to a value between min and max proportional to the
 *  fraction of T that has elapsed. Useful for turning something on or off
 *  over a specific period of time. Used as a control structure:
 *
 *       aramp(1000, v, 0, 255) {
 *         analogWrite(pin, v);
 *         adelay(50);
 *       }
 *
 *  NOTE: Make sure there is some adelay or other async function inside
 *        the loop body.
 */
#define aramp( T, v, start, end)					\
    adel_pc = anextstep;						\
    adel_ramp_start = millis();						\ 
    adel_debug("aramp", a_my_index, __LINE__);				\
 case anextstep:							\
    while ((millis() <= (adel_ramp_start + T)) &&			\
           ((v = map(millis(), adel_ramp_start, adel_ramp_start + T, start, end)) == v) && \
           (adel_pc = anextstep))

/** alternate
 * 
 *  Alternate between two functions. Execute the first function until it
 *  calls "ayourturn". Then start executing the second function until *it*
 *  calls "ayourturn", at which point continue executing the first one
 *  where it left off. Continue until either one finishes.
 */
#define alternate( f , g )						\
    adel_pc = anextstep;						\
    ainit(achild(1));							\
    ainit(achild(2));							\
    adel_condition = true;						\
    adel_debug("alternate", a_my_index, __LINE__);			\
  case anextstep:							\
    if (adel_condition) {						\
      acall(f_status, 1, f);						\
      if (f_status.cont()) return adel::ACONT;				\
      if (f_status.yield()) {						\
	adel_condition = false;						\
        return adel::ACONT;						\
      }									\
    } else {								\
      acall(g_status, 2, g);						\
      if (g_status.cont()) return adel::ACONT;				\
      if (g_status.yield()) {						\
        adel_condition = true;						\
        return adel::ACONT;						\
      }									\
    }

/** ayourturn
 *
 *  Use only in functions being called by "alternate". Stop executing this
 *  function and start executing the other function.
 */
#define ayourturn(v)							\
    adel_pc = anextstep;						\
    acallerar->val = v;							\
    adel_debug("ayourturn", a_my_index, __LINE__);			\
    return adel::AYIELD;						\
  case anextstep: ;

/** amyturn
 *
 *  Get the value passed to this function by ayourturn
 */
#define amyturn acallerar->val

/** afinish
 * 
 *  Semantics: leave the function immediately, and communicate to the
 *  caller that it is done.
 */
#define afinish							\
    adel_pc = ADEL_FINALLY;					\
    adel_debug("afinish", a_my_index, __LINE__);		\
    return adel::ACONT;

#endif

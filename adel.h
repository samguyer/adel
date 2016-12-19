#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

#ifndef ADEL_V4
#define ADEL_V4

/** Adel runtime
 *
 *  Stack of function states. Since Adel is emulating concurrency, the
 *  stack is not a linear data structure, but a tree of actively running
 *  functions. 
*/

#define ADEL_FINALLY 0xFFFF

/** adel status
 * 
 *  All Adel functions return an enum that indicates whether the routine is
 *  done or has more work to do. The various concurrency constructs use
 *  this result to decide whether to continue executing the function, or
 *  move on to the next step.
 */
class astatus
{
public:
  typedef enum { ANONE, ADONE, ACONT, AYIELD } _status;
  
private:
  _status m_status;

public:
  astatus(_status s) : m_status(s) {}
  astatus() : m_status(ANONE) {}
  astatus(const astatus& other) : m_status(other.m_status) {}
  
  bool done() const { return m_status == ADONE; }
  bool cont() const { return m_status == ACONT; }
  bool yield() const { return m_status == AYIELD; }
  bool notdone() const { return m_status == ACONT || m_status == AYIELD; }
};

/** Adel activation record
 *
 *  The base class for all activation records. All other information --
 *  including the code to run -- is bound up in the closure stored in each
 *  subclass.
 */
class AdelAR
{
private:
  AdelAR * children[3];
  uint32_t val;

public:
  AdelAR()
    : val(0)
  {
    children[0] = 0;
    children[1] = 0;
    children[2] = 0;
  }

  inline void clear(int i) {
    if (children[i]) {
      delete children[i];
      children[i] = 0;
    }
  }
  
  inline void init(int i, AdelAR * ar) {
    clear(i);
    children[i] = ar;
  }

  virtual astatus run() = 0;

  inline astatus runchild(int i) const { return children[i]->run(); }

  ~AdelAR() {
    clear(0);
    clear(1);
    clear(2);
  }
};

/** LocalAdelAR
 *
 *  This class is the key to supporting local variables in a natural
 *  way. The activation record for each Adel function is initialized with a
 *  lambda that captures the local variables declared above it, including
 *  the arguments to the function, creating the effect of local variables
 *  that persist as the function makes progress.
 *
 *  The templating is necessary to get around the fact that C++11/14 do not
 *  allow you to declare the type of a lambda.
 */
template<typename T>
class LocalAdelAR : public AdelAR
{
public:
  T body;
  
 LocalAdelAR(const T& the_lambda)
   : AdelAR(),
     body(the_lambda)
  {}

  virtual astatus run() {
    return body(this);
  }
};

/** Runtime stack
 *
 * This class encapsulates a single control stack. The top-level macros,
 * such as aonce and arepeat, each create a separate instance of this class
 * to hold their activation records. Activation records are structured as a
 * tree. Each pass over the currently active functions starts with a call
 * to run() on the root.
 */
class AdelRuntime
{
public:

  // -- Pointer to the current runtime object
  static AdelRuntime * curStack;

private:
  AdelAR * root;

public:
  AdelRuntime()
    : root(0)
  {}

  inline bool not_running() const { return root == 0; }
  
  inline void init(AdelAR * ar) { root = ar; }

  inline astatus run() { return root->run(); }

  inline void reset() {
    if (root) {
      delete root;
      root = 0;
    }
  }
};

// ------------------------------------------------------------
//   Internal macros

#ifdef ADEL_DEBUG
#define adel_debug(m, line)			\
  Serial.print(m);				\
  Serial.print(" in ");				\
  Serial.print(a_fun_name);			\
  Serial.print(":");				\
  Serial.println(line)
#else
#define adel_debug(m, line)  ;
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
 *  Run an adel function from the top one time. Once complete, .
 */
#define aonce( f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  if (AdelRuntime::curStack->not_running())				\
     AdelRuntime::curStack->init( f );					\
  AdelRuntime::curStack->run();

/** aforever
 *
 *  Run the top adel function over and over.
 */
#define arepeat( f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  if ( AdelRuntime::curStack->not_running())				\
    AdelRuntime::curStack->init( f );					\
  astatus agensym(f_status, __LINE__) = AdelRuntime::curStack->run();	\
  if (agensym(f_status, __LINE__).done()) {				\
    AdelRuntime::curStack->reset();					\
  }

/** aevery
 *  
 *  Run this function every T milliseconds.
 */
#define aevery( T, f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  static uint32_t agensym(anexttime,__LINE__) = millis() + T;		\
  if ( AdelRuntime::curStack->not_running())				\
    AdelRuntime::curStack->init( f );					\
  astatus agensym(f_status, __LINE__) = AdelRuntime::curStack->run();	\
  if (agensym(f_status, __LINE__).done() &&				\
      agensym(anexttime,__LINE__) < millis()) {				\
    AdelRuntime::curStack->reset();					\
  }

// ------------------------------------------------------------
//   Function prologue and epilogue

/** adel 
 *
 * Every Adel function must be declared with "adel" as the return type.
 * Here is the general form of an Adel function:
 *
 *     adel blinkylights(int pin)
 *     {
 *       abegin:
 *       ...
 *       aend;
 *     }
 *
 *  The definition of this macro ensures that users don't forget to
 *  call Adel functions inside a concurrency primitive, even if it
 *  is just "andthen".
 */
#define adel AdelAR * __attribute__((warn_unused_result)) 

/** abegin
 *
 * Always add abegin and aend to every adel function. Note that the lambda
 * is defined in such a way that all local variables above it are captured
 * and later copied into the LocalAdelAR. 
 */
#define abegin								\
  const char * a_fun_name = __FUNCTION__;				\
  /* -- These variables become the persistent state in the closure */	\
  uint16_t adel_pc = 0;							\
  uint32_t adel_ramp_start = 0;						\
  uint32_t adel_wait = 0;						\
  uint8_t  adel_condition = 0;						\
  /* -- Start the lambda -- the body of the function */			\
  auto adel_body = [=](AdelAR * a_ar) mutable {				\
    astatus f_status, g_status;						\
    bool a_skipahead = false;						\
    if (adel_pc == 0) { adel_debug("abegin", __LINE__);}		\
    switch (adel_pc) {							\
   case 0

/** aend
 *
 */
#define aend								\
        case ADEL_FINALLY: ;						\
      }									\
      adel_debug("aend", __LINE__);					\
      adel_pc = ADEL_FINALLY;						\
      return astatus::ADONE;						\
    };									\
  LocalAdelAR<decltype(adel_body)> * a_this_ar = 0;			\
  a_this_ar = new LocalAdelAR<decltype(adel_body)>(adel_body);		\
  Serial.print(a_fun_name); \
  Serial.print(" @ "); \
  Serial.println((int) a_this_ar, HEX); \
  return a_this_ar;

// ------------------------------------------------------------
//   General Adel functions

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)							\
    adel_pc = anextstep;						\
    adel_wait = millis() + t;						\
    adel_debug("adelay", __LINE__);					\
 case anextstep:							\
    if (millis() < adel_wait) return astatus::ACONT;

/** andthen
 *
 *  Semantics: execute f synchronously, until it is done (returns DONE)
 *  Example use:
 *     andthen( turn_on_light() );
 *     andthen( turn_off_light() );
 */
#define andthen( f )							\
    adel_pc = anextstep;						\
    a_ar->init(0, f );							\
    adel_debug("andthen", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    if ( f_status.notdone() ) return astatus::ACONT;			\
    a_ar->clear(0);

/** await
 *  Wait asynchronously for a condition to become true. Note that this
 *  condition CANNOT be an adel function.
 */
#define await( c )							\
    adel_pc = anextstep;						\
    adel_debug("await", __LINE__);					\
  case anextstep:							\
    if ( ! ( c ) ) return astatus::ACONT

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
    a_ar->init(0, f );							\
    adel_wait = millis() + t;						\
    adel_debug("aforatmost", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    if (f_status.notdone() && millis() < adel_wait)			\
      return astatus::ACONT;						\
    a_ar->clear(0);							\
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
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    adel_debug("atogether", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    g_status = a_ar->runchild(1);					\
    if (f_status.notdone() || g_status.notdone())			\
      return astatus::ACONT;						\
    a_ar->clear(0);							\
    a_ar->clear(1);

/** a4together
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
#define a4together( e, f , g , h, )					\
    adel_pc = anextstep;						\
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    adel_debug("atogether", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    g_status = a_ar->runchild(1);					\
    if (f_status.notdone() || g_status.notdone())			\
      return astatus::ACONT;
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
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    adel_debug("auntil", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    g_status = a_ar->runchild(1);					\
    if (f_status.notdone() && g_status.notdone())			\
      return astatus::ACONT;						\
    a_ar->clear(0);							\
    a_ar->clear(1);							\
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
    adel_debug("aramp", __LINE__);					\
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
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    adel_condition = true;						\
    adel_debug("alternate", __LINE__);					\
  case anextstep:							\
    if (adel_condition) {						\
      f_status = a_ar->runchild(0);					\
      if (f_status.cont()) return astatus::ACONT;			\
      if (f_status.yield()) {						\
	adel_condition = false;						\
        return astatus::ACONT;						\
      }									\
    } else {								\
      g_status = a_ar->runchild(1);					\
      if (g_status.cont()) return astatus::ACONT;			\
      if (g_status.yield()) {						\
        adel_condition = true;						\
        return astatus::ACONT;						\
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
    adel_debug("ayourturn", __LINE__);					\
    return astatus::AYIELD;						\
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
    adel_debug("afinish", __LINE__);				\
    return astatus::ACONT;

#endif

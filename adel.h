#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

#ifndef ADEL_V4
#define ADEL_V4

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
  // -- Each Adel function can have up three callees running simulatenously
  //    (see athree, for example)
  AdelAR * children[3];

public:
  AdelAR() {
    children[0] = 0;
    children[1] = 0;
    children[2] = 0;
  }

  // -- Clear a particular child function, deleting its activation record
  inline void clear(int i) {
    AdelAR * ch = children[i];
    if (ch) {
      children[i] = 0;
      delete ch;
    }
  }

  // -- Initialize a child function. Typically, the "ar" argument is the
  //    result of calling a user's function, which creates the new AR with
  //    the lambda inside it.
  inline void init(int i, AdelAR * ar) {
    clear(i);
    children[i] = ar;
  }

  // -- Run the adel function one time. Each local AR overrides this
  //    function to invoke its lambda.
  virtual astatus run() = 0;

  // -- Most of the time, the parent AR calls run
  inline astatus runchild(int i) const { return children[i]->run(); }

  // -- Delete this AR, and the ARs of all of its children functions
  virtual ~AdelAR() {
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

  // -- Invoke the lambda, passing its own AR pointer, so it can create and
  //    attach ARs for children functions.
  virtual astatus run() { return body(this); }
};

/** Runtime stack
 *
 * This class encapsulates a single control stack. Activation records are
 * structured as a tree, since multiple functions can be active at the same
 * time. Each pass over the currently active functions starts with a call
 * to run() on the root. The top-level loop macros, such as aonce and
 * arepeat, each create a separate instance of this class to hold their
 * activation records.
 */
class AdelRuntime
{
public:

  // -- Global pointer to the current stack
  static AdelRuntime * curStack;

private:
  // -- Root of this tree of activation records
  AdelAR * root;

public:
  AdelRuntime()
    : root(0)
  {}

  // -- A null root signals that the function is not running
  inline bool not_running() const { return root == 0; }

  // -- Initialize a new run
  inline void init(AdelAR * ar) { root = ar; }

  // -- Run a single pass over the tree. This function is executed many,
  //    many times as the functions make progress.
  inline astatus run() { return root->run(); }

  // -- Reset the run, deleting all activation records
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
//
//   You can put as many of these as you'd like in your loop function

/** aforever
 *
 *  Run the given Adel function over and over.
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
 *  Run the given Adel function every T milliseconds.
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
    agensym(anexttime,__LINE__) += T;					\
  }

/** aonce
 *
 *  Run an adel function from the top one time. Once complete, the program
 *  stops running and nothing will happen on the microcontroller until it
 *  is restarted. Probably not what you want!
 */
#define aonce( f )							\
  static AdelRuntime agensym(aruntime, __LINE__);			\
  AdelRuntime::curStack = & agensym(aruntime, __LINE__);		\
  if (AdelRuntime::curStack->not_running())				\
     AdelRuntime::curStack->init( f );					\
  AdelRuntime::curStack->run();

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
 * Always add abegin and aend to every adel function. These macros wrap the
 * body of the function in a lambda, which is used as a continuation during
 * concurrent execution. In this sense, the user's function just serves as
 * an initialization function to set up the continuation. Note that the
 * lambda is defined in such a way that all local variables above it are
 * captured and later copied into the LocalAdelAR.
 */
#define abegin								\
  const char * a_fun_name = __FUNCTION__;				\
  /* -- These variables become the persistent state in the closure */	\
  uint16_t adel_pc = 0;							\
  uint32_t adel_wait = 0;						\
  uint32_t adel_ramp_start = 0;						\
  /* ----- Start the lambda -- the body of the function ----- */	\
  auto adel_body = [=](AdelAR * a_ar) mutable {				\
    astatus f_status, g_status, h_status;				\
    if (adel_pc == 0) { adel_debug("abegin", __LINE__);}		\
    switch (adel_pc) {							\
   case 0

/** aend
 *
 *  Create and return a local activation record with the new lambda
 *  embedded in it.
 */
#define aend								\
        case ADEL_FINALLY: ;						\
      }									\
      adel_debug("aend", __LINE__);					\
      adel_pc = ADEL_FINALLY;						\
      return astatus::ADONE;						\
    };									\
  /* -- Make and return the new AR */					\
  return new LocalAdelAR<decltype(adel_body)>(adel_body);

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
 *
 *  The way this is implemented in adel is sneaky -- we use the adel PC as
 *  a way to remember whether the function finished or not.
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
    if (f_status.done()) adel_pc = alaterstep(1);			\
    else                 adel_pc = alaterstep(2);			\
  case alaterstep(1):							\
  case alaterstep(2):							\
  if ( adel_pc != alaterstep(1) )
    
/** aboth
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      aboth( flash_led(), play_sound() );
 */
#define aboth( f , g )							\
    adel_pc = anextstep;						\
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    adel_debug("aboth", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    g_status = a_ar->runchild(1);					\
    if (f_status.notdone() || g_status.notdone())			\
      return astatus::ACONT;						\
    a_ar->clear(0);							\
    a_ar->clear(1);

/** athree
 *
 *  Semantics: execute f, g, and h asynchronously, until *all* are done.
 */
#define athree( f , g , h )						\
    adel_pc = anextstep;						\
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    a_ar->init(2, h );							\
    adel_debug("athree", __LINE__);					\
  case anextstep:							\
    f_status = a_ar->runchild(0);					\
    g_status = a_ar->runchild(1);					\
    h_status = a_ar->runchild(2);					\
    if (f_status.notdone() || g_status.notdone() || h_status.notdone())	\
      return astatus::ACONT;

/** auntil
 *
 *  Semantics: execute f and g until either one of them finishes (contrast
 *  with aboth).  This construct behaves like a conditional statement:
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
    if (f_status.done()) adel_pc = alaterstep(1);			\
    else                 adel_pc = alaterstep(2);			\
  case alaterstep(1):							\
  case alaterstep(2):							\
  if ( adel_pc == alaterstep(1) )

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
    adel_pc = alaterstep(0);						\
    a_ar->init(0, f );							\
    a_ar->init(1, g );							\
    adel_debug("alternate", __LINE__);					\
  case alaterstep(0):							\
    f_status = a_ar->runchild(0);					\
    if (f_status.cont()) return astatus::ACONT;				\
    if (f_status.yield()) {						\
	adel_pc = alaterstep(1);					\
        return astatus::ACONT;						\
    } else								\
        adel_pc = alaterstep(2);					\
  case alaterstep(1):							\
    g_status = a_ar->runchild(1);					\
    if (g_status.cont()) return astatus::ACONT;				\
    if (g_status.yield()) {						\
	adel_pc = alaterstep(0);					\
        return astatus::ACONT;						\
    }									\
  case alaterstep(2):

/** ayourturn
 *
 *  Use only in functions being called by "alternate". Stop executing this
 *  function and start executing the other function.
 */
#define ayourturn							\
    adel_pc = anextstep;						\
    adel_debug("ayourturn", __LINE__);					\
    return astatus::AYIELD;						\
  case anextstep: ;

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

# Adel
**A new way to program microcontrollers**

**NOTE:** This code is under active development, so the API could still change. Also, please see usage warnings at the end of this document. 

Adel is a library that makes it easier to program microcontrollers, such as the Arduino. The main idea is to provide a simple kind of concurrency, similar to coroutines. Using Adel, any function can be made to behave in an asynchronous way, allowing it to run concurrently with other Adel functions without interfering with them. The library is implemented entirely as a set of C/C++ macros, so it requires no new compiler tools or flags. Just download the Adel directory and install it in your Arduino IDE libraries folder.

## Overview and examples

Adel came out of my frustration with microcontroller programming. In particular, that seemingly simple behavior can be very hard to implement. As an example, consider a function that blinks an LED attached to some pin every N milliseconds:

```{c++}
void blink(int some_pin, int N) {
   digitalWrite(some_pin, HIGH);
   delay(N);
   digitalWrite(some_pin, LOW);
   delay(N);
}
```

OK, that's easy enough. I can call it with two different values, say 500ms and 300ms:

```{c++}
for (int i = 0; i < 100; i++) blink(3, 500);
for (int i = 0; i < 100; i++) blink(4, 300);
```

But what if I want to blink them **at the same time**? Now, suddenly, I have lost composability. This code doesn't work:

```{c++}
for (int i = 0; i < 100; i++) {
  blink(3, 500);
  blink(4, 300);
}
```

To get **concurrent** behavior I have to write completely different code, which makes the scheduling explicit:

```{c++}
int last300, last500;
loop() {
  uint32_t t = millis();
  if (t - last300 > 300) {
    if (pin3_on) digitalWrite(3, LOW);
    else digitalWrite(3, HIGH);
    last300 = t;
  }
  if (t - last500 > 500) {
    ... etc ...
 }
 ```

Aside from the obvious complexity of this code, there are some specific problems. First, all behaviors that might occur concurrently must be part of the same loop with the same timing control. The modularity is completely gone. Second, we need to introduce a global variable for each behavior that remembers the last time it executed. The code would become significantly more complex if we wanted to blink the lights for a specific amount of time, or if we had other modes where the lights are not blinking. Similar problems arise with input as well. Imagine if we want to blink a light until a button is pressed (inluding debouncing the button signal). 

## The Adel API

The central problem is the `delay()` function, which makes timing easy for individual behaviors, but blocks the whole processor. The key feature of Adel, therefore, is an asynchronous delay function called `adelay` (hence the name Adel). The `adelay` function works just like `delay`, but allows other code to run concurrently. 

Concurrency in Adel is specified at the function granularity, using a fork-join style of parallelism. Functions are designated as "Adel functions" by defining them in a stylized way. The body of the function can use any of the Adel library routines shown below:

* `adelay( T )` : asynchronously delay the current function for T milliseconds.
* `andthen( f )` : run Adel function `f` to completion before continuing (synchronous execution).
* `await( c )` : wait asynchronously until condition `c` is true (`c` must *not* be an Adel function).
* `aforatmost( T, f )` : run Adel function `f` until it completes, or T milliseconds (whichever comes first)
* `aboth( f , g )` : run Adel functions `f` and `g` concurrently until they **both** finish.
* `auntil( f , g ) { ... } else { ... }` : run Adel functions `f` and `g` concurrently until **one** of them finishes. Executes the true branch if `f` finishes first or the false branch if `g` finishes first.
* `aramp( T, v, min, max) { ... }` : execute the body for T milliseconds; each time it is executed, v will be set to a value between min and max proportional to the fraction of T that has elapsed. Useful for turning something on or off over a specific period of time.
* `afinish` : finish executing the current function (like a return)
* `alternate( f , g )` : run `f` continuously until it yields by calling `ayourturn`; then run `g` until it yields. Continue back and forth until either function completes.
* `ayourturn` : use in a function being called by `alternate` to yield control to the other function (like "yield" in conventional coroutines).

Using these routines we can rewrite the blink routine (below). Every Adel function contains a minimum of three things: return type `adel`, and macros `abegin:` and `aend` at the begining and end of the function. (**NOTE** that ``abegin`` is always followed by a colon). But otherwise, the code is almost identical.

```{c++}
adel blink(int some_pin, int N) 
{
  abegin:
  while (1) {
    digitalWrite(some_pin, HIGH);
    adelay(N);
    digitalWrite(some_pin, LOW);
    adelay(N);
  }
  aend;
}
```

Notice that I added a `while (1)` infinite loop -- this function will blink the light forever, or until it is stopped by its caller. **And that's ok** because it will not stop other code from running. As a result, we can execute blink concurrently, like this:

```{c++}
aboth( blink(3, 500), blink(4, 500) );
```

This code does exactly what we want: it blinks the two lights at different intervals at the same time. Notice also that we have two instances of blink doing slightly different things concurrently -- this kind of modularity and reuse is much harder to achieve in the traditional coding style.

The `aboth` macro waits until both functions are complete, which is not always desirable. For example, we might want to blink a light until a button is pressed. Assuming we have a `button` function (shown later), we can use the `auntil` construct:

```{c++}
auntil( button(pin), blink(3, 350) );
```

The semantics are simple: when the `button` routine completes, `auntil` simply stops calling the `blink` routine, in effect interrupting it at the last point it yielded. (In the current implementation, the interrupted function has no opportunity to respond to this interruption or clean up in any way.)

The same construct could be use to implement a timeout by defining a function that simply delays for a specified amount of time:

```{c++}
adel timeout(int ms) {
  abegin:
  adelay(ms);
  aend;
}
```

To blink for 2 seconds we write:

```{c++}
auntil( timeout(2000), blink(3, 350) );
```

Timeouts are so common, though, that Adel supports this construct directly, without having to define a separate function:

```{c++}
aforatmost( 2000, blink(3, 350) );
```

One special feature of `aforatmost` is that it behaves like a conditional, where the true branch is executed only when the timeout occurs first, and the optional false branch is executed if the function finishes before the timeout:

```{c++}
aforatmost( 2000, blink(3, 350) ) {
    // -- Timeout happened before blinking finished
}
```

The `auntil` construct can be used in the same way to detect which of the two functions finished first. The true branch is executed if the first one finishes first; the false branch is executed if the second one finishes first:

```{c++}
auntil( button(pin), blink(3, 350) ) {
    // -- User hit the button
} else {
    // -- blink completed
}
```

Of course, we can still force functions to occur synchronously, but now we need to say that explicitly. The `andthen` macro executes a single Adel function to completion before moving to the next one. For example, here is how we could program a button that turns a light on and off:

```{c++}
adel toggle(int buttonpin, int ledpin)
{
   abegin:
   while (1) {
      andthen( button(buttonpin) );
      digitalWrite( ledpin, HIGH );
      andthen( button(buttonpin) );
      digitalWrite( ledpin, LOW );
   }
   aend;
}
```

Here is the `button()` function, which returns when the user presses the button. It uses the `await` construct to wait for the pin to go high or low. Notice that we enclose the whole routine in an infinite `while (1)` loop, so that the routine *only* finishes when the button is pushed. 

```{c++}
adel button(int pin)
{
   abegin:
   while (1) {
      await (digitalRead(pin) == HIGH);
      adelay (50);
      if (digitalRead(pin) == HIGH) {
         await (digitalRead(pin) == LOW);
         afinish;
      }
   }
   aend;
}
```

## Local variables

One of the challenges in Adel is supporting local variables. From the standpoint of the underlying C runtime, control enters and exits each function many times before it finishes. Each time it exits, any local variables disappear and lose their values. The latest version of Adel uses C++ lambdas to capture local variables, making them behave as you would expect in a regular function. For example, here is a loop that includes an asynchronous delay:

```{c++}
adel counter()
{
  int i;

  abegin:
    for (i = 0; i < 100; i++) {
      digitalWrite(pin, i);
      adelay(100);
    }
  aend;
}
```

The lambda definition inside the `abegin` macro captures the variable `i`, so that its state persists and the loop works as expected. Some important things to note:

First, only variables above the `abegin` are persistent across Adel calls (such as `adelay`). You can introduce other local variables in the body of the function, but only as short-lived temporaries.

Second, initializers will probablty not work as expected, so make sure you initialize your variables in the code following `abegin`.

Finally, do not put any other code above the `abegin` -- it will be executed at unexpected times.

## Your turn, my turn

Classic coroutines allow a function to yield to its caller **without** losing track of where it is currently executing. Subsequent entry to the function continues where it left off. The problem with this approach is that it requires an explicit "init" to start the function, followed by repeated invocations ("next") until it is done. 

Instead, Adel limits this behavior to single `alternate` construct, which takes two functions and alternates executing then until either one finishes. A function chooses when to let the other function execute by calling `ayourturn`, which is like "yield". In this example the button routine is augmented with a yield when the button is held down. We can use this version to make an LED get brighter and brighter until the button is released. We use a global variable `delta_t` to communicate the amount of time the button has been held. Here is the augmented button routine:

```{c++}
uint32_t delta_t;
adel button(int pin)
{
  uint32_t starttime;
  abegin:
    while (1) {
      await (digitalRead(pin) == HIGH);
      adelay (50);
      if (digitalRead(pin) == HIGH) {
         starttime = millis();
         while (digitalRead(pin) != LOW) {
            delta_t = millis() - starttime;
            ayourturn;
         }
         afinish;
      }
    }
    
  aend;
}
```

Here is a simple function that receives these values and sets the LED brightness accordingly. The `map` function is provided by the Arduino standard library; it just maps an integer value from one range to another range. In this case, we map the time (in milliseconds, from 0 to 10 seconds) to the brightness, which ranges from 0 to 255.

```{c++}
adel brighten(int pin)
{
  int level;
  abegin:
    level = 0;
    while (1) {
      analogWrite(pin, level);
      ayourturn;
      level = map(delta_t, 0, 10000, 0, 256);
    }
    
  aend;
}
```

Finally, we can connect these two in the caller to get the aggregate behavior:

```{c++}
alternate( button(2) , brighten(11) );
```

## Top-level loop

Since the top-level loop function in an Arduino program is not an Adel function, we need some machinery to get the whole execution process started. The simplest construct is `arepeat`, which executes the whole Adel program over and over. For example, if your program creates an elaborate light pattern, `arepeat` will keep playing the pattern repeatedly.

```{c++}
void loop()
{ 
  arepeat( mylightshow() );
}
```

One nice thing about the top-level loop is that you can put as many of these "processes" as you want, and they all run concurrently:

```{c++}
void loop()
{ 
  arepeat( mylightshow() );
  arepeat( mysoundshow() );
}
```

If you really only want the program to run one time, you can write `aonce`, but once the program completes the device will not do anything else until it is restarted.

```{c++}
void loop()
{ 
  aonce( mylightshow() );
}
```

Finally, at the top level you can schedule Adel functions to be run at a specific interval using `aevery`. For example, let's say you want to check for some user input every 500ms throughout the whole light show, you can write this:

```{c++}
void loop()
{ 
  arepeat( mylightshow() );
  aevery( 500, checkforinput() );
}
```
## Debugging

In general, debugging microcontroller programs is tough. Adel offers some help in the form of a debug mode that prints the names of the Adel functions being executed to the serial interface. To enable debugging mode, add the following line *before* the include of `adel.h`:

```{c++}
#define ADEL_DEBUG 1
#include <adel.h>
````

## WARNINGS

(1) Do not use `switch` or `break` statements inside Adel functions. The co-routine implementation encloses all function bodies in a giant switch statement to allow them to be reentrant. Adding other switch and break statements can have unpredictable results.

(2) Loops, like `for` and `while`, are perfectly fine to use inside Adel functions, but make sure that there is at least one Adel function (like `adelay`) in the body, so that the loop does not stall the rest of the program.

(3) All Adel functions must be called within one of the concurrency constructs. You should never call an Adel function directly as a regular statement. It will not do what you want (and probably won't even compile).

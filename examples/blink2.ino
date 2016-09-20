#include <adel.h>

/** Very simple Adel program
 *  
 *  Blink two LEDs at different rates.
 */

#define LED_PIN_1 3
#define LED_PIN_2 6

void setup()
{
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
}

/** blink
 *  
 *  Blink an LED at some interval forever. We don't need to worry about
 *  the infinite loop because an Adel routine can be interrupted by
 *  one of the concurrency patterns, such as auntil.
 */
adel blink(int pin, int interval)
{
  abegin:
  while (1) {
    digitalWrite(pin, HIGH);
    adelay(interval);
    digitalWrite(pin, LOW);
    adelay(interval);
  }
  aend;
}

/** blink2
 *  
 *  Blink two LEDs at different intervals forever. In this case, blink one
 *  every 300ms and the other every 800ms.
 */
adel blink2()
{
  abegin:
  atogether( blink(LED_PIN_1, 300), blink(LED_PIN_2, 800) );
  aend;
}


void loop()
{
  arepeat( blink2() );
}

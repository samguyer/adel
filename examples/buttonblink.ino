#include <adel.h>

/** Turn a blinking light on and off with a button
 *  
 *  Includes debouncing logic that does not interfere with the
 *  blinking light.
 */

#define LED_PIN 3
#define BUTTON_PIN 6

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
}

/** Wait for a button
 *  
 *  First wait for the pin to go high, then debounce, then wait for the
 *  button to go low. The key feature of this routine is that it does not
 *  return until the button is pressed.
 */
adel waitbutton(int pin)
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

/** blink
 *  
 *  Blink an LED continuously at some interval
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

adel buttonblink()
{
  abegin:
  // -- Wait for button
  andthen( waitbutton(BUTTON_PIN) );
  // -- Blink the light until the button is pressed again
  auntil(  waitbutton(BUTTON_PIN), blink(LED_PIN, 300) );
  aend;
}

void loop()
{
  arepeat( buttonblink() );
}


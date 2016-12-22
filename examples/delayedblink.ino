#include <adel.h>

/** Turn a blinking light on and off with a button
 *  
 *  This is a weird version of buttonblink in which the buttons take 1s to take
 *  effect. It's a very easy change to make to the Adel program (just add an
 *  adelay after the button), but very hard for regular Arduino programs.
 */

#define LED_PIN 3
#define BUTTON_PIN 6

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
}

/** Wait for a button
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

/** Delayed button
 *  Waits for a second after the button is pressed
 */
adel delayedbutton(int pin)
{
  abegin:
  andthen( waitbutton(pin) );
  adelay(500);
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
  andthen( delayedbutton(BUTTON_PIN) );
  auntil(  delayedbutton(BUTTON_PIN), blink(LED_PIN, 300) );
  aend;
}

void loop()
{
  arepeat( buttonblink() );
}


// #define ADEL_DEBUG 1

#include <adel.h>

/** Use a button to turn a light on/off slowly
 *  
 *  Uses the aramp command to turn light on/off over a specific period of time
 */

#define LED_PIN 5
#define BUTTON_PIN 6

void setup()
{
  delay(1000);
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(LED_PIN, LOW);
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
         Serial.println("PRESS");
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

adel rampuplight(int pin, int howlong)
{
  int val;
  abegin:
  aramp(howlong, val, 0, 255) {
    analogWrite(pin, val);
    adelay(50);
  }
  analogWrite(pin, 255);
  aend;
}

adel rampdownlight(int pin, int howlong)
{
  int val;
  abegin:
  aramp(howlong, val, 255, 0) {
    analogWrite(pin, val);
    adelay(50);
  }
  analogWrite(pin, 0);
  aend;
}

adel buttonblink()
{
  int howlong;
  abegin:
  // -- Wait for button to get started
  andthen( waitbutton(BUTTON_PIN) );
  // -- Initially, make the light go fast
  howlong = 1000;
  while (1) {
    auntil( waitbutton(BUTTON_PIN) , rampuplight(LED_PIN, howlong) );
    auntil( waitbutton(BUTTON_PIN) , rampdownlight(LED_PIN, howlong) );
    // -- On each cycle, make it go slower
    howlong += 1000;
    Serial.println(howlong);
  }
  aend;
}

void loop()
{
  arepeat( buttonblink() );
}


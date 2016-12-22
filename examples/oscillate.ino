#include <adel.h>

#define LED_PIN 3
#define BUTTON_PIN 6

void setup()
{
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
}

/** Oscillating LED
 *  
 *  This example shows the use of a "local" variable to store state --
 *  in this case, remembering the start time for use in computing
 *  the wave using cosine.
 */
adel oscillate(int pin)
{
  long int start_time;
  
  abegin:
  start_time = millis();
  while (1) {
    {
      double d = (double) (millis() - start_time);
      double b = (1.0 - cos(d/200.0)) * 127.0;
      analogWrite(pin, b);
    }
    adelay(10);
  }

  aend;
}

/** Button
 *  
 *  Wait for a momentary button to be pushed.
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

adel oscillateonoff()
{
  abegin:
  // -- Wait for button
  andthen( waitbutton(BUTTON_PIN) );
  // -- Oscillate until the button is pressed again
  auntil(  waitbutton(BUTTON_PIN), oscillate(LED_PIN) );
  aend;
}

void loop()
{
  arepeat( oscillateonoff() );
}


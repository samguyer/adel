#include <adel.h>

/** Control a light show with commands
 *  
 *  The key feature of this example is that processing the user's input does
 *  not interrupt the "light show".
 */

#define LED_PIN 3

void setup()
{
  while (!Serial);  // required for Flora & Micro
  delay(500);
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Enter a command:");
  Serial.println("   fast: blink the light quickly");
  Serial.println("   slow: blink the light slowly");
  Serial.println("   pulse: pulse the light");
  Serial.println("   stop: stop the show");
}

adel getUserInput(char buffer[], uint8_t maxSize)
{
  uint8_t count;
  
  abegin:
  // -- Wait for serial data  
  await( Serial.available() != 0 );

  // -- Fill the buffer
  memset(buffer, 0, maxSize);
  count = 0;
  do {
    // count += Serial.readBytes(buffer+count, maxSize);
    buffer[count] = Serial.read();
    count++;
    adelay(10);
  } while( (count < maxSize) && !(Serial.available() == 0) );

  aend;
}

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

adel lightshow(int mode)
{
  abegin:
  if (mode == 0) {
    digitalWrite(LED_PIN, LOW);
    adelay(10000);
  }
  if (mode == 1) {
    andthen( blink(LED_PIN, 100) );
  }
  if (mode == 2) {
    andthen( blink(LED_PIN, 1000) );
  }
  if (mode == 3) {
    andthen( oscillate(LED_PIN) );
  }
  aend;
}

#define BUFSIZE 20
char command[BUFSIZE+1];

adel runshow()
{
  int mode;
  abegin:
  
  mode = 0;

  while(1) {
    auntil( getUserInput(command, BUFSIZE), lightshow(mode) );
    if (strcmp(command, "stop") == 0) {
      mode = 0;
    }
    if (strcmp(command, "fast") == 0) {
      mode = 1;
    }
    if (strcmp(command, "slow") == 0) {
      mode = 2;
    }
    if (strcmp(command, "pulse") == 0) {
      mode = 3;
    }
  }

  aend;
}

void loop()
{
  arepeat( runshow() );
}


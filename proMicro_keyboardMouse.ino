#include "Keyboard.h"
#include "Mouse.h"
#define BUF_LEN 1024
#define BUF_LEN0 (BUF_LEN-1)
#define REASONABLE 1000
#define US_SPEED 9600
#define PS_SPEED 9600
#define SEC 1000
#define MAX_INT_LEN 6
#define MAX_INT_CHARS 10
#define GOOD 0
#define BAD 1

//uS -> USB
//pS ->rx/tx
#define pS (&Serial1)

#define USBDEBUG

#ifdef USBDEBUG
#define uS (&Serial)
#endif

char  dataBuffer[BUF_LEN];
int   dOff;


void moveMouse(char * l, char * d)
{
  //Serial.println(l);
  //Serial.println(d);
  unsigned int pixels;
  pixels = atoi(l);
  switch (d[0]) {
    case 'u':
    case 'U':
      // move mouse up
      Mouse.move(0, -pixels);
      break;
    case 'd':
    case 'D':
      // move mouse down
      Mouse.move(0, pixels);
      break;
    case 'l':
    case 'L':
      // move mouse left
      Mouse.move(-pixels, 0);
      break;
    case 'r':
    case 'R':
      // move mouse right
      Mouse.move(pixels, 0);
      break;
    case 'm':
      // perform mouse left click
      Mouse.click(MOUSE_LEFT);
      break;
    case 'M':
      // perform mouse left click
      Mouse.click(MOUSE_RIGHT);
      break;
  }
}

void
setup()
{
  delay(10 * SEC);
  memset(dataBuffer, 0x00, BUF_LEN);
  dOff = 0;
  pS->begin(PS_SPEED);
#ifdef USBDEBUG
  uS->begin(US_SPEED);
#endif
  Keyboard.begin();
  Mouse.begin();
}

int
keyboardString(char *d)
{
  //Serial.println("keybaordstring");

  Keyboard.print(d);


}

unsigned char
deNibble(unsigned char x)
{
  if (x >= 'A' and x <= 'F') {
    return x - 'A' + 10;
  }
  if (x >= 'a' and x <= 'f') {
    return x - 'a' + 10;
  }
  if (x >= '0' and x <= '9') {
    return x - '0';
  }
}


int
keyboardMulti(char *d)
{
  //Serial.println("keyboardmulti");
  for (int i = 0; i < strnlen(d, 100); i += 2) {
    unsigned char upper;
    unsigned char lower;
    unsigned char h;

    upper = (0x0f & deNibble(d[i]));
    lower = (0x0f & deNibble(d[i + 1]));

    h = (upper << 4) + lower;

    //Serial.print("multi:");
    //Serial.println(h, HEX);
    Keyboard.press(h);
  }
  delay(100);
  Keyboard.releaseAll();
}

int
doit(char *c, char *l, char *d)
{

  switch (c[0]) {
    case 'A':
      {
        //Serial.println("A");
        keyboardMulti(d);
      }
      break;
    case 'w':
    case 'W':
      {
        //Serial.println("w");
        // no newline
        keyboardString(d);
      }
      break;
    case 'M':
      {
        moveMouse(l, d);
      }
      break;
  }
}

int
validate(char *c, char *l, char *d)
{
  int   len;

  if ((NULL == c) || (NULL == l) || (NULL == d))
    return 6 * BAD;


  if (strnlen(c, MAX_INT_CHARS) != 1) {
    return BAD;
  }


  switch (c[0]) {

    case 'A':
    case 'W':
    case 'w':
    case 'M':
      break;
    default:
      return 2 * BAD;

  }

  if (strnlen(l, MAX_INT_CHARS) > MAX_INT_LEN) {
    return 3 * BAD;
  }
  len = atoi(l);
  if (len < 0 || len > BUF_LEN0) {
    return 4 * BAD;
  }
  if (d != NULL && strnlen(d, REASONABLE + 1) > REASONABLE) {
    return 5 * BAD;
  }
  return GOOD;
}

int
parse(char *data)
{
  char           *c;
  char           *l;
  char           *d;
  int   v;
  c = strtok(data, "|");
  l = strtok(NULL, "|");
  d = strtok(NULL, "|");

  if (c == NULL || l == NULL) {
    return BAD;
  }
  v = validate(c, l, d);

  if (!v) {
    doit(c, l, d);
    return GOOD;
  } else {
    return BAD;
  }

}

void
loop()
{

  int retval;
  if (pS->available() > 0) {
    char    inChar = pS->read();

#ifdef USBDEBUG
    uS->print("Rcv:");
    uS->println(inChar);
#endif

    if (inChar == '\n' || inChar == '\r') {

#ifdef USBDEBUG
      //uS->println("about to send to parse");
      uS->print("RECV:");
      uS->println(dataBuffer);
#endif
      retval = parse(dataBuffer);

#ifdef USBDEBUG
      uS->print("PARSE:");
      uS->println(retval);
#endif

      memset(dataBuffer, 0x00, BUF_LEN);
      dOff = 0;
    } else if (dOff < BUF_LEN0) {
      dataBuffer[dOff] = inChar;
      dOff += 1;
    } else {
      memset(dataBuffer, 0x00, BUF_LEN);
      dOff = 0;
    }

  }
}


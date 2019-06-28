#include <ESP8266WiFi.h>

#include <algorithm>

#define STASSID "SSID"
#define STAPSK  "PASSWORD LONGER THAN 8 CHARS"

#define BUF_LEN0 (BUF_LEN-1)
#define REASONABLE 1000
#define US_SPEED 9600
#define PS_SPEED 9600
#define SEC 1000
#define MAX_INT_LEN 6
#define MAX_INT_CHARS 10
#define RXBUFFERSIZE 1024
#define pS (&Serial)
#define STACK_PROTECTOR  2048 // bytes
#define PARSELEN 1024
#define PARSELEN0 (PARSELEN-1)
#define GOOD 0
#define BAD 1

//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 1
const int  port = 1337;

WiFiServer  server(port);
WiFiClient  serverClients[MAX_SRV_CLIENTS];

char  parseBuffer[PARSELEN];
char  pongBuffer[PARSELEN];
int   state;
int   startFound;
int   countZ;
int   command;
int   commandLen;

void
setup()
{
  memset(parseBuffer, 0x00, PARSELEN);
  memset(pongBuffer, 0x00, PARSELEN);
  state = 0;
  startFound = 0;
  countZ = 0;
  command = 0;
  commandLen = 0;

  delay(2 * SEC);
  pS->begin(US_SPEED);
  pS->setRxBufferSize(RXBUFFERSIZE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  pS->print("\nConnecting to ");
  pS->println(STASSID);
  while (WiFi.status() != WL_CONNECTED) {
    pS->print('.');
    delay(SEC / 2);
  }
  pS->println();
  pS->print("connected, address=");
  pS->println(WiFi.localIP());

  //start server
  server.begin();
  server.setNoDelay(true);

  pS->print("Ready! Use nc ");
  pS->print(WiFi.localIP());
  pS->printf(" %d' to connect\n", port);
}

int
validate(char *c, char *l, char *d)
{
  int   len;
  if (strnlen(c, 10) != 1) {
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
  if (len < 0 || len > PARSELEN0) {
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

  if (NULL == c || NULL == l) {
    return BAD;
  }
  v = validate(c, l, d);

  if (GOOD == v) {
    serverClients[0].write("valid\n");
    return GOOD;
  } else {
    serverClients[0].write("invalid\n");
    return BAD;
  }

}

void
loop()
{
  //check if there are any new clients
  if (server.hasClient()) {
    //find free / disconnected spot
    int   i;
    for (i = 0; i < MAX_SRV_CLIENTS; i++)
      if (!serverClients[i]) {
        //equivalent to ! serverClients[i].connected()
        serverClients[i] = server.available();

        memset(parseBuffer, 0x00, PARSELEN);
        state = 0;
        startFound = 0;
        countZ = 0;
        command = 0;
        commandLen = 0;
        break;
      }
    //no free / disconnected spot so reject
    if (i == MAX_SRV_CLIENTS) {
      server.available().println("busy");
      //hints:  server.available() is a WiFiClient with short -term scope
      //    when  out of  scope, a WiFiClient will
      //             -flush() - all data will be sent
      //             -stop() - automatically too
      // pS->printf("server is busy with %d active connections\n", MAX_SRV_CLIENTS);
    }
  }
  //check TCP clients for data
  for (int i = 0; i < MAX_SRV_CLIENTS; i++) {
    while (serverClients[i].available()) {
      //working char by char is not very efficient
      if              (!startFound) {
        if (serverClients[i].read() == 'Z') {
          countZ += 1;
        } else {
          countZ = 0;
        }
        if (countZ == 5) {
          startFound = 1;
          serverClients[i].write("START\n");
        }
      } else {
        switch (state) {
          case 0:
            {

              command = serverClients[i].read();
              //get rid of the..Z \ n
              if ('\n' == command || '\r' == command)
                continue;
              state = 1;
              parseBuffer[commandLen] = (char)command;
              commandLen = 1;
            }
            break;
          case 1:
            {
              if (commandLen < PARSELEN - 1) {
                char  c = serverClients[i].read();
                if ('\n' == c || '\r' == c) {
                  serverClients[i].write("Parse\n");
                  memcpy(pongBuffer, parseBuffer, PARSELEN);
                  if (parse(parseBuffer) == GOOD) {
                    pS->write(pongBuffer);
                    pS->write("\r\n");
                    serverClients[i].write("->");
                    serverClients[i].write(pongBuffer);
                    serverClients[i].write("\n");
                  }
                  memset(parseBuffer, 0x00, PARSELEN);
                  memset(pongBuffer, 0x00, PARSELEN);

                  state = 0;
                  commandLen = 0;
                } else {
                  parseBuffer[commandLen] = c;
                  commandLen += 1;
                }
              }
            }
            break;
          case 2:
            break;
        }
      }
    }
  }

  //determine maximum output size "fair TCP use"
  // client.availableForWrite() returns 0 when ! client.connected()
  size_t    maxToTcp = 0;
  for (int i = 0; i < MAX_SRV_CLIENTS; i++)
    if (serverClients[i]) {
      size_t    afw = serverClients[i].availableForWrite();
      if (afw) {
        if (!maxToTcp) {
          maxToTcp = afw;
        } else {
          maxToTcp = std::min(maxToTcp, afw);
        }
      } else {
        //warn but ignore congested clients
        //pS->println("one client is congested");
      }
    }
  //check UART for data
  size_t len = std::min((size_t) Serial.available(), maxToTcp);
  len = std::min(len, (size_t) STACK_PROTECTOR);
  if (len) {
    uint8_t   sbuf   [len];
    size_t    serial_got = Serial.readBytes(sbuf, len);
    //push UART data to all connected telnet clients
    for (int i = 0; i < MAX_SRV_CLIENTS; i++)
      //if client.availableForWrite() was 0(congested)
      // and increased since then,
      //ensure write space is sufficient:
      if (serverClients[i].availableForWrite() >= serial_got) {
        size_t    tcp_sent = serverClients[i].write(sbuf, serial_got);
        if (tcp_sent != len) {
          //pS->printf("len mismatch: available:%zd serial-read:%zd tcp-write:%zd\n", len, serial_got, tcp_sent);
        }
      }
  }
}

#include <Ethernet.h>
#include <SPI.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <EthernetBonjour.h>


class Control
{
  public:
    char  *address;
    float value;
    int inputPin;
    int indicatorPin;
};

bool found = false;

EthernetUDP Udp;

//destination IP
IPAddress outIp;
unsigned int outPort;

byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const int CONTROL_COUNT = 2;

Control controls[CONTROL_COUNT];

void setup() {

  Control c1;
  c1.address = "/1/volume1";
  c1.inputPin = 0;
  controls[0] = c1;

  Control c2;
  c2.address = "/1/volume2";
  c2.inputPin = 0;
  controls[1] = c2;
  
  Serial.begin(9600);
  Serial.print("DHCP....");
  if (!Ethernet.begin(mac)) {
    Serial.println("failed to get IP");
    while(1);
  }

  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  
  Serial.println();
  
  Udp.begin(7001);

  EthernetBonjour.begin("Arduino");

  EthernetBonjour.setServiceFoundCallback(serviceFound);

  EthernetBonjour.addServiceRecord("Arduino [iPad] (TouchOSC)._osc", 7001, MDNSServiceUDP);
  
  while(!found){
    if (!EthernetBonjour.isDiscoveringService())
    {
      Serial.print("Discovering services");
      EthernetBonjour.startDiscoveringService("_osc", MDNSServiceUDP, 5000);
    }
    EthernetBonjour.run();
  }
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void loop()
{  
  EthernetBonjour.run();
    
  for (int i = 0; i < CONTROL_COUNT; i++) {
    Control *c = &controls[i];
    float val = mapfloat(analogRead(c->inputPin), 0, 1023, 0, 1);
    float valueDifference = abs(val - c->value);
    if (valueDifference < 0.1 && valueDifference > 0.010 ) {
      c->value = val;
      OSCMessage msg(c->address);
      msg.add(val);
      Udp.beginPacket(outIp, outPort);
      msg.send(Udp);
      Udp.endPacket();
      msg.empty(); 
    }
  }
  

  OSCBundle bundleIN;
  int size;

  if ((size = Udp.parsePacket()) > 0) {
    while (size--) {
      bundleIN.fill(Udp.read());
    }

    if (!bundleIN.hasError()) {
      for (int i = 0; i < CONTROL_COUNT; i++) {
        Control *c = &controls[i];
        OSCMessage message = bundleIN.getOSCMessage(c->address);
        if (message.fullMatch(c->address)) {
          c->value = message.getFloat(0);
          Serial.println(c->address);
          Serial.println(c->value);
        }
      }
    }
  }
}


// This function is called when a name is resolved via MDNS/Bonjour. We set
// this up in the setup() function above. The name you give to this callback
// function does not matter at all, but it must take exactly these arguments
// as below.
// If a service is discovered, name, ipAddr, port and (if available) txtContent
// will be set.
// If your specified discovery timeout is reached, the function will be called
// with name (and all successive arguments) being set to NULL.
void serviceFound(const char* type, MDNSServiceProtocol proto,
                  const char* name, const byte ipAddr[4],
                  unsigned short port,
                  const char* txtContent) {
  if (NULL == name) {
    Serial.print("Finished discovering services of type ");
    Serial.println(type);
  } else {

    found = true;
    
    Serial.println("");
    Serial.print("Found: '");
    Serial.print(name);
    Serial.print("' at ");
    Serial.print(ip_to_str(ipAddr));
    Serial.print(", port ");
    outIp = IPAddress(ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
    outPort = port;
    Serial.print(port);

    // Check out http://www.zeroconf.org/Rendezvous/txtrecords.html for a
    // primer on the structure of TXT records. Note that the Bonjour
    // library will always return the txt content as a zero-terminated
    // string, even if the specification does not require this.
    if (txtContent) {
      Serial.print("\ttxt record: ");

      char buf[256];
      char len = *txtContent++, i = 0;;
      while (len) {
        i = 0;
        while (len--)
          buf[i++] = *txtContent++;
        buf[i] = '\0';
        Serial.print(buf);
        len = *txtContent++;

        if (len)
          Serial.print(", ");
        else
          Serial.println();
      }
    }
  }
}

// This is just a little utility function to format an IP address as a string.
const char* ip_to_str(const uint8_t* ipAddr) {
  static char buf[16];
  sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return buf;
}






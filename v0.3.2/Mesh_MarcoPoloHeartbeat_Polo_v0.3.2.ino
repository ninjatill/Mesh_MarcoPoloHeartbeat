#include "Particle.h"

// This strips the path from the filename
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


SYSTEM_THREAD(ENABLED);


bool heartbeat = false;
bool meshPub = false;

unsigned long lastBeatTime = 0;
unsigned long beatTimeout = 1000;

bool meshLost = false;
unsigned long meshLostTime = 0;
unsigned long meshResetTimeout = 600000;  //10 min = 600000

//Device info/Version Variables.
const char version[] = "MeshMarcoPoloHeartbeat_Polo 0.3.2";
char gDeviceInfo[120];  //adjust size as required


void setup() {
    Serial.begin(9600);
    
    Particle.variable("version", version);
    Particle.variable("deviceInfo",gDeviceInfo);
    
    snprintf(gDeviceInfo, sizeof(gDeviceInfo)
        ,"App: %s, Date: %s, Time: %s, Sysver: %s"
        ,__FILENAME__
        ,__DATE__
        ,__TIME__
        ,(const char*)System.version()  // cast required for String
    );

    pinMode(D7, OUTPUT);
    pinMode(D4, INPUT_PULLDOWN);
    
    if (digitalRead(D4) == HIGH) {
        SelectExternalMeshAntenna();
    }
    
    waitUntil(Mesh.ready); //Added for bug in RC27 causing SOS 10 Mesh.subcribe if mesh is not ready. 
    Mesh.subscribe("Marco", ProcessBeat);
}

void loop() {
    
    if (heartbeat && meshPub) {
        if (Mesh.ready()) {
            Mesh.publish("Polo", System.deviceID());
            meshPub = false;
            meshLost = false;
        } else {
            if (!meshLost) {
                meshLostTime = millis();
            }
            meshPub = false;
            meshLost = true;
        }
    }
    
    //Turn off LED after beat timeout.
    if(heartbeat && ((millis() - lastBeatTime) >= beatTimeout)) {
        heartbeat = false;
        digitalWrite(D7, LOW);
    }
    
    //Reset if mesh network is down longer than meshResetTimeout (default 10 min).
    if (meshLost) {
        if (Mesh.ready()) {
            meshLost = false;
        } else {
            if (millis() - meshLostTime > meshResetTimeout) {
                System.reset();
            }
        }
    }
}

void ProcessBeat(const char *name, const char *data) {
    heartbeat = true;
    meshPub = true;
    lastBeatTime = millis();
    digitalWrite(D7,HIGH);
}

void SelectExternalMeshAntenna() {
    #if (PLATFORM_ID == PLATFORM_ARGON)
    	digitalWrite(ANTSW1, 1);
    	digitalWrite(ANTSW2, 0);
    #elif (PLATFORM_ID == PLATFORM_BORON)
    	digitalWrite(ANTSW1, 0);
    #else
    	digitalWrite(ANTSW1, 0);
    	digitalWrite(ANTSW2, 1);
    #endif
}


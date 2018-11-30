#include "Particle.h"


SYSTEM_THREAD(ENABLED);


bool heartbeat = false;
bool meshPub = false;

unsigned long lastBeatTime = 0;
unsigned long beatTimeout = 1000;

bool meshLost = false;
unsigned long meshLostTime = 0;
unsigned long meshResetTimeout = 600000;  //10 min = 600000

const char version[] = "MeshMarcoPoloHeartbeat_Polo 0.3";


void setup() {
    Serial.begin(9600);

    pinMode(D7, OUTPUT);
    
    Particle.variable("version", version);
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


#include "Particle.h"
#include <limits.h>
#include <math.h>

// This strips the path from the filename
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Choose pins for various settings.
#define MPWP D2  //MarcoPolo Wake Pin
#define MPSS D3  //MarcoPolo Sleep Select Pin
#define MPAS D4  //MarcoPolo Antenna Select Pin
#define MPLED D7 //MarcoPolo LED Indicator Pin

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

//Optional Functionality: Use Acknowledgements
//-------------------------------------------------------------------------------------
//If acknowlegements are enabled on the  
struct acknowledgements {
    unsigned long retryInterval = 0; 
    unsigned long uid = 0;
    uint8_t retryCount = 0;
    bool ackd = false;
} ack;
//-------------------------------------------------------------------------------------

bool processBeat = false;
bool processAck = false;

bool heartbeat = false;
bool meshPub = false;

unsigned long lastBeatTime = 0;
unsigned long beatTimeout = 1000;

bool meshLost = false;
unsigned long meshLostTime = 0;
unsigned long meshResetTimeout = 600000;  //10 min = 600000

//Device info/Version Variables.
const char version[] = "MeshMarcoPoloHeartbeat_Polo_v0.5.0";
char gDeviceInfo[120];  //adjust size as required

//Message variables
char beatMsg[128];
char ackMsg[128];

//Sleep variables
bool sleepEnabled = false;
unsigned long beatInterval = 10000;
unsigned long preHeartbeatWakeupBuffer = 1000;  //Number of millis before the heatbeat to wakeup.

void setup() {
    Serial.begin(9600);
    
    Mesh.connect();
    //Particle.connect();

    //Create a device info variable.
    snprintf(gDeviceInfo, sizeof(gDeviceInfo)
        ,"App: %s, Date: %s, Time: %s, Sysver: %s"
        ,__FILENAME__
        ,__DATE__
        ,__TIME__
        ,(const char*)System.version()  // cast required for String
    );
    
    //Setup cloud variables.
    //Particle.variable("version", version);
    //Particle.variable("deviceInfo", gDeviceInfo);
    
    //Setup mesh subscribes.
    Mesh.subscribe("Marco", ProcessBeat);
    Mesh.subscribe("PoloAck", ProcessAck);

    //Set pin modes.
    pinMode(MPSS, INPUT_PULLDOWN);
    pinMode(MPAS, INPUT_PULLDOWN);
    pinMode(MPLED, OUTPUT);
    
    //If antenna select pin is high, activate extenal antenna.
    if (digitalRead(MPAS) == HIGH) {
        SelectExternalMeshAntenna();
    }
}

void loop() {
    if (processBeat) {
        processBeat = false;
        ProcessBeatParameters();
    }
    
    if (processAck) {
        processAck = false;
        ProcessAckParameters();
    }
    
    if (heartbeat && meshPub) {
        //Process Marco parameters, if any.
        
        if (Mesh.ready()) {
            Serial.println("Sending Polo...");
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
    processBeat = true;
    strcpy(beatMsg,data);
    Serial.printlnf("Beat Received Name/Data: %s ; %s", name, data);
}

void ProcessAck(const char *name, const char *data) {
    processAck = true;
    strcpy(ackMsg, data);
    Serial.printlnf("Ack Received Name/Data: %s ; %s", name, data);
}

void ProcessBeatParameters() {
    if (strlen(beatMsg) > 0) {
        //Acknowledgements are being used. Parse the ack parameters.
        char * pch;
        int i = 1;
        
        pch = strtok (beatMsg,":");
        
        if (strcmp(pch,"ACK") == 0) {
            Serial.println("ACK Parameters received. Parsing...");
            acknowledgements newAck;
            
            pch = strtok (NULL, ":");
            while (pch != NULL)
            {
                switch (i++) {
                    case 1:
                        newAck.uid = atol(pch);
                        break;
                    case 2:
                        newAck.retryCount = atoi(pch);
                        break;
                    case 3:
                        newAck.retryInterval = atol(pch);
                        break;
                    case 4:
                        if (strcmp(pch,"SLP") == 0) {
                            sleepEnabled = true;
                        } else {
                            sleepEnabled = false;
                        }
                        break;
                    case 5:
                        if (sleepEnabled) {
                            beatInterval = atol(pch);
                        }
                        break;
                    case 6:
                        if (sleepEnabled) {
                            preHeartbeatWakeupBuffer = atol(pch);
                        }
                        break;
                    //default:
                        
                }
                pch = strtok (NULL, ":");
            }
            
            Serial.printlnf("Old data: %u,%lu,%lu; New Data: %u,%lu,%lu"
                , ack.uid
                , ack.retryCount
                , ack.retryInterval
                , newAck.uid
                , newAck.retryCount
                , newAck.retryInterval
            );
            
            if (newAck.uid > ack.uid || (newAck.uid == ack.uid && ack.ackd == false) || (ack.uid == ULONG_MAX && newAck.uid < ack.uid) ) {
                Serial.println("This is a new heartbeat uid or retry.");
                ack.uid = newAck.uid;
                ack.retryCount = newAck.retryCount;
                ack.retryInterval = newAck.retryInterval;
                ack.ackd = false;
                EnableHeartbeat();
            } else {
                
            }
        } else if (strcmp(pch,"RST") == 0) {
            Serial.println("Resetting Heartbeat.");
            ack.uid = 0;
            ack.retryCount = 0;
            ack.retryInterval = 0;
            ack.ackd = false;
            heartbeat = false;
            sleepEnabled = false;
            meshPub = false;
            digitalWrite(MPLED,LOW);
        } else if (strcmp(pch,"SLP") == 0) {
            Serial.println("Sleep Requested.");
            
            sleepEnabled = true;
            
            pch = strtok (NULL, ":");
            while (pch != NULL)
            {
                switch (i++) {
                    case 1:
                        if (sleepEnabled) {
                            beatInterval = atol(pch);
                        }
                        break;
                    case 2:
                        if (sleepEnabled) {
                            preHeartbeatWakeupBuffer = atol(pch);
                        }
                        break;
                    //default:
                        
                }
                pch = strtok (NULL, ":");
            }
        } else {
            ResetSleep();
        }
        
        
    } else {
        //Acknowlegements not being used. Enable heartbeat.
        EnableHeartbeat();
    }
}

void ProcessAckParameters() {
    if (strcmp(ackMsg, System.deviceID().c_str()) == 0) {
        ack.ackd = true;
        Serial.println("Polo Ackd.");
        
        GoToSleep();
        ResetSleep();
    }
}

void EnableHeartbeat() {
    Serial.println("Enabling Heartbeat...");
    heartbeat = true;
    meshPub = true;
    lastBeatTime = millis();
    digitalWrite(MPLED,HIGH);
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

void GoToSleep() {
    if (sleepEnabled || digitalRead(MPSS) == HIGH) { 
        unsigned long elapsedMillis = millis() - lastBeatTime;
        float remainingSecs = floor( ((float)beatInterval - elapsedMillis) / 1000 ) ;
        unsigned long wakeMillis = preHeartbeatWakeupBuffer + (ack.retryInterval * ack.retryCount);
        float sleepSecs = floor( (float)(beatInterval - wakeMillis - elapsedMillis) / 1000);
        
        Serial.printlnf("BeatInterval: %lu; remSecs: %f; sleepSecs: %f; wakeMillis: %lu"
            , beatInterval
            , remainingSecs
            , sleepSecs
            , wakeMillis
            );
            
        //Only sleep if there is enough remaining time. Otherwise, it's not worth the sleep.
        if ( remainingSecs > 0 && sleepSecs > 0 && remainingSecs >= sleepSecs ) {
            digitalWrite(MPLED, LOW);
            
            //Particle.disconnect();
            Mesh.disconnect();
            Mesh.off();
            
            Serial.printlnf("Sleeping for %f seconds.", sleepSecs);
            
            System.sleep(MPWP,RISING,(unsigned long)sleepSecs);
            
            Mesh.on();
            Mesh.connect();
            //Particle.connect();
        } else {
            Serial.printlnf("Sleep enabled but not enough remaining time (%f) to sleep (%f)."
                , remainingSecs
                , sleepSecs
                );
        }
    }
}

void ResetSleep() {
    sleepEnabled = false;
}


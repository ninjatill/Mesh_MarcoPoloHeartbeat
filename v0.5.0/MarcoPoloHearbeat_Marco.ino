#include "Particle.h"

#define MAX_MESH_NODES 10  //Maximum number of nodes expected on the mesh. Make small as possible to preserve memory space.

// This strips the path from the filename
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Choose pins for various settings.
#define MPWP D2  //MarcoPolo Wake Pin
#define MPSS D3  //MarcoPolo Sleep Select Pin
#define MPAS D4  //MarcoPolo Antenna Select Pin
#define MPLED D7 //MarcoPolo LED Indicator Pin

SYSTEM_THREAD(ENABLED);

//Optional Functionality: Use Acknowledgements
//-------------------------------------------------------------------------------------
//If enabled, the ACK parameters will be sent as part of the heartbeat publish.
//The marco node will resend a heartbeat if an Polo message is not received
//within the RetryTimout period. 
//To enable, set ackRetryTimeout to a valid timeout (every 2-4 seconds recommended.)
//To disable, set ackRetryTimeout to 0.
struct acknowledgements {
    unsigned long retryInterval = 2000; 
    unsigned long uid = 0;
    unsigned long delay = 1000;         //Delay before acknowledging the Polo nodes. This may prevent inbound/outboud packet collissions.
    uint8_t retryCount = 0;
    uint8_t retryProcessed = 0;
    uint8_t index = 0;
} ack;
//-------------------------------------------------------------------------------------

enum stats { TotalHeartbeats = 0, ResponsesReceived = 1 };

bool heartbeat = false;
bool cloudPub = false;

unsigned long beatInterval = 20000;
unsigned long lastBeatMillis = 0;
unsigned long beatTimeout = 8000;
unsigned long lastPoloMillis = 0;


//Variables to record known and reporting endpoints, or nodes.
uint8_t knownNodesCount = 0;
char knownNodes[MAX_MESH_NODES][32];
unsigned long knownNodesStats[MAX_MESH_NODES][2];  //Use to calculate response percentages. Use enum stats to access second dimension.

uint8_t reportingNodesCount = 0;
char reportingNodes[MAX_MESH_NODES][32];
unsigned long reportingNodesMillis[MAX_MESH_NODES];

//Variables to record when cloud connection is lost.
bool cloudLost = false;
unsigned long cloudLostMillis = 0;
unsigned long cloudResetTimeout = 600000; //10 min = 600000

unsigned long sentBeats = 0;
unsigned long sentRetries = 0;
unsigned long responseMissedCount = 0;

//Device info/Version variables.
const char version[] = "Mesh_MarcoPoloHeartbeat_Marco_v0.5.0";
char deviceInfo[120];  //adjust size as required
char msg[100];  //Buffer for creating publish messages.

//Sleep variables
unsigned long preHeartbeatWakeupBuffer = 2000;  //Number of millis before the heatbeat to wakeup.
//bool sleepEnabled = false;  


void setup() {
    Serial.begin(9600);
    
    Particle.variable("version", version);
    Particle.variable("deviceInfo", deviceInfo);
    
    Particle.variable("sentBeats", sentBeats);
    Particle.variable("sentRetries", sentRetries);
    
    Particle.variable("respsMissed", responseMissedCount);
    
    Particle.publish("Marco-Polo heartbeat test started.");
    
    pinMode(MPSS, INPUT_PULLDOWN);
    pinMode(MPAS, INPUT_PULLDOWN);
    pinMode(MPLED, OUTPUT);
    
    snprintf(deviceInfo, sizeof(deviceInfo)
        ,"App: %s, Date: %s, Time: %s, Sysver: %s"
        ,__FILENAME__
        ,__DATE__
        ,__TIME__
        ,(const char*)System.version()  // cast required for String
    );
    
    if (digitalRead(D4) == HIGH) {
        SelectExternalMeshAntenna();
    }

    Mesh.subscribe("Polo", ProcessBeat);
    
    ResetReportingNodes();
    
    Serial.printlnf("Version: %s", version);
    Serial.println("Marco-Polo heartbeat test started.");
    
    Mesh.publish("Marco", "RST");
}


void loop() {

    //Send heartbeat collection message every beatInterval.
    if (!heartbeat && ((millis() - lastBeatMillis) >= beatInterval)) {
        Serial.printlnf("Heartbeat started: %dms response window.", beatTimeout);
        
        ResetReportingNodes();
        if (ack.retryInterval > 0) { ResetAck(); }
        
        heartbeat = true;
        digitalWrite(MPLED, HIGH);
        
        if (Mesh.ready()) {
            Serial.println("Mesh Available. sending Marco...");
            PublishBeat();
        } else {
            Serial.println("Mesh not ready. Cannot send Marco...");
            heartbeat = false;
        }
        
        //Moving millis timer reset to after publish for better synch with Polo nodes.
        lastBeatMillis = millis();
        lastPoloMillis = lastBeatMillis;
    }
    
    //Turn off LED after beat timeout.
    if (heartbeat && ((millis() - lastBeatMillis) >= beatTimeout)) {
        heartbeat = false;
        cloudPub = true;
        digitalWrite(MPLED, LOW);

        Serial.println("Marco response period timed out.");
        ResponseWrapUp();
    }
    
    //Acknowledge each Polo resonse if enabled.
    if ( (ack.retryInterval > 0) && (millis() - lastBeatMillis > ack.delay) ) {
        while (ack.index < reportingNodesCount) {
            Serial.printlnf("Ack: %s", reportingNodes[ack.index]);
            Mesh.publish("PoloAck", reportingNodes[ack.index++]);
        }
    }
    
    //Retry if all nodes haven't reportd within the RetryTimeout period.
    if ( heartbeat && (ack.retryInterval > 0) && (reportingNodesCount < knownNodesCount) ) {
        if ( ((millis() - lastBeatMillis) > (ack.retryInterval * ack.retryCount)) && (ack.retryProcessed < ack.retryCount) ) {
            Serial.println("Retrying heartbeat...");
            ack.retryCount++;
            PublishBeat();
        }
    }
    
    //Publish collected heartbeat results to cloud.
    if (cloudPub) {
        if (Particle.connected()) {
            Serial.print("Publishing results to cloud: ");
            
            char msg[80];
            snprintf(msg, arraySize(msg)-1, "Nodes:%d of %d;Millis:%d", reportingNodesCount, knownNodesCount, lastPoloMillis - lastBeatMillis);
            Serial.println(msg);
            
            bool pubSuccess = Particle.publish("MarcoPoloHeartbeat", msg, PRIVATE);
            if (!pubSuccess) { StartCloudLost(); }
            
            cloudPub = false;
            
            //TODO: Report which knownNodes did not report.
        }
        else {
            StartCloudLost();
        }
    }
    
    //Check for lost cloud. Reset if down for more than cloudResetTimeout (default 10 min).
    if (cloudLost) {
        if (Particle.connected()) {
            EndCloudLost();
        } else {
            if (millis() - cloudLostMillis > cloudResetTimeout) {
                Serial.println("Cloud did not re-establish before timeout. Resetting device...");
                delay(500);
                System.reset();
            }
        }
    }
}

void ResetAck() {
    ack.uid++;
    ack.retryCount = 0;
    ack.index = 0;
    ack.retryProcessed = 0;
}

void PublishBeat() {
    
    char sleepMsg[20] = "";
    if (digitalRead(MPSS) == HIGH) {
        snprintf(sleepMsg, arraySize(sleepMsg), "SLP:%lu:%lu", beatInterval, preHeartbeatWakeupBuffer);
    }
    
    if (ack.retryInterval > 0) {
        snprintf(msg, arraySize(msg), "ACK:%u:%lu:%lu:%s"
            , ack.uid
            , ack.retryCount++
            , ack.retryInterval
            , sleepMsg
        );
        Serial.printlnf("Marco msg: %s", msg);
        Mesh.publish("Marco", msg);
        if (ack.retryCount - 1 > 0) {
            sentRetries++;
        } else {
            sentBeats++;
        }
    } else {
        Mesh.publish("Marco");
        sentBeats++;
    }
}

void ProcessBeat(const char *name, const char *data) {
    //If this is a reponse to a retry, then don't store duplicates.
    if (ack.retryInterval > 0 && ack.retryCount - 1 > 0) {
        //Serial.printlnf("Duplicate check! %s", data);
        for (int i = 0; i < sizeof(reportingNodes); i++) {
            if (strcmp(data, reportingNodes[i]) == 0) {
                //Serial.println("Duplicate Found!");
                return;
            } else if (strcmp(reportingNodes[i],"") == 0) {
                //Serial.println("Blank Found!");
                break;
            }
        }
    }
    
    //Record the device that is responding in the next available reporting slot and exit fast.
    uint8_t count = reportingNodesCount++;
    strcpy(reportingNodes[count], data);
    reportingNodesMillis[count] = millis();
    lastPoloMillis = millis();
}

void ResetReportingNodes() {
    for (int i; i < arraySize(reportingNodes); i++) {
        strcpy(reportingNodes[i], "");
    }
    
    reportingNodesCount = 0;
}

void StartCloudLost() {
    if (!cloudLost) {
        cloudLostMillis = millis();
        Serial.println("Cloud lost. Starting timer.");
    }
    cloudPub = false;
    cloudLost = true;
}

void EndCloudLost() {
    cloudLost = false;
    Serial.println("Cloud Restored. Disabling timer.");
}

void ResponseWrapUp() {
    Serial.println("Cataloging responses...");
    
    if (knownNodesCount < reportingNodesCount) {
        responseMissedCount++;
    }
    
    //Loop through reporting nodes and catatalog any unknown nodes.
    for (int x = 0; x < arraySize(reportingNodes); x++) {
        if (x < reportingNodesCount) {
            Serial.printlnf("Response. index: %i; device: %s; millis: %u", x, reportingNodes[x], reportingNodesMillis[x] - lastBeatMillis);
        } else {
            break;
        }
        
        for (int i = 0; i < arraySize(knownNodes); i++) {
            //If we get to a blank array slot, record this node there.
            if (strcmp(knownNodes[i],"") == 0) {
                snprintf(knownNodes[i], arraySize(knownNodes[i])-1, reportingNodes[x]);
                knownNodesCount++;
                knownNodesStats[i][ResponsesReceived]++;
                break;
            }
            
            //If we the reporting know is known already stop looking for this reporting Node.
            if (strcmp(knownNodes[i],reportingNodes[x]) == 0) {
                knownNodesStats[i][ResponsesReceived]++;
                break;
            } 
        }
    }
    
    //Increment the number of total heartbeats.
    for (int i; i < knownNodesCount; i++) {
        knownNodesStats[i][TotalHeartbeats]++;
    }
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

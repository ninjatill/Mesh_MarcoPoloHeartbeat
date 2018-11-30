#include "Particle.h"


SYSTEM_THREAD(ENABLED);


bool heartbeat = false;
bool cloudPub = false;

unsigned long beatInterval = 10000;
unsigned long lastBeatTime = 0;
unsigned long beatTimeout = 8000;
unsigned long lastPoloTime = 0;

char knownNodes[10][50];
uint8_t knownNodeCount = 0;
bool reportingNodes[10];
uint16_t nodeReportCount = 0;

bool cloudLost = false;
unsigned long cloudLostTime = 0;
unsigned long cloudResetTimeout = 600000; //10 min = 600000

const char version[] = "MeshMarcoPoloHeartbeat_Marco 0.3";


void setup() {
    Serial.begin(9600);

    pinMode(D7, OUTPUT);
    
    Particle.variable("version", version);
    Particle.publish("Marco-Polo heartbeat test started.");
    Mesh.subscribe("Polo", ProcessBeat);
    
    ResetReportingNodes();
    
    Serial.printlnf("Version: %s", version);
    Serial.println("Marco-Polo heartbeat test started.");
}


void loop() {

    //Send heartbeat collection message every beatInterval.
    if (!heartbeat && ((millis() - lastBeatTime) >= beatInterval)) {
        Serial.printlnf("Heartbeat started: %dms response window.", beatTimeout);
        ResetReportingNodes();
        nodeReportCount = 0;
        
        heartbeat = true;
        lastBeatTime = millis();
        lastPoloTime = lastBeatTime;
        digitalWrite(D7, HIGH);
        if (Mesh.ready()) {
            Serial.println("Mesh Available. sending Marco...");
            Mesh.publish("Marco");
        } else {
            Serial.println("Mesh not ready. Cannot send Marco...");
        }
    }
    
    //Turn off LED after beat timeout.
    if(heartbeat && ((millis() - lastBeatTime) >= beatTimeout)) {
        heartbeat = false;
        cloudPub = true;
        digitalWrite(D7, LOW);
        Serial.println("Marco response period timeout.");
    }
    
    //Publish collected heartbeat results to cloud.
    if(cloudPub) {
        if (Particle.connected()) {
            Serial.print("Publishing results to cloud: ");
            
            char msg[80];
            snprintf(msg, arraySize(msg)-1, "Nodes:%d of %d;Millis:%d", nodeReportCount, knownNodeCount, lastPoloTime - lastBeatTime);
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
            if (millis() - cloudLostTime > cloudResetTimeout) {
                Serial.println("Cloud did not re-establish before timeout. Resetting device...");
                delay(200);
                System.reset();
            }
        }
    }
}


void ProcessBeat(const char *name, const char *data) {
    Serial.printlnf("Polo response received from: %s; millis: %u", data, millis() - lastBeatTime);
    
    //Loop through known nodes array and look for matches.
    for (int i; i < arraySize(knownNodes); i++) {
        //If we get to a blank array slot, record this node there.
        if (strcmp(knownNodes[i],"") == 0) {
            snprintf(knownNodes[i], arraySize(knownNodes[i])-1, data);
            //knownNodes[i] = data;
            reportingNodes[i] = true;
            nodeReportCount++;
            knownNodeCount++;
            lastPoloTime = millis();
            break;
        }
        
        //If we encounter a node already known, just count it.
        if (strcmp(knownNodes[i], data) == 0) {
            nodeReportCount++;
            reportingNodes[i] = true;
            lastPoloTime = millis();
            break;
        }
    }
}

void ResetReportingNodes() {
    for (int i; i < arraySize(reportingNodes); i++) {
        reportingNodes[i] = false;
    }
}

void StartCloudLost() {
    if (!cloudLost) {
        cloudLostTime = millis();
        Serial.println("Cloud lost. Starting timer.");
    }
    cloudPub = false;
    cloudLost = true;
}

void EndCloudLost() {
    cloudLost = false;
    Serial.println("Cloud Restored. Disabling timer.");
}

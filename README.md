# Mesh_MarcoPoloHeartbeat
Sample code for testing the mesh network of Particle Mesh devices.

This is fairly simple code to test the mesh network connectivity on new Particle Mesh devices (currently Gen 3 devices). The code is to mimic the Marco-Polo game played in a swimming pool. To play the game, the person who is "it" closes their eyes and tries to tag another player within the pool. To hone in on the other players, the "it" person calls out "Marco!" to which everyone else is obligated to respond "Polo!".

There are 2 sides to the code; Marco and Polo. The Marco code is normally flashed to the mesh gateway while the Polo code is flashed to all the endpoints. The Marco code could reside on an endpoint of the network but the mesh networks are somewhat unstable at the moment (11/30/2018, just a few days after the Pre-Order devices landed in my hands, using device OS 0.8.RC-25). The program flow follows this pattern:
1. The Marco code sends a Mesh.publish() command to which all the endpoints with Polo code subscribe to.
2. When the Polo code receives the "Marco" publish, it responds by issuing it's own Mesh.publish() with it's device ID as the argument. 
3. The Marco code keeps track of the number of total devices on the mesh and how many devices respond within the heartbeat timeout period. 4. The Marco code then publishes the results of the heartbeat to the Particle Cloud. 
5. This data can be sent off to a service such as Losant so that you can visualize the network latency and stability over time.

    Example Losant Dashboard for tracking stability over time:
    
    ![Losant Dashboard](https://github.com/ninjatill/Mesh_MarcoPoloHeartbeat/blob/master/assets/Losant_StarshipTallahassee_20181214.JPG)

### Changelog
#### v0.3.1
+ Added method for selecting external antenna during startup if D4 is high. To use: power down device, attach external antenna to "BT" u.Fl connector, connect D4 to 3.3V, power device back up.
+ ##### Marco 
    Moved cataloging of responses outside of the subscribe callback. This was to ensure that heartbeats were not being missed due to cataloging. Now, the subscribe callback sets flags, records data and exits as fast as possible.
+ ##### Polo 
    Added Particle.variable() for "version". This was to allow you to request the variable from console.particle.io to verify the device is online, contactable, and verify the user firmware version. Increment the version number before flashing the device that way you can verify the flash was received and applied to the device.

#### v0.3.2
+ Added @peekay123's method for creating a device info variable.

#### v0.4.3
+ Implemented an "acknowledgement" system in an attempt to get heartbeat reliability up to 100%. Prior to v0.4.3, it seems that at least a few times an hour, there is a missed heartbeat from one of the node. One of the possible theories is that there is a socket collision at the Marco node (i.e. messages from more than one Polo node arrive at the Marco node at the exact same millisecond.)

+ With v0.3.x, I was seeing that about 99%+ reliability. That high reliability is probably adequate in most situations. After all, you could probably wait until several heartbeats are missed, for an individual node, before sending some type of alert. I have been contemplating how to get up to 100% and I came up with this acknowledgement system. The v0.4.x code should be backwards compatible with nodes running v0.3.x. However, a node running v0.3.x will not be aware of the acknowledgement system and may create superfluous traffic on the mesh.

+ The new traffic flow is:
1. A “Marco” event is published from Marco node… now there is event data which includes an unique ID (UID) for the “Marco” attempt, the current retry count (starts at 0, increments by one on each subsequent retry), and the retry interval timeout.
2. The Polo node responds to the Marco event exactly as before (with a “Polo” event).
3. The Marco node catalogs each response and then sends a “PoloAck” event with the device ID of the Polo node being acknowledged. This step doubles the amount of mesh network traffic.
4. The Polo node accepts the PoloAck event and sets a flag so that it will not respond to any subsequent Marco messages with the same UID.
5. The Marco node will check if all nodes have reported at the ack.retryInterval. If the number of reporting nodes is less than the number of known nodes, another “Marco” event is published. The UID is kept the same but the ack.retryCount is incremented by one. This step repeats every time the retryInterval is reached and the reporting vs known node counts do not match.

#### v0.4.8
+ Adds configurable pins using #define statements for both Marco and Polo nodes.
+ Implements sleep on the Polo nodes. If enabled, the Polo node will sleep for the remainder of the heartbeat. It will wakeup a set number of seconds prior to the next expected heartbeat (controlled by the variable preHeartbeatWakeupBuffer). There are 2 ways to enable the sleep:
1. On the Marco node, set the MarcoPolo Sleep Select (MPSS) pin HIGH to enable sleep for all Polo nodes on the mesh. By default, the MPSS is pin D3. When enabled, the Marco adds parameters onto the Marco event payload so the Polo node is aware of the heartbeat interval as well as the preHeartbeatWakupBuffer settings.
2. On the Polo node, set the MarcoPolo Sleet Select (MPSS) pin HIGH to enable sleep for just a single Polo node. Since the Polo node does not receive the sleep settings from the Marco event payload, the heartbeat interval and the preHeartbeatWakeupBuffer settings will need to be hard-coded into the Polo.ino code.

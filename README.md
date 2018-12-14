# Mesh_MarcoPoloHeartbeat
Sample code for testing the mesh network of Particle Mesh devices.

This is fairly simple code to test the mesh network connectivity on new Particle Mesh devices (currently Gen 3 devices). The code is to mimic the Marco-Polo game played in a swimming pool. To play the game, the person who is "it" closes their eys and trys to tag another player within the pool. To hone in on the other players, the "it" person calls out "Marco!" to which everyone else is obligated to respone "Polo!".

There are 2 sides to the code; Marco and Polo. The Marco code is normally flashed to the mesh gateway while the Polo code is flashed to all the endpoints. The Marco code could reside on an endpoint of the network but the mesh networks are somewhat unstable at the moment (11/30/2018, just a few days after the Pre-Order devices landed in my hands, using device OS 0.8.RC-25). The Marco code sends a Mesh.publish() command to which all the endpoints with Polo code subscribe to. When the Polo code receives the "Marco" publish, it responds by issuing it's own Mesh.publish() with it's device ID as the argument. The Marco code keeps track of the number of total devices on the mesh and how many devices respond within the heartbeat timeout period. The Marco code then publishes the results of the heartbeat to the Particle Cloud. This data can be sent off to a service such as Losant so that you can visualize the network latency and stability over time.

### Changelog
#### v0.3.1
+ Added method for selecting external antenna during startup if D4 is high. To use: power down device, attach external antenna to "BT" u.Fl connector, connect D4 to 3.3V, power device back up.
+ ##### Marco 
    Moved cataloging of responses outside of the subscribe callback. This was to ensure that heartbeats were not being missed due to cataloging. Now, the subscribe callback sets flags, records data and exits as fast as possible.
+ ##### Polo 
    Added Particle.variable() for "version". This was to allow you to request the variable from console.particle.io to verify the device is online, contactable, and verify the user firmware version. Increment the version number before flashing the device that way you can verify the flash was received and applied to the device.

#### v0.3.2
+ Added @peekay123's method for creating a device info variable.

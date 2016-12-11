# Network-Layer2-Interface


Layer2 Interface's main purpose is to trigger events whenever there is activity at the network adapter. The network adapter events that are handled by the CL2I are
Link_Down
Link_Up
Link_Going_Down
Link_Detected


MIH_Link_Down

it is an event that is generated when any of the connected network interfaces goes down or the device is out of the range of an access point and signals are not being received..
This event is implemented by making sue of the NETLINK sockets. These sockets are used to transfer information between kernel modules and user space processes, it provides kernel/user space bidirectional communication links. It consists of a standard sockets based interface for user processes and an internal kernel API for kernel modules. 

In this case NETLINK sockets are used to communicate with the kernel, as soon as any of the active interface goes down ( I.e. the hardware button is turned off) or when the system is out of the range of the AP and is unable to receive the signals, kernel sends back a message indicating the occurred change.

In this case case there is an open netlink socket that is continuously monitored for any events received on the socket. 


Link_Up
whenever CL2I will receive a message from the HA, indicating to join a specific ESSID along with the name of the interface on which we want to connect, CL2I will call the following method 

void makeessid(char *interfacename)

this will connect the specified interface name to the specified ESSID. After performing this function successfully MIH_LINK_UP trigger is send to the HA through the MIH_SAP.
The message contains the following



Link_Going_Down
it is continuously monitoring after a specific interval, all the connected interfaces and is recording their Link Quality, Level and Noise.
Calculations are performed on these three attributes and depending upon the results of the calculation Link_Going_Down trigger is generated. 





Link_Detected
similarly whenever the select statement  times out, a scan operation is also performed and the all the ESSID's from the surroundings are gathered and compared with the previous ESSID's, if there is a new ESSID, MIH_Link_Detected trigger is generated.




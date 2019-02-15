# COP5601: Operating System Assignment # 1
## Authors: Muhammad Haseeb and Usman Tariq
# Requirements
1.	cmake, g++ and rpc libary are required to run this project.
2.	g++ can be installed by using the command:
sudo apt-get install g++
3.	rpc library can be downloaded and installed from the following github repository.
https://github.com/rpclib/rpclib
4.	Note that installation of rpc library requires cmake 3.9 or later.

# Running the Program
The program was tested on signle machine and on a Three-Machine network on the google cloud.
## Single Machine
1.	Uncompress the source.
2.	Run "make" to compile the program. (The code was tested on Ubuntu 16.04)
3.	The program should compile without any warnings or errors.
4.	Three .exe files will be generated i.e. gateway.exe, sensors.exe, and devices.exe
5.	Run gateway.exe first. The gateway server will start and wait for devices and sensors to register. Leave the server running in the separate terminal.
6.	Open two new terminals. In the first one run './devices.exe 127.0.0.1 8080' and in the second one run './sensor.exe 127.0.0.1 8080'.
7.	Important: The `gateway.exe` must be run before any of the sensors.exe or devices.exe.
8.	The gateway will perform tests with sensors and devices and start automation in HOME mode. On the gateway terminal, the user will have the option to switch between HOME/AWAY mode or EXIT the automation to shutdown all gateway, sensors and devices.

## Distributed System
1.	The code was successfully tested on three VM instances on google cloud with machine type "n1-standard-2 (2 vCPUs, 7.5 GB memory)" and 10GB of storage.
2.	The project on Google cloud has been shared with the email address "qli027@fiu.edu" where you can simply execute the process.
3.	Run the three instances i.e. gateway, sensors, and devices.
4.	cd to the iotOS folder.
5.	On gateway, run `make gateway.exe`. On sensors, run `make sensors.exe`, and on devices run `make devices.exe`.
6.	Once the compilation is successful, run the corresponding files on each instances in the mentioned order: First, run `./gateway.exe` on gateway isntance. Then, either run `./devices.exe <gateway_IP> 8080` on devices instance and then run `./sensors.exe <gateway_IP> 8080` on sensors instance.
7.	Important: The `gateway.exe` must be run before any of the `sensors.exe` or `devices.exe` else you may need to restart everything.
8.	The gateway will perform tests with sensors and devices and start automation in HOME mode. On the gateway terminal, the user will have the option to switch between HOME/AWAY mode or EXIT the automation to shutdown all gateway, sensors and devices.

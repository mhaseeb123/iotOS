# iotOS
COP5601: Operating System Assignment # 1
# Requirements
g++ and rpc libary are required to run this project.
g++ can be installed by using the command:
sudo apt-get install g++
rpc library can be downloaded and installed from the following github repository.
https://github.com/rpclib/rpclib
Note that installation of rpc library requires cmake 3.9 or later.

# Running the Program
The program was tested on signle machine and on a Three-Machine network on the google cloud.

## Single Machine
Uncompress the source.
Run "make" to compile the program. (The code was tested on Ubuntu 16.04)
The program should compile without any warnings or errors.
Three .exe files will be generated i.e. gateway.exe, sensors.exe, and devices.exe
Run gateway.exe file. The gateway server will start and wait for devices and sensors to register. Leave the server running in the separate terminal.
Open two new terminals. In the first one run the command "./devices.exe 127.0.0.1 8080" and in the second one run "./sensor.exe 127.0.0.1 8080".
Once the sensors and devices register successfully, the automation process will start.
On the gateway terminal the user has the option to switch between HOME and AWAY mode or simply EXIT.

## Distributed System
The code was successfully tested on three VM instances on google cloud with machine type "n1-standard-2 (2 vCPUs, 7.5 GB memory)" and 10GB of storage.
The project on google cloud has been shared with the email address "qli027@fiu.edu" where you can simply execute the process.
Run the three instances i.e. gateway, sensors, and devices.
cd to the iotOS folder.
On gateway, run "make gateway.exe". On sensors, run "make sensors.exe", and on devices run "make devices.exe".
Once the compilatoin is successful, run the corresponding files on each instances:
On gateway, run "./gateway.exe". On devices, run "./devices.exe 10.142.0.5 8080". On sensors, run "./sensors.exe 10.142.0.5 8080".
This will start the automation process in HOME mode.
On the gateway console, user can switch between HOME and AWAY mode or EXIT.

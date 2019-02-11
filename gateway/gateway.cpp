#include <iostream>
#include <string>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "rpc/server.h"
#include "rpc/client.h"

#define TEMP 0
#define MOTION 1
#define BULB 2
#define OUTLET 3

#define SENSOR 0
#define DEVICE 1

#define SUCCESS 0
#define FAILURE 1

#define ISMOTION 1
#define NOMOTION 0

#define BULBON = 1
#define BULBOFF = 0



#define HOME                             1 << 0
#define AWAY                             1 << 1
#define EXIT                             1 << 2

int registerf(std::string ltype, std::string lname);

std::string name[4] = {"temp", "motion", "bulb", "outlet"};
std::string type[4] = {"sensor", "sensor", "device", "device"};
std::string *ips;
int ports[4] = {0};
static int isRegistered[4] = {0};
int states[4] = {0};

std::string mode = HOME;

void foo() {
    std::cout << "foo was called!" << std::endl;
}

void printstuff()
{
	std::cout << "registered: ";
		for (int i = 0; i < 4; i++)
		{
			std::cout << isRegistered[i] << " ";
		}
		std::cout << std::endl;
		std::cout << "IP: ";
		for (int i = 0; i < 4; i++)
		{
			if (ips[i] != "")
				std::cout << ips[i] << " ";
		}
		std::cout << std::endl;
}


//change to update the state array by shifting long long.
long long query_state(int device_id)
{
	long long value;
	switch (device_id)
	{
		case 0:
			if (isRegistered[0] == 1 && ips[0] != "")
			{
				rpc::client temp(ips[0], ports[0]);
				value = temp.call("query_state", device_id).as<long long>();
			}
			break;
		case 1:
			if (isRegistered[1] == 1 && ips[1] != "")
			{
				rpc::client motion(ips[1], ports[1]);
				value = motion.call("query_state", device_id).as<long long>();
			}
			break;
		case 2:
			if (isRegistered[2] == 1 && ips[2] != "")
			{
				rpc::client bulb(ips[2], ports[2]);
				value = bulb.call("query_state", device_id).as<long long>();
			}
			break;
		case 3:
			if (isRegistered[3] == 1 && ips[3] != "")
			{
				rpc::client outlet(ips[3], ports[3]);
				value = outlet.call("query_state", device_id).as<long long>();
			}
			break;
		default:
			value = 0;
	}
	return value;
}

int change_state(int device_id, int state)
{
	int ack = FAILURE;
	if (isRegistered[device_id] == 1 && ips[device_id] != "")
	{
		rpc::client cln(ips[device_id], ports[device_id]);
		ack = cln.call("change_state", device_id, state);
		
		if (ack == SUCCESS)
			states[device_id] = state;
	}
	return ack;
}

//call this function every 5 secs in a new thread.
int task1()
{
	long long temp_and_id = query_state(TEMP);
	int device_id = (int) (temp_and_id >> 32);
	int temperature = (int) temp_and_id;
	
	if (device_id == TEMP)
	{
		if (temperature < 1)
		{
			chagne_state(OUTLET, true);
		}
		if (temperature > 2)
		{
			change_state(OUTLET, false);
		}
	}
}

int task2(int isMotion)
{
	long long bulb_state_and_id = query_state(BULB);
	int device_id = (int) (bulb_state_and_id >> 32);
	int isON = (int) bulb_state_and_id;
	
	switch (mode)
	{
		case HOME:
			if (states[MOTION] == ISMOTION && isON == BULBOFF)
			{
				// if timer running, stop the timer.
				change_state(BULB, BULBON);
			}
			if (states[MOTION] == NOMOTION && isON == BULBON)
			{
				// if timer not running start 5 min timer
				// else keep the timer running
			}
			break;
		case AWAY:
			if (states[MOTION] == ISMOTION)
			{
				//OMG. HELP. Intruder in the house.
			}
			break;
		default:
	}
}

int change_mode(int inmode)
{
	mode = inmode;
}

int main(int argc, char *argv[]) {
    // Creating a server that listens on port 8080
	ips = new std::string[4];
	
	pthread_t thread1;
	pthread_t thread2;
	
    rpc::server srv(8080);

    // Binding the name "foo" to free function foo.
    // note: the signature is automatically captured
    srv.bind("foo", &foo);

    // Binding a lambda function to the name "add".
    srv.bind("add", [](int a, int b) {
        return a + b;
    });
	
	//4 = wrong name.
	//5 = already registered.
	srv.bind("registerf", [](std::string ltype, std::string lname, std::string IP, int port) {
		
		for (int i = 0; i < 4; i++)
        {
                if (lname.compare(name[i]) == 0)
                {
					if (isRegistered[i] == 0)
					{
                        isRegistered[i] = 1;
						ips[i] = IP;
						ports[i] = port;
						printstuff();
                        return i;
					}
					else
						return 5;
                }
        }
        return 4;
	});
	
	//report_state
	srv.bind("report_state", [](int device_id, int state) {
		if (device_id == 1)
		{
			states[1] = state;
		}
	});

    // Run the server loop.
    srv.run();

    return 0;
}

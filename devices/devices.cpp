/*
 * The MIT License (MIT)
 * Copyright (c) 2019 Muhammad Haseeb, Usman Tariq
 *      COP5614: Operating Systems | SCIS FIU
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "devices.h"
using namespace std;

/* Global Variables */
string gateway_ip;
int gateway_port;
int lead = -1;

/* Store the device ip and port */
string devices_ip;
const int devices_port = 7070;

/* States of smart devices */
bool bulb = false;
bool outlet = false;

/* The device ID assigned by the gateway */
int bulbid = ERR_SENSOR_NOT_REGISTERED;
int outletid = ERR_SENSOR_NOT_REGISTERED;

/* Berkeley Offset */
int offset = 0;

/* Lamport Time (Atomic Counter) */
atomic<int> lamport;

/* Mutex locks for synchronization */
LOCK bulb_lock;
LOCK outlet_lock;
LOCK mode_lock;


/* FUNCTION: getIPAddress
 * DESCRIPTION: Returns my IP.
 * Taken from https://www.geeksforgeeks.org/c-program-display-hostname-ip-address/
 *
 * @params: none
 * @returns: my IP address
 */
string getIPAddress()
{
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    /* Retrieve hostname */
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));

    if (hostname == -1)
    {
        perror("gethostname");
        exit(1);
    }

    /* Retrieve host information */
    host_entry = gethostbyname(hostbuffer);

    if (host_entry == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }

    /* Convert an Internet network
     * address into ASCII string */
    IPbuffer = inet_ntoa(*((struct in_addr*)
                           host_entry->h_addr_list[0]));

    string IP(IPbuffer);

    /* Return IP */
    return IP;
}

/* FUNCTION: query_state
 * DESCRIPTION: The query_state for devices
 *
 * @params: device id
 * @returns: device_id and state
 */
long long query_state(int device_id, int r_lamport)
{
    int res = -1;

	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
		
    /* Check the device id */
    if (device_id == bulbid)
    {
        bulb_lock.lock();
        res = bulb;
        bulb_lock.unlock();
    }
    else if (device_id == outletid)
    {
        outlet_lock.lock();
        res = outlet;
        outlet_lock.unlock();
    }
    else
    {
        /* Set to 0xffffffff */
        res = -1.0;
    }

    /* Multiplex device_id */
    long long query = (long long) res;
    query |= ((long long) device_id << 32);

    return query;
}

/* FUNCTION: request_timestamp
 * DESCRIPTION: Function for server to request timestamp
 *
 * @params: device id
 * @returns: device_id and timestamp
 */
long long request_timestamp(int device_id, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;

	long long res = 0;
	if (device_id == bulbid || device_id == outletid)
	{
		res = getLocalTimeStamp();
	}
	
	return res;
}

/* FUNCTION: set_offset
 * DESCRIPTION: Receive the correction factor from server to adjust the timestamp
 *
 * @params: device id
 * @returns: device_id and offset
 */
long long set_offset(int device_id, int l_offset, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
	
	if (device_id == bulbid || device_id == outletid)
	{
		printf("Offset received %d\n", l_offset);
		offset = l_offset;
	}
	
	long long ret_offset = (long long) l_offset;
	ret_offset |= ((long long) device_id << 32);
	
	return ret_offset;
}

/* FUNCTION: request_lamport
 * DESCRIPTION: Returns lamport time variable to remote requests.
 *
 * @params: none
 * @returns: int local time
 */
 int request_lamport()
 {
	 return lamport;
 }

/* FUNCTION: getLocalTimeStamp
 * DESCRIPTION: Get local time im ms adjusted by the berkeley offset
 *
 * @params: none
 * @returns: int local time
 */
long long getLocalTimeStamp()
{
	auto duration = chrono::system_clock::now().time_since_epoch();
	long long millis = (long long)(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
	millis += offset;
	
	return millis;
}

/*
 * FUNCTION: powerdown
 * DESCRIPTION: System` powerdown
 *
 * @params: none
 * @returns: none
 */
void powerdown(int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
    
	/* Unlock the powerdown mutex */
    mode_lock.unlock();
    return;
}

/*
 * FUNCTION: change_state
 * DESCRIPTION: Change the device state
 *
 * @params: device id and updated state
 * @returns: acknowledgement
 */
STATUS change_state(int device_id, int new_state, int r_lamport)
{
    STATUS status = SUCCESS;

	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
    
	/* Check the device ID */
    if (device_id == bulbid)
    {
        cout << "Changing State for Smart Bulb to: " << new_state << endl;
        bulb_lock.lock();
        bulb = (bool) new_state;
        bulb_lock.unlock();
    }
    else if (device_id == outletid)
    {
        cout << "Changing State for Smart Outlet to: " << new_state << endl;
        outlet_lock.lock();
        outlet = (bool) new_state;
        outlet_lock.unlock();
    }
    else
    {
        /* Invalid device ID */
        status = ERR_INVLD_ARGS;
    }

    return status;
}

/* FUNCTION: leader
 * DESCRIPTION: Choose the leader
 *
 * @params: none
 * @returns: id of leader
 */
void leader(int lid, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
	
	lead = lid;
}

/* FUNCTION: election
 * DESCRIPTION: Bully Algorithm for leader election
 *
 * @params: none
 * @returns: status of leader election
 */
int election(int id, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
		
	cout << "Leader Election in Progress.." << endl;
	
	int myid = bulbid;

	if (myid < outletid)
	{
		myid = outletid;
	}

	if (myid > id)
	{
		cout << "I am the leader..." << endl;
		lead = myid;
		return 0;
	}
	else
	{
		return 1;
	}
}

/*
 * FUNCTION: ServerEntry
 * DESCRIPTION: Entry function for Server Task
 *
 * @params: args: pointer to input arguments
 * @returns: pointer to any results
 */
void *ServerEntry(void *arg)
{
    /* Creating a server */
    rpc::server srv(devices_port);

    /* Register the calles */
    cout << "Registering calls \n";

    srv.bind("query_state", &query_state);
    srv.bind("change_state", &change_state);
    srv.bind("powerdown", &powerdown);
    srv.bind("election", &election);
    srv.bind("leader", &leader);
	srv.bind("request_timestamp", &request_timestamp);
	srv.bind("set_offset", &set_offset);
	srv.bind("request_lamport", &request_lamport);

    /* Run the server loop with 2 threads */
    srv.async_run(2);

    /* Wait for powerdown signal */
    mode_lock.lock();
    
    return NULL;
}

/*
 * FUNCTION: main
 * DESCRIPTION: Main Function
 *
 * @params: args: gateway ip and port
 * @returns: status of execution
 */
STATUS main(int argc, char **argv)
{
    STATUS status = SUCCESS;
    void *result_ptr = NULL;
    pthread_t devs;

    /* Lock the powerdown lock */
    mode_lock.lock();

    /* Check if gateway ip and port passed */
    if (argc < 3)
    {
        cout << "ERROR: Gateway IP or Port Number not specified\n";
        cout << "USAGE: sensor gate.way.ip.addr port\n";

        status = ERR_INVLD_ARGS;
    }

    /* Set gateway IP and port */
    if (status == SUCCESS)
    {
        gateway_ip = argv[1];
        gateway_port = atoi(argv[2]);
    }

    if (status == SUCCESS)
    {
        /* Get my IP */
        devices_ip = getIPAddress();

        if (strcmp(devices_ip.c_str(), "") == 0)
        {
            status = ERR_INVLD_IP;
        }
    }

    /* Create the device side server */
    if (status == SUCCESS)
    {
        printf("Main Task: Creating Server Task\n");
        status = pthread_create(&devs, NULL, &ServerEntry, NULL);
        if (status != SUCCESS)
        {
            printf("Error: Server Task Creation Failed\nABORT!!\n\n");
            status = ERR_THREAD_CREATION_FAILED;
        }
    }

    /* Connect to Gateway and register the devices */
    if (status == SUCCESS)
    {
        cout << "\nConnecting to Gateway...\n";

        // Connect to the gateway
        rpc::client client(gateway_ip, gateway_port);

        cout << "Registering Smart Bulb...\n";

        bulbid = client.call("registerf", "device", "bulb", devices_ip, devices_port, lamport++).as<int>();

        if (bulbid == ERR_SENSOR_NOT_REGISTERED)
        {
            cout << "Smart Bulb Registration Failed..\n";
            status = ERR_DEV_NOT_REGD;
        }

        if (status == SUCCESS)
        {
            cout << "Registering Smart Outlet...\n";
            
            cout << "SUCCESS \n";
            
            outletid = client.call("registerf", "device", "outlet", devices_ip, devices_port, lamport++).as<int>();

            if (outletid == ERR_SENSOR_NOT_REGISTERED)
            {
                cout << "Smart Bulb Registration Failed..\n";
                status = ERR_DEV_NOT_REGD;
            }
        }

        /* Wait for Server Task to complete (shutdown) */
        if (status == SUCCESS)
        {
            pthread_join(devs, &result_ptr);
            pthread_exit(NULL);
        }
    }

    return status;
}
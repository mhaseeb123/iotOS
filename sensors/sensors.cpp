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

#include "sensors.h"
using namespace std;

#define MOTIONTIME    1000
#define AWAYTIME      5000000
#define DOORTIME      8

/* Global Variables */
string gateway_ip;
int gateway_port;
int lead = -1;

/* Store the device ip and port */
string sensor_ip;
const int sensor_port = 6060;

MODE mode = HOME;
int motion = MOTIONTIME;
bool door = false;
float temp = 0.5;
float modeoffset = 0;
bool bari = false;

/* Timer for motion updates */
TIMER motion_timer;

/* Berkeley Offset */
int offset = 0;

/* Lamport Time (Atomic Counter) */
atomic<int> lamport;

/* Sensor IDs assigned by Gateway */
int msensor = ERR_SENSOR_NOT_REGISTERED;
int tsensor = ERR_SENSOR_NOT_REGISTERED;
int dsensor = ERR_SENSOR_NOT_REGISTERED;
int psensor = ERR_SENSOR_NOT_REGISTERED;

/* Locks for thread synchronization */
LOCK offset_lock;
LOCK motion_lock;


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

    return IP;
}

/* FUNCTION: change_mode
 * DESCRIPTION: change_mode for sensors and devices
 *
 * @params: new mode
 * @returns: none
 */
void change_mode(int inmode, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
	
    /* Acquire mode update lock */
    offset_lock.lock();
	
	cout << "Change Mode to = " << inmode << endl;
	
    switch (inmode)
    {
        case HOME:

            mode = HOME;
			bari=false;

            /* Winter time, temperature now varies from 0 to 2C */
			motion = MOTIONTIME;
			(void)SetTimer(MOTIONTIME);
            modeoffset = 0;
            
            break;

        case AWAY:

            mode = AWAY;
		    bari=true;

            /* Spring time, temperature now varies from 10 to 13 C instead */
            modeoffset = 10;

            /* User is away, set the motion timer to 5 sec */
			motion = AWAYTIME;
            (void)SetTimer(AWAYTIME);
            break;

        case EXIT:

            mode = EXIT;

            exit(0); // Shutdown
            break;

        default:
            break;
    }

    /* Release mode update lock */
    offset_lock.unlock();
}

/*
 * FUNCTION: Random Number
 * DESCRIPTION: Returns a random number between
 * -1.0 and 1.0
 *
 * @params: none
 * @returns: random number
 */
int Random_Number()
{
    return rand();
}

/* FUNCTION: SetTimer
 * DESCRIPTION: Reset or Update timer
 *
 * @params: none
 * @returns: timer value in milliseconds
 */
STATUS SetTimer(int msecs)
{
    STATUS status = SUCCESS;

    /* Update Timer */
    motion_timer.it_value.tv_sec = msecs / 1000;
    motion_timer.it_value.tv_usec = (msecs * 1000) % 1000000;
    motion_timer.it_interval = motion_timer.it_value;

    if (setitimer(ITIMER_REAL, &motion_timer, NULL) == -1)
    {
        cout << "Timer Failed !\n";
        status = ERR_TIMER_CREATION_FAILED;
    }

    return status;
}

/*
 * FUNCTION: report_state
 * DESCRIPTION: Entry function for Server Task
 *
 * @params: args: pointer to input arguments
 * @returns: pointer to any results
 */
void *report_state(void *arg)
{
    // Creating a client that connects to the localhost on port 8080
    cout << "\nConnecting to Gateway...\n";

    rpc::client client(gateway_ip, gateway_port);

    cout << "Registering Temperature Sensor...\n";
    tsensor = client.call("registerf", "sensor", "temp", sensor_ip, sensor_port, lamport++).as<int>();

    cout << "Registering Motion Sensor...\n";
    msensor = client.call("registerf", "sensor", "motion", sensor_ip, sensor_port, lamport++).as<int>();

    if (msensor == ERR_SENSOR_NOT_REGISTERED)
    {
        cout << "Motion Sensor Registration Failed..\n";
        return NULL;
    }

    cout << "SUCCESS \n";

    while (true)
    {
        /* Wait for timer expiration */
        motion_lock.lock();

        /* Randomly generate motion with 60% probability */
        bool val = (Random_Number() % 100) < 60 ? true : false;

        if (val==true)
        {
            cout << "Motion Detected " << endl;

            /* Push motion sense data */
            client.call("report_state", msensor, val, lamport++);
        }
    }

    /* No results required */
    return NULL;
}

/*
 * FUNCTION: door
 * DESCRIPTION: Entry function for Door Sensor Task
 *
 * @params: args: pointer to input arguments
 * @returns: pointer to any results
 */
void *doorf(void *arg)
{
    // Creating a client that connects to the localhost on port 8080
    cout << "\nConnecting to Gateway...\n";

    rpc::client client(gateway_ip, gateway_port);

    cout << "Registering Door Sensor...\n";
    dsensor = client.call("registerf", "sensor", "door", sensor_ip, sensor_port, lamport++).as<int>();

    cout << "Registering Keyhain...\n";
    psensor = client.call("registerf", "sensor", "keychain", sensor_ip, sensor_port, lamport++).as<int>();

    if (dsensor == ERR_SENSOR_NOT_REGISTERED)
    {
        cout << "Door Sensor Registration Failed..\n";
        return NULL;
    }
    
    if (psensor == ERR_SENSOR_NOT_REGISTERED)
    {
        cout << "Keychain Registration Failed..\n";
        return NULL;
    }

    cout << "Door Sensor Active \n";

    while (true)
    {
        /* Wait for door */
        sleep(DOORTIME);
    
        /* Randomly generate door change with 70% probability */
        bool val = (Random_Number() % 100) < 70 ? true : false;
        
        /* Send keychain value with 50% probability */
        bool val2 = (Random_Number() % 100) < 50 ? true : false;

        if (val == true)
        {
            /* Check if door state changed */
            door = val;

			/* Check if User is AWAY then 
               randomize between user and 
               intruder
            */
			offset_lock.lock();
            
			if (bari == true)
            {
	            /* Since door was opened and closed */
                /* Change motion timer settings */
				if (motion == AWAYTIME)
				{
					motion = MOTIONTIME;
				}
				if (val2 == true)
				{
					cout << "Keychain Reported " << endl;
			
					client.call("report_state", psensor, val2, lamport++);
				}  
			}
			
			offset_lock.unlock();
			
            cout << "Door Opened " << endl;

            /* Open the door */
            client.call("report_state", dsensor, val, lamport++);
            
			sleep(1);

            /* Close the door */
            client.call("report_state", dsensor, false, lamport++);
            
            cout << "Door Closed " << endl;
            
            door = val;
            
            (void)SetTimer(motion);
        }
    }

    /* No results required */
    return NULL;
}

/* FUNCTION: query_state
 * DESCRIPTION: The query_state for sensors
 *
 * @params: device id
 * @returns: device_id and state
 */
long long query_state(int device_id, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
	
    /* Random number for temperature of motion */
    float res = (float) Random_Number();

    /* Check the device id */
    if (device_id == tsensor)
    {
        /* Acquire offset lock */
        offset_lock.lock();

        /* Report temperature between 0 and 3 */
        res = (res / (float)(RAND_MAX / 3)) + modeoffset;
        
        /* Mux device_id */
        cout << "Temperature Queried " << res << endl;

        /* Scale the temperature readings to integer */
        res *= TEMPSCALE;

        /* Release offset lock */
        offset_lock.unlock();
    }
    else if (device_id == msensor)
    {
        /* If even then motion, else no motion (50 % prob) */
        res = (Random_Number() & 0x1) == 0 ? 1 : 0;
		/* Mux device_id */
        cout << "Motion Queried " << res << endl;
    }
    else if (device_id == dsensor)
    {
        /* Return state of door */
        res = door;
		/* Mux device_id */
        cout << "Door Queried " << res << endl;
    }
    else if (device_id == psensor)
    {
        /* Is it intruder? 50 % prob */
        res = (Random_Number() & 0x1) == 0 ? 1 : 0;
		/* Mux device_id */
        cout << "Keycard Queried " << res << endl;
    }
    else
    {
        /* Set to 0xffffffff */
        res = -1.0;
    }

    /* Scale the temperature to 1000 */
    long long query = (long long) (res);
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
	if (device_id == msensor || device_id == tsensor || device_id == dsensor || device_id == psensor)
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
	
	if (device_id == msensor || device_id == tsensor || device_id == dsensor || device_id == psensor)
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

void Update()
{
    /* Release the motion mutex here */
    motion_lock.unlock();
}

void leader(int lid, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
	
	lead = lid;
}

int election(int id, int r_lamport)
{
	/* Adjust Lamport's Logical Clock */
	if (lamport++ <= r_lamport)
		lamport = r_lamport + 1;
	
	cout << "Leader Election in Progress.." << endl;
	int myid = tsensor;
	
	if (myid < dsensor)
		myid = dsensor;
	if (myid < msensor)
		myid = msensor;
	if (myid < psensor)
		myid = psensor;

	if (myid > id)
	{
		lead = myid;
		cout << "I am the leader..." << endl;
		return 0;
	}
	else
	{
		return 1;
	}
}

/*
 * FUNCTION: Server_Entry
 * DESCRIPTION: Entry function for Server Task
 *
 * @params: args: pointer to input arguments
 * @returns: pointer to any results
 */
void *Server_Entry(void *arg)
{
    /* Creating a server */
    rpc::server srv(sensor_port);

    /* Register calls */
    cout << "Registering calls \n";

    srv.bind("query_state", &query_state);
    srv.bind("change_mode", &change_mode);
    srv.bind("election", &election);
    srv.bind("leader", &leader);
	srv.bind("request_timestamp", &request_timestamp);
	srv.bind("set_offset", &set_offset);
	srv.bind("request_lamport", &request_lamport);
	

    /* Run the server loop. */
    srv.run();

    /* No results required */
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

    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;

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
        /* Get your IP */
        sensor_ip = getIPAddress();

        if (strcmp(sensor_ip.c_str(), "") == 0)
        {
            status = ERR_INVLD_IP;
        }
    }

    if (status == SUCCESS)
    {
        /* Provide a random seed to RNG */
        srand((int) (time(0) * 23) % (1 + 5) + (10 + 1) * 29 - time(0) + (int) time(0) % (5 + 1) * time(0));

        /* Upon SIGALRM, call Update_Temperature() */
        if (signal(SIGALRM, (void (*)(int)) Update)== SIG_ERR)
        {
            cout <<"Unable to catch SIGALRM \n";
            status = ERR_TIMER_CREATION_FAILED;
        }
    }

    /* Create Tasks (pthreads) */
    if (status == SUCCESS)
    {
        printf("Main Task: Creating TSensor Task\n");
        status = pthread_create(&thread1, NULL, &Server_Entry, NULL);
        if (status != SUCCESS)
        {
            printf("Error: TSensor Task Creation Failed\nABORT!!\n\n");
            status = ERR_THREAD_CREATION_FAILED;
        }
    }

    /* Wait for calls to be registered */
    sleep(0.2);

    /* Create Tasks (pthreads) */
    if (status == SUCCESS)
    {
        printf("Main Task: Creating MSensor Task\n");
        status = pthread_create(&thread2, NULL, &report_state, NULL);
        if (status != SUCCESS)
        {
            printf("Error: MSensor Task Creation Failed\nABORT!!\n\n");
            status = ERR_THREAD_CREATION_FAILED;
        }
    }

    /* Create Tasks (pthreads) */
    if (status == SUCCESS)
    {
        printf("Main Task: Creating DSensor Task\n");
        status = pthread_create(&thread3, NULL, &doorf, NULL);
        if (status != SUCCESS)
        {
            printf("Error: DSensor Task Creation Failed\nABORT!!\n\n");
            status = ERR_THREAD_CREATION_FAILED;
        }
    }

    if (status == SUCCESS)
    {
        cout << "Main Task: Creating Timers\n\n";
        status = SetTimer(MOTIONTIME);

        if (status != SUCCESS)
        {
            cout << "Timer Creation Failed\n";
        }
    }

    if (status == SUCCESS)
    {
        /* If Gateway Not available, then exit */
        pthread_join(thread2, &result_ptr);
        pthread_join(thread3, &result_ptr);

        pthread_cancel(thread1);

        //pthread_join(thread1, &result_ptr);

        /* All set - Exit now */
        pthread_exit(NULL);
    }

    return status;
}
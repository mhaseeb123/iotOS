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

#include "gateway.h"

//#define LATENCY

using namespace std;

/* Database of devices and sensors */
 string name[6] = {"temp", "motion", "door", "keychain", "bulb", "outlet"};
 string type[6] = {"sensor", "sensor", "sensor", "sensor", "device", "device"};
 string *ips;
int ports[6] = {0};

/* Global Variables */
int devs_reg = 0;
static int isRegistered[6] = {0};
int lead = -1;

/* Timer and states */
TIMER motion_timer;
float temperature = 1.0;
bool doorstatus = false;
bool authenticated = true;
int states[2] = {OFF, OFF};

/* Berkeley Offset */
int offset = 0;

/* Lamport Time (Atomic Counter) */
atomic<int> lamport;

/* Structures for DB */
vector<string> dbstates;
vector<string> history;


/* Locks for synchronization */
LOCK motion_lock;
LOCK bulb_lock;
LOCK start_lock;
LOCK mode_lock;
LOCK test_lock;

/* Automation Mode */
int mode = HOME;

/* FUNCTION: printHeader
 * DESCRIPTION: Prints the header
 *
 * @params: none
 * @returns: none
 */
void printHeader()
{
 cout << "\n**********************************\n";
 cout << "*                                *\n";
 cout << "* COP5614:  A Smarter Home       *\n";
 cout << "* Muhammad Haseeb, Usman Tariq   *\n";
 cout << "*                                *\n";
 cout << "**********************************\n\n";
}

/* FUNCTION: askMode
 * DESCRIPTION: Prints user menu
 *
 * @params: none
 * @returns: none
 */
void askMode()
{
    printf("\n\nSet the Home Mode? (1= Home), (2 = Away), (0 = Exit) \nCurrent Mode: %d \\> ", mode);
    fflush(stdout);
}

/* FUNCTION: printstuff
 * DESCRIPTION: Debug info
 *
 * @params: none
 * @returns: none
 */
void printstuff()
{
     cout << "Registered: ";
        for (int i = 0; i < 6; i++)
        {
             cout << isRegistered[i] << " ";
        }
         cout <<  endl;
         cout << "IP: ";
        for (int i = 0; i < 6; i++)
        {
            if (ips[i] != "")
                 cout << ips[i] << " ";
        }
         cout <<  endl;
}

/* FUNCTION: query_state
 * DESCRIPTION: query_state of a sensor or device
 *
 * @params: device id
 * @returns: queried device_id and state
 */
long long query_state(int device_id)
{
    long long value = 0;

    if (device_id > 5 || device_id < 0)
        return 0;

    if (isRegistered[device_id] == 1 && ips[device_id] != "")
    {
        rpc::client cln(ips[device_id], ports[device_id]);
        auto start   = chrono::system_clock::now();
        value = cln.call("query_state", device_id, lamport++).as<long long>();
        auto end   = chrono::system_clock::now();
#ifdef LATENCY
        chrono::duration<double> elapsed_seconds = end - start;
        std::cout << "Latency: " << elapsed_seconds.count() << std::endl;
#endif        
    }

    return value;
}

/* FUNCTION: UpdateTimer
 * DESCRIPTION: Reset or Update timer
 *
 * @params: none
 * @returns: timer value in milliseconds
 */
void UpdateTimer(int msecs)
{
    motion_timer.it_value.tv_sec = msecs / 1000;
    motion_timer.it_value.tv_usec = (msecs * 1000) % 1000000;
    motion_timer.it_interval = motion_timer.it_value;

    if (setitimer(ITIMER_REAL, &motion_timer, NULL) == -1)
    {
        cout << "Timer Update Failed !\n";
    }

    return ;
}

/* FUNCTION: DisableTimer
 * DESCRIPTION: Disables the timer
 *
 * @params: none
 * @returns: none
 */
void DisableTimer()
{
    motion_timer.it_value.tv_sec = 0;
    motion_timer.it_value.tv_usec = 0;
    motion_timer.it_interval = motion_timer.it_value;

    if (setitimer(ITIMER_REAL, &motion_timer, NULL) == -1)
    {
        cout << "Disable Timer Failed !\n";
    }

    return ;
}

/* FUNCTION: change_state
* DESCRIPTION: change_state of a sensor or device
*
* @params: device id
* @returns: acknowledgement from device/sensor
*/
int change_state(int device_id, int state)
{
    int ack = FAILURE;

    if (isRegistered[device_id] == 1 && ips[device_id] != "")
    {
        rpc::client cln(ips[device_id], ports[device_id]);
        ack = cln.call("change_state", device_id, state, lamport++).as<int>();

		/* Log the event into database */
		long long timeStamp = cln.call("request_timestamp", device_id, lamport++).as<long long>();
		int l_lamport = cln.call("request_lamport").as<int>();
		if (lamport++ <= l_lamport)
			lamport = l_lamport + 1;
		setState(device_id, state);
		logEntry(timeStamp, lamport, device_id, state, " ");
		
        if (ack == SUCCESS)
            states[device_id - 4] = state;
    }

    return ack;
}

/* FUNCTION: TimerExpired
 * DESCRIPTION: The routine for no motion timer expiration
 *
 * @params: none
 * @returns: none
 */
void TimerExpired()
{
    int ack = SUCCESS;

  /* Acquire bulb control lock */
    bulb_lock.lock();

#ifdef DEBUG
    cout << "Timer Expired " << endl;
#endif

   /* Turn the bulb off if on */
    if (states[BULB-4] == ON)
    {
        ack = change_state(BULB, OFF);
    }

    /* Release bulb control lock */
    bulb_lock.unlock();

    /* Check for acknowledgement */
    if (ack != SUCCESS)
    {
        cout << "ERROR: change_state for bulb failed with error code:\t" << ack << endl;
    }
}

/* FUNCTION: text_message
 * DESCRIPTION: The intruder alert message for user
 *
 * @params: interrupt signal
 * @returns: Intruder alert message
 */
void text_message(int sig)
{
    cout << endl << endl << "     !!!INTRUDER ALERT !!!\nInfo: Sensed motion in the house" << endl;
    askMode();

    return;
}

/* FUNCTION: UserEntry
 * DESCRIPTION: Entry Function for User Task
 *
 * @params: none
 * @returns: none
 */
void *UserEntry(void *arg)
{
    /* Wait for Smart Home initialization */
    start_lock.lock();
    start_lock.unlock();
    int lmode = HOME;
    void (*prev_handler)(int);

    while (true)
    {
        /* Print user menu */
        askMode();
        cin >> lmode;

        /* User is home */
        if (lmode == HOME)
        {
			authenticated = (query_state(KEY) & 0xFF);

			if (authenticated == true)
			{
				/* Disarm the intruder alert */
				signal(SIGINT, SIG_DFL);
				/* Change mode to Home (& Winters) */
				change_mode(lmode);
			}
        }
        /* User is going on vacation */
        else if (lmode == AWAY)
        {
			authenticated = false;
            /* Arm the intruder alert */
            prev_handler = signal(SIGINT, text_message);
            /* Change mode to Away (& Spring break) */
            change_mode(lmode);
        }

        /* Shutdown Automation */
        else if (lmode == EXIT)
        {
            signal(SIGINT, SIG_DFL); // Disarm the intruder alert
            change_mode(lmode); // Shutdown mode
            exit(0); // Shutdown
        }
    }

    return NULL;
}

/* FUNCTION: HeatManage
 * DESCRIPTION: Entry Function for smart outlet control task
 *
 * @params: none
 * @returns: none
 */
void *HeatManage(void *arg)
{
    int ack = SUCCESS;

    /* Wait for automation start */
    start_lock.lock();
    start_lock.unlock();

    long long temp_and_id = -1;
    int device_id = -1;

    while (true)
    {
        /* Wait for 5 seconds */
        sleep(5);

        /* Query the current temperature */
        temp_and_id = query_state(TEMP);
        device_id = (int)(temp_and_id >> 32);
        temperature = (float)(temp_and_id & 0xFFFFFFFF);

#ifdef DEBUG
        cout << "Temperature Reported: "<< temperature << endl;
#endif
        /* Validate device id reported */
        if (device_id == TEMP)
        {
            /* Too cold. Turn the outlet ON if OFF */
            if (temperature < 1.0 * TEMPSCALE)
            {
                if (states[OUTLET-4] == OFF)
                {
                    ack = change_state(OUTLET, ON);
                }
            }
            /* Not too cold. Turn the outlet OFF if ON */
            if (temperature > 2.0 * TEMPSCALE)
            {
                if (states[OUTLET-4] == ON)
                {
                    ack = change_state(OUTLET, OFF);
                }
            }
        }
        /* Report if acknowledgement failed */
		if (ack != SUCCESS)
		{
            cout << "ERROR: change_state failed for Outlet with code:\t" << ack << endl;
		}
    }

    return NULL;
}

/* FUNCTION: BulbManage
 * DESCRIPTION: Entry Function for Smart bulb management task
 *
 * @params: none
 * @returns: none
 */
void *BulbManage(void *arg)
{
    STATUS ack = SUCCESS;
    start_lock.lock();
    start_lock.unlock();

    while (true)
    {
        /* Consume the motion sensor lock */
        motion_lock.lock();

        /* Acquire the mode and bulb management locks */
        bulb_lock.lock();
        mode_lock.lock();

        if (isRegistered[BULB] == 1)
        {
                switch (mode)
                {
                    case HOME:
                        /* Turn bulb ON if OFF */
                        if (states[BULB-4]==OFF)
                        {
                            ack = change_state(BULB, ON);
                        }
                        UpdateTimer(HOMETIMER); // Reset the timer
                        break;

                    case AWAY:
                        raise(SIGINT);  // Raise the alert
                        DisableTimer(); // Keep the timer off.
                        break;
                    default:
                        break;
                }
        }

        /* Release the mode and bulb management locks */
        mode_lock.unlock();
        bulb_lock.unlock();

        /* Check if failed */
        if (ack != SUCCESS)
        {
            cout << "ERROR: change_state failed for bulb with code:\t" << ack << endl;
        }
    }

    return NULL;
}

/* FUNCTION: berkeleyTime
 * DESCRIPTION: Entry function for berkeley_manage task
 *
 * @params: none
 * @returns: none
 */
void *berkeleyTime(void *arg)
{
	int num_devs = 6;
	long long timestamp[num_devs];
	int response = 0;
	while (true)
	{
		long long avg = 0;
		for (int device_id = 0; device_id < num_devs; device_id++)
		{
			rpc::client cln(ips[device_id], ports[device_id]);
			//FixMe: return from remote call has device id in the upper 32 bits
			timestamp[device_id] = cln.call("request_timestamp", device_id, lamport++).as<long long>();
			avg += timestamp[device_id];
		}
		
		avg += getLocalTimeStamp();
		avg /= (num_devs + 1);
		
		for (int device_id = 0; device_id < num_devs; device_id++)
		{
			int remote_offset = (int)(avg - timestamp[device_id]);
			rpc::client cln(ips[device_id], ports[device_id]);
			//FixMe: return is long long.
			response = (int)(cln.call("set_offset", device_id, remote_offset, lamport++).as<long long>());
			if (response != remote_offset)
				printf("Error: berkeleyTime - device id: %d not set properly.\n", device_id);
		}
		printf("Berkeley: Time adjusted to %lld\n", avg);
		sleep(10); //repeat every 10 seconds.
	}
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

/* FUNCTION: change_mode
 * DESCRIPTION: change_mode for sensors and devices
 *
 * @params: new mode
 * @returns: none
 */
void change_mode(int lmode)
{
   /* Acquire mode change lock */
    mode_lock.lock();

    if (lmode != mode)
    {
        mode = lmode;
        rpc::client cln(ips[MOTION], ports[MOTION]);
        cln.async_call("change_mode", lmode, lamport++); // Change mode on sensors

        if (lmode == HOME)
        {
            change_state(BULB, ON); // Home. Turn bulb on
            UpdateTimer(HOMETIMER); // Set no motion timer to 2s (5 mins)
        }
        else if (lmode == AWAY)
        {
            change_state(BULB, OFF); // Turn bulb off
            DisableTimer(); // Disable motion timer
        }
        else if (lmode == EXIT)
        {
            rpc::client cln2(ips[OUTLET], ports[OUTLET]);
            cln2.call("powerdown", lamport++); // Powerdown
        }
    }

    /* Release mode change lock */
    mode_lock.unlock();
}

/* FUNCTION: test_system
 * DESCRIPTION: Tests the smart home system
 *
 * @params: none
 * @returns: status
 */
STATUS test_system()
{
    STATUS status = SUCCESS;
    int count = 0;
    long long status_and_id;
    int device_id;

    /* Test device registration */
    for (int i = 0; i < 6; i++)
    {
        if (isRegistered[i] == 1)
            count++;
    }

    if (count == 6)
    {
         cout << "\nAll sensors and devices are registered." <<  endl;
         cout << "\nPerforming System Test...\n" <<  endl;
         cout << "Information:" <<  endl;
    }
    else
    {
         cout << "Error: Some sensors or devices might not have been registered or configured properly. "
                   "Please contact our customer support for further assistance! :p" <<  endl;
        status = FAILURE;
    }

    for (int i = 0; i < 6; i++)
    {
         cout << name[i] << ":\t" << ips[i] << "\t" << ports[i] <<  endl;
    }

    /* Test the query state for heartbeat */
     cout << "\nTesting query_state:" <<  endl;

    for (int i = 0; i < 6; i++)
    {
        status_and_id = query_state(i);
        device_id = (int)(status_and_id >> 32);

    if (device_id == i)
    {
             cout << name[i] << " " << type[i] << " status:\tOK" <<  endl;
    }
    else
    {
            status = FAILURE;
			cout << name[i] << " " << type[i] << " didn't respond as expected" <<  endl;
        }
    }

  /* Test remote control for devices */
     cout << "\nTesting change_state:" << endl;

    for (int state = OFF; state <= ON; state ++)
    {
        cout << "Turning devices:\t" << state << endl;

        for (int i = BULB; i <= OUTLET; i++)
        {
            status = change_state(i, state);

            if (status == SUCCESS)
            {
                 cout << name[i] << " " << type[i] << " status:\tOK" <<  endl;
            }
            else
            {
                 cout << name[i] << " " << type[i] << " didn't respond as expected. Error code:\t" << status <<  endl;
            }
        }
    }

    return status;
}

STATUS lelection()
{
	int i;
	int ack = -1;
	int myid = GATEWAY;

	cout << "Leader Election in Progress.." << endl;
	for (i = 0; i < 6; i++)
	{
		if (isRegistered[i] == 1 && ips[i] != "")
		{
			rpc::client cln(ips[i], ports[i]);
			ack = cln.call("election", myid, lamport++).as<int>();
			if (ack == 0)
			{
				break;
			}
		}
	}
	
	int lid = myid;
	
	if (ack == 0)
	{
		lid = i;
	}
	else
	{
		cout << " I am the leader..." << endl;
	}
	
	for (int j = 0; j < 6; j++)
	{
        rpc::client cln(ips[j], ports[j]);
        (void) cln.call("leader", lid, lamport++);
	}

    return SUCCESS;
}

void leader(int lid)
{
	lead = lid;
}

int election(int id)
{
	int myid = GATEWAY;
	
	cout << "Leader Election in Progress.." << endl;
	
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
 * FUNCTION: main
 * DESCRIPTION: Main Function
 *
 * @params: none
 * @returns: status of execution
 */
STATUS main()
{
    STATUS status = SUCCESS;

    /* New strings for sensor and device IPs */
    ips = new  string[6];
    void *result_ptr = NULL;

    /* pthreads */
    pthread_t user_task;
    pthread_t bulb_manage;
    pthread_t heat_manage;
	pthread_t berkeley_manage;

    /* Lock the Motion Sensor and Automation */
    motion_lock.lock();
    start_lock.lock();
    test_lock.lock();

 /* Print Header */
    printHeader();

    cout << "GATEWAY: Waiting for Sensors and Devices...";

    fflush(stdout);

    /* Upon SIGALRM, call Update_Temperature() */
    if (signal(SIGALRM, (void (*)(int)) TimerExpired)== SIG_ERR)
    {
        cout <<"Unable to catch SIGALRM \n";
        //status = ERR_TIMER_CREATION_FAILED;
    }

    /* Initialize the RPC server */
    rpc::server srv(8080);

#ifdef DEBUG
    cout << "Running Server " <<endl;
#endif

	/* Bind the leader election calls */
    srv.bind("lelection", &lelection);
    srv.bind("election", &election);
    srv.bind("leader", &leader);
	
    /* Bind the Register Function */
    srv.bind("registerf", []( string ltype,  string lname,  string IP, int port, int r_lamport)
    {
#ifdef DEBUG
        cout << "registerf called " <<endl;
#endif
		/* Adjust Lamport's Logical Clock */
		if (lamport++ <= r_lamport)
			lamport = r_lamport + 1;
		
        int devid = -1007;

        for (int i = 0; i < 6; i++)
        {
           if (lname.compare(name[i]) == 0)
           {
               if (isRegistered[i] == 0)
               {
                   /* Store the device/sensor information */
                   isRegistered[i] = 1;
                   ips[i] = IP;
                   ports[i] = port;
#ifdef DEBUG
                   printstuff();
#endif
                   /* Add the number of sensors/devices registered */
                   devs_reg++;

                   /* Assign and return the ID to the device */
                   devid = i;
               }
               else
               {
                   /* Device already registered, just reutn the same ID */
                   devid = i;
               }
           }
           if (devs_reg == 6)
           {
                /* Let's test the smart home */
                test_lock.unlock();
#ifdef DEBUG
                cout << "Starting Home " <<endl;
#endif
                UpdateTimer(HOMETIMER);
           }
        }

        return devid;
    });

    /* Bind the report_state Function */
    srv.bind("report_state", [](int device_id, bool state, int r_lamport)
    {
		void (*prev_handler)(int);
#ifdef DEBUG
        cout << "Motion Detected: " << device_id << endl;
#endif
		/* Adjust Lamport's Logical Clock */
		if (lamport++ <= r_lamport)
			lamport = r_lamport + 1;
		
        if (device_id == MOTION)
        {
            DisableTimer(); /* Stop the timer */
            motion_lock.unlock(); /* Detect the motion */
        }
        if (device_id == KEY)
        {
            /* User authenticated. Switch to HOME mode */
            cout << "Auth OK" <<endl;
            if (mode == AWAY)
            {
				/* Disarm the intruder alert */
				//signal(SIGINT, SIG_DFL);
                //cout << "HOME MODE" <<endl;
                //change_mode(HOME);
                authenticated = true;
            }
        }
        if (device_id == DOOR)
        {
            doorstatus = state;

			cout << "Door state = " << doorstatus << endl;
            
            /* Door was opened and closed. */
            if (state == false)
            {
                if (mode == HOME)
                {
					 /* Arm the intruder alert */
					prev_handler = signal(SIGINT, text_message);
                    change_mode(AWAY);
                    authenticated = false;
				    cout << "AWAY MODE" <<endl;
					cout << "Auth Disable" <<endl;
                }
	            if (mode == AWAY && authenticated == true)
                {
					/* Disarm the intruder alert */
					signal(SIGINT, SIG_DFL);
					cout << "HOME MODE" <<endl;
					change_mode(HOME);
				}

            }
        }
		
		/* Log the event into database */
		rpc::client cln(ips[device_id], ports[device_id]);
		long long timeStamp = cln.call("request_timestamp", device_id, lamport++).as<long long>();
		int l_lamport = cln.call("request_lamport").as<int>();
		if (lamport++ <= l_lamport)
			lamport = l_lamport + 1;
		setState(device_id, (int)state);
		logEntry(timeStamp, lamport, device_id, (int)state, " ");
    });

    /* Run the server loop */
    srv.async_run(4);

 /* Run system test */
 
    test_lock.lock();
    status = test_system();
	test_lock.unlock();
    if (status == SUCCESS)
    {
		/* All set. Let's run the leader election */
		status = lelection();
        /* All set. Let's start the Smart Home */
        start_lock.unlock();
    }
	
	/* Create task to synchronize time using berkeley algorithm */
	if (status == SUCCESS)
    {
        status = pthread_create(&berkeley_manage, NULL, &berkeleyTime, NULL);
		
		if (status != SUCCESS)
		{
			printf("Error: berkeley_manage Task Creation Failed\nABORT!!\n\n");
		}
    }
	
    /* Create User Task */
    if (status == SUCCESS)
    {
        status = pthread_create(&user_task, NULL, &UserEntry, NULL);

        if (status != SUCCESS)
        {
            printf("Error: Task Creation Failed\nABORT!!\n\n");
           // status = ERR_THREAD_CREATION_FAILED;
        }
	}

    /* Create Bulb Management Task */
    if (status == SUCCESS)
    {
        status = pthread_create(&bulb_manage, NULL, &BulbManage, NULL);

        if (status != SUCCESS)
        {
            printf("Error: Task Creation Failed\nABORT!!\n\n");
            //status = ERR_THREAD_CREATION_FAILED;
        }
    }

    /* Create Outlet Management Task */
    if (status == SUCCESS)
    {
        status = pthread_create(&heat_manage, NULL, &HeatManage, NULL);

        if (status != SUCCESS)
        {
            printf("Error: TSensor Task Creation Failed\nABORT!!\n\n");
            //status = ERR_THREAD_CREATION_FAILED;
        }
    }

    /* All set, let the threads run */
    if (status == SUCCESS)
    {
        pthread_join(user_task, &result_ptr);
        pthread_join(bulb_manage, &result_ptr);
        pthread_join(heat_manage, &result_ptr);
		pthread_join(berkeley_manage, &result_ptr);

        pthread_exit(NULL);
    }

    return status;
}
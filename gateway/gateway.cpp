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

#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <mutex>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include "rpc/server.h"
#include "rpc/client.h"

#define TEMP           0
#define MOTION         1
#define BULB           2
#define OUTLET         3

#define SUCCESS        0
#define FAILURE        1

#define OFF            0
#define ON             1

#define HOMETIMER      2000
#define TEMPSCALE      1000

typedef int STATUS;
typedef unsigned int MODE;
typedef struct itimerval TIMER;
typedef std::mutex LOCK;

#define HOME           1 << 0
#define AWAY           1 << 1
#define EXIT           1 << 2

using namespace std;



void change_mode(int inmode);
int registerf(std::string ltype, std::string lname);

std::string name[4] = {"temp", "motion", "bulb", "outlet"};
std::string type[4] = {"sensor", "sensor", "device", "device"};
int devs_reg = 0;
std::string *ips;
int ports[4] = {0};
static int isRegistered[4] = {0};
TIMER motion_timer;
float temperature = 1.0;
int states[2] = {OFF, OFF};


LOCK motion_lock;
LOCK bulb_lock;
LOCK start_lock;
LOCK mode_lock;

int mode = HOME;


void printstuff()
{
    std::cout << "Registered: ";
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
        case TEMP:
            if (isRegistered[0] == 1 && ips[0] != "")
            {
                rpc::client temp(ips[0], ports[0]);
                value = temp.call("query_state", device_id).as<long long>();
            }
            break;
        case MOTION:
            if (isRegistered[1] == 1 && ips[1] != "")
            {
                rpc::client motion(ips[1], ports[1]);
                value = motion.call("query_state", device_id).as<long long>();
            }
            break;
        case BULB:
            if (isRegistered[2] == 1 && ips[2] != "")
            {
                rpc::client bulb(ips[2], ports[2]);
                value = bulb.call("query_state", device_id).as<long long>();
            }
            break;
        case OUTLET:
            if (isRegistered[3] == 1 && ips[3] != "")
            {
                rpc::client outlet(ips[3], ports[3]);
                value = outlet.call("query_state", device_id).as<long long>();
            }
            break;
        default:
            value = 0;
            break;
    }
    return value;
}

void UpdateTimer(int msecs)
{
    STATUS status = SUCCESS;

    motion_timer.it_value.tv_sec = msecs / 1000;
    motion_timer.it_value.tv_usec = (msecs * 1000) % 1000000;
    motion_timer.it_interval = motion_timer.it_value;

    if (setitimer(ITIMER_REAL, &motion_timer, NULL) == -1)
    {
        cout << "Timer Update Failed !\n";
    }

    return ;    
}

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


int change_state(int device_id, int state)
{
    int ack = FAILURE;
    
    if (isRegistered[device_id] == 1 && ips[device_id] != "")
    {
        rpc::client cln(ips[device_id], ports[device_id]);
        ack = cln.call("change_state", device_id, state).as<int>();
        
        if (ack == SUCCESS)
            states[device_id - 2] = state;
    }
    return ack;
}

void TimerExpired()
{
    bulb_lock.lock();

#ifdef DEBUG
    cout << "Timer Expired " << endl;
#endif

    if (states[BULB-2] == ON)
    {
        change_state(BULB, OFF);
    }

    bulb_lock.unlock();
}

void text_message(int sig)
{
    cout << endl << "    !!!INTRUDER ALERT !!!\n Info: Sensed motion in the house" << endl;
    return;
}

void *UserEntry(void *arg)
{
    /* Wait for Smart Home initialization */
	start_lock.lock();
    start_lock.unlock();

    int lmode = HOME;

    /* Register the Intruder alert interrupt */
	void (*prev_handler)(int);
	prev_handler = signal(SIGINT, text_message);

    while (true)
    {
        cout << endl << "Set the Mode \\> ";
        cin >> lmode;
        if (lmode >= HOME && lmode <= AWAY)
        {
            change_mode(lmode);
        }
    }

    return NULL;
}

void *HeatManage(void *arg)
{
    start_lock.lock();
    start_lock.unlock();
    
    long long temp_and_id = -1;
    int device_id = -1;

    while (true)
    {
        sleep(5);
        temp_and_id = query_state(TEMP);
        device_id = (int)(temp_and_id >> 32);
        temperature = (float)(temp_and_id & 0xFFFFFFFF);

        //cout << "Temperature Reported: "<< temperature << endl;
    
        if (device_id == TEMP)
        {
            if (temperature < 1.0 * TEMPSCALE)
            {
                if (states[OUTLET-2] == OFF)
                {
                    change_state(OUTLET, ON);
                }
            }
    
            if (temperature > 2.0 * TEMPSCALE)
            {
                if (states[OUTLET-2] == ON)
                {
                    change_state(OUTLET, OFF);
                }
            }
        }
    }
    
    return NULL;
}

void *BulbManage(void *arg)
{
    start_lock.lock();
    start_lock.unlock();
    
    while (true)
    {
        motion_lock.lock();
        bulb_lock.lock();
        mode_lock.lock();
        if (isRegistered[BULB] == 1)
        {
                switch (mode)
                {
                    case HOME:
                        if (states[BULB-2]==OFF)
                        {
                            change_state(BULB, ON);
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

        mode_lock.unlock();
        bulb_lock.unlock();
    }
    
    return NULL;
}


void change_mode(int lmode)
{
    mode_lock.lock();

    if (lmode != mode)
    {
        mode = lmode;
        rpc::client cln(ips[MOTION], ports[MOTION]);
        cln.call("change_mode", mode);
    
        if (mode == HOME)
        {
    	    UpdateTimer(HOMETIMER);
        }
        else if (mode == AWAY)
        {
            DisableTimer();
        }
    }

    mode_lock.unlock();
}

STATUS main() 
{
    STATUS status = SUCCESS;

    ips = new std::string[4];
    void *result_ptr = NULL;
        
    pthread_t user_task;
    pthread_t bulb_manage;
    pthread_t heat_manage;
    
    /* Lock the Motion Sensor and Automation */
    motion_lock.lock();
    start_lock.lock();

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

    /* Bind the Register Function */
    srv.bind("registerf", [](std::string ltype, std::string lname, std::string IP, int port) 
    {
#ifdef DEBUG
        cout << "registerf called " <<endl;
#endif
        int devid = 4;

        for (int i = 0; i < 4; i++)
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
           if (devs_reg == 4)
           {
                // Let's start the smart home
                start_lock.unlock();
#ifdef DEBUG
                cout << "Starting Home " <<endl;
#endif
                UpdateTimer(HOMETIMER);
           }
        }
        
        return devid;
    });

    /* Bind the report_state Function */
    srv.bind("report_state", [](int device_id, bool state)
    {
#ifdef DEBUG
        cout << "Motion Detected: " << device_id << endl;
#endif
        if (device_id == MOTION)
        {
            DisableTimer(); // stop the timer
            motion_lock.unlock(); // update
        }
    });

    /* Run the server loop */
    srv.async_run(4);
    
    /* Create pthreads */
    if (status == SUCCESS)
    { 
        status = pthread_create(&user_task, NULL, &UserEntry, NULL);

        if (status != SUCCESS)
        {
            printf("Error: Task Creation Failed\nABORT!!\n\n");
           // status = ERR_THREAD_CREATION_FAILED;
        }
	}
    
    if (status == SUCCESS)
    { 
        status = pthread_create(&bulb_manage, NULL, &BulbManage, NULL);

        if (status != SUCCESS)
        {
            printf("Error: Task Creation Failed\nABORT!!\n\n");
            //status = ERR_THREAD_CREATION_FAILED;
        }
    }
	
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
    
        pthread_exit(NULL);
    }

    return status;
}

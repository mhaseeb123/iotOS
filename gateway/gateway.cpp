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

#define BULBON         1
#define BULBOFF        0

#define OFF            0
#define ON             1

typedef int STATUS;
typedef unsigned int MODE;
typedef struct itimerval TIMER;
typedef std::mutex LOCK;

#define HOME           1 << 0
#define AWAY           1 << 1
#define EXIT           1 << 2

using namespace std;

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


int mode = HOME;

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
    change_state(BULB, BULBOFF);
    bulb_lock.unlock();
}

void *UserEntry(void *arg)
{
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
        sleep(3);
        device_id = (int)(temp_and_id >> 32);
        temp_and_id = query_state(TEMP);
        temperature = (float)(temp_and_id & 0xFFFFFFFF);
    
        if (device_id == TEMP)
        {
            if (temperature < 1.0)
            {
                change_state(OUTLET, true);
            }
    
            if (temperature > 2.0)
            {
                change_state(OUTLET, false);
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
        
        if (isRegistered[BULB] == 1)
        {
            switch (mode)
            {
                case HOME:
                    change_state(BULB, BULBON);
                    UpdateTimer(10000); // Reset the timer
                    break;
                    
                case AWAY:
                    //OMG. HELP. Intruder in the house.
                    DisableTimer(); // Keep the timer off.
                    break;
                default:
                    break;
            }
        }

        bulb_lock.unlock();
    }
    
    return NULL;
}


int change_mode(int inmode)
{
    mode = inmode;
}

int main() 
{    
    ips = new std::string[4];
    
    void *result_ptr = NULL;
    
    motion_lock.lock();
    start_lock.lock();
    
    pthread_t user_task;
    pthread_t bulb_manage;
    pthread_t heat_manage;
    
    /* Upon SIGALRM, call Update_Temperature() */
    if (signal(SIGALRM, (void (*)(int)) TimerExpired)== SIG_ERR)
    {
        cout <<"Unable to catch SIGALRM \n";
    }
        
    rpc::server srv(8080);
    
    cout << "Running Server " <<endl;

    srv.bind("registerf", [](std::string ltype, std::string lname, std::string IP, int port) 
    {
        cout << "registerf called " <<endl;
        int devid = 4;
        
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
                      devs_reg++;
                   devid = i;
               }
               else
               {
                     devid = i;
               }
           }
           
           if (devs_reg == 4)
           {
                // Let's start the smart home
                start_lock.unlock();
                cout << "Starting Home " <<endl;
                UpdateTimer(10000);
           }
        }
        
        return devid;
    });

    //report_state
    srv.bind("report_state", [](int device_id, bool state) {
        cout << "Motion Detected: " << device_id << endl;
        
        if (device_id == MOTION)
        {
            DisableTimer(); // stop the timer
            motion_lock.unlock(); // update
        }
    });

    // Run the server loop.
    srv.async_run(4);
    
    pthread_create(&user_task, NULL, &UserEntry, NULL);
    pthread_create(&bulb_manage, NULL, &BulbManage, NULL);
    pthread_create(&heat_manage, NULL, &HeatManage, NULL);
    
    pthread_join(user_task, &result_ptr);
    pthread_join(bulb_manage, &result_ptr);
    pthread_join(heat_manage, &result_ptr);
    
    pthread_exit(NULL);

    return 0;
}

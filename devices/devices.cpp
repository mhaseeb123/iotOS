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

string devices_ip;
int devices_port = 7070;

bool bulb = false;
bool outlet = false;

int bulbid = ERR_SENSOR_NOT_REGISTERED;
int outletid = ERR_SENSOR_NOT_REGISTERED;

LOCK bulb_lock;
LOCK outlet_lock;

// This functions from: https://www.geeksforgeeks.org/c-program-display-hostname-ip-address/
string getIPAddress()
{
    char hostbuffer[256];
    char *IPbuffer; 
    struct hostent *host_entry; 
    int hostname; 
  
    // To retrieve hostname 
    hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 

    if (hostname == -1) 
    { 
        perror("gethostname"); 
        exit(1); 
    }  

    // To retrieve host information 
    host_entry = gethostbyname(hostbuffer);

    if (host_entry == NULL) 
    { 
        perror("gethostbyname"); 
        exit(1); 
    }

    // To convert an Internet network 
    // address into ASCII string 
    IPbuffer = inet_ntoa(*((struct in_addr*) 
                           host_entry->h_addr_list[0]));

    string IP(IPbuffer);
    
    return IP;
} 

long long query_state(int device_id)
{
    int res = -1;

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
        res = -1.0;
    }

    /* Mux device_id */

    long long query = (long long) res;
    query |= ((long long) device_id << 32);

    return query;
}

STATUS change_state(int device_id, bool new_state)
{
    STATUS status = SUCCESS;

    if (device_id == bulbid)
    {
        bulb_lock.lock();
        bulb = new_state;
        bulb_lock.unlock();
    }
    else if (device_id == outletid)
    {
        outlet_lock.lock();
        outlet = new_state;
        outlet_lock.unlock();
    }
    else
    {
        status = ERR_INVLD_ARGS;
    }

    return status;
}
/* Main Function */
STATUS main(int argc, char **argv)
{
    STATUS status = SUCCESS;
    void *result_ptr = NULL;

    pthread_t thread1;
    pthread_t thread2;

    /* Check if number of tosses passed as parameter */
    if (argc < 3)
    {
        cout << "ERROR: Gateway IP or Port Number not specified\n";
        cout << "USAGE: sensor gate.way.ip.addr port\n";

        status = ERR_INVLD_ARGS;
    }

    if (status == SUCCESS)
    {
        gateway_ip = argv[1];
        gateway_port = atoi(argv[2]);
    }

    if (status == SUCCESS)
    {
        /* Get your IP */
        devices_ip = getIPAddress();

        if (strcmp(devices_ip.c_str(), "") == 0)
        {
            status = ERR_INVLD_IP;
        }
    }

    // Creating a server that listens on port
    rpc::server srv(device_port);

    cout << "Registering calls \n";

    srv.bind("query_state", &query_state);
    srv.bind("change_state", &change_state);

    // Run the server loop with 2 threads
    srv.async_run(2);

    cout << "\nConnecting to Gateway...\n";

    // Connect to the gateway
    rpc::client client(gateway_ip, gateway_port);
    rpc::client::connection_state state = (rpc::client::connection_state) 1;

    /* Wait until connected */
    if (client.get_connection_state() != state)
    {
        cout << "Gateway Not Available.." << endl;
        return 0;
    }

    cout << "CONNECTED \n\n";
    cout << "Registering Smart Bulb...\n";

    msensor = client.call("registerf", "device", "bulb", sensor_ip, sensor_port).as<int>();

    if (msensor == ERR_SENSOR_NOT_REGISTERED)
    {
        cout << "Smart Bulb Registration Failed..\n";
        status = ERR_DEV_NOT_REGD;
    }

    if (status == SUCCESS)
    {
        cout << "SUCCESS \n";
        
        cout << "Registering Smart Outlet...\n";

        tsensor = client.call("registerf", "device", "outlet", sensor_ip, sensor_port).as<int>();

        if (tsensor == ERR_SENSOR_NOT_REGISTERED)
        {
            cout << "Smart Bulb Registration Failed..\n";
            status = ERR_DEV_NOT_REGD;
        }
    }

    if (status == SUCCESS)
    {
        cout << "SUCCESS \n";

        /* Loop forever */
        while (1)
        {
            sleep(1);
        }
    }

    return status;
}
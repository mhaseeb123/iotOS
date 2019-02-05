//============================================================================
// Name        : sensors.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "main.h"
using namespace std;

/* Global Variables */
string gateway_ip;
int gateway_port;
MODE mode = HOME;
bool motion = false;
float temp = 0.5;
float modeoffset = 0;

TIMER temp_timer; /* Timer for updating temperature */
TIMER motion_timer; /* Timer for motion updates */

int msensor = ERR_SENSOR_NOT_REGISTERED;
int tsensor = ERR_SENSOR_NOT_REGISTERED;

LOCK offset_lock;
LOCK motion_lock;

void change_mode(int inmode)
{
    offset_lock.lock();

    switch (inmode)
    {
        case HOME:
            mode = HOME;
            modeoffset = 0;
            break;

        case AWAY:
            /* Lock the update mutex */
            mode = AWAY;
            modeoffset = 10;
            /* Lock the update mutex */
            break;

        case EXIT:
            mode = EXIT;
            break;

        default:
            break;
    }

    offset_lock.unlock();
}

/*
 * FUNCTION: Random Number
 * DESCRIPTION: Returns a random number between
 -1.0 and 1.0
 *
 * @params: none
 * @returns: random number [0.0, 2.0]
 */
int Random_Number()
{
    return rand();
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
    rpc::client client(gateway_ip, gateway_port);

    msensor = client.call("registerf", "sensor", "temp").as<int>();

    if (msensor == ERR_SENSOR_NOT_REGISTERED)
    {
        return NULL;
    }

    tsensor = client.call("registerf", "sensor", "motion").as<int>();

    if (tsensor == ERR_SENSOR_NOT_REGISTERED)
    {
        return NULL;
    }

    while (true)
    {
        motion_lock.lock();

        /* Send data */
        bool val = (Random_Number() & 0x1) == 0 ? 1.0 : 0.0;

        cout << "report_state" << endl;

        //(void)client.call("report_state", msensor, val);
    }

    /* No results required */
    return NULL;
}

long long query_state(int device_id)
{
    float res = (float) Random_Number();

    if (device_id == tsensor)
    {
        offset_lock.lock();
        res = (res / (float) (RAND_MAX / 2)) + modeoffset;
        offset_lock.unlock();
    }
    else if (device_id == msensor)
    {
        res = (Random_Number() & 0x1) == 0 ? 1 : 0;
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

void Update()
{
    /* Release Mutex here */
    motion_lock.unlock();
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
    while (tsensor == ERR_SENSOR_NOT_REGISTERED || msensor == ERR_SENSOR_NOT_REGISTERED)
    {
        sleep(1);
    }

    // Creating a server that listens on port 8080
    rpc::server srv(6060);

    cout << "Registering calls \n";

    srv.bind("query_state", &query_state);
    srv.bind("change_mode", &change_mode);

    // Run the server loop.
    srv.run();

    /* No results required */
    return NULL;
}

/* Main Function */
STATUS main(int argc, char **argv)
{
    STATUS status = SUCCESS;
    void *result_ptr = NULL;

    pthread_t thread1;
    pthread_t thread2;

    /* Lock the msensor mutex */
    motion_lock.lock();

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
        /* Provide a random seed */
        srand((int) (time(0) * 23) % (1 + 5) + (10 + 1) * 29 - time(0) + (int) time(0) % (5 + 1) * time(0));

        /* Create Timers here */
        TIMER temp_timer; /* Timer for updating temperature */

        cout << "Main Task: Creating Timers\n\n";

        /* Upon SIGALRM, call Update_Temperature() */
        if (signal(SIGALRM, (void (*)(int)) Update)== SIG_ERR)
        {
            cout <<"Unable to catch SIGALRM \n";
            status = ERR_TIMER_CREATION_FAILED;
        }
    }

    if (status == SUCCESS)
    {
        temp_timer.it_value.tv_sec = TEMP_INTERVAL / 1000;
        temp_timer.it_value.tv_usec = (TEMP_INTERVAL * 1000) % 1000000;
        temp_timer.it_interval = temp_timer.it_value;

        if (setitimer(ITIMER_REAL, &temp_timer, NULL) == -1)
        {
            cout << "Timer Failed !\n";
            status = ERR_TIMER_CREATION_FAILED;
        }
    }

    /* Create Tasks (pthreads) */
    if (status == SUCCESS)
    {
        printf("Main Task: Creating MSensor Client Task\n");
        status = pthread_create(&thread2, NULL, &report_state, NULL);
    }
    else
    {
        printf("Error: MSensor Client Task Create failed\nABORT!!\n\n");
        status = ERR_THREAD_CREATION_FAILED;
    }

    /* Create Tasks (pthreads) */
    if (status == SUCCESS)
    {
        printf("Main Task: Creating Server Task\n");
        status = pthread_create(&thread1, NULL, &Server_Entry, NULL);
    }
    else
    {
        printf("Error: Server Task Create failed\nABORT!!\n\n");
        status = ERR_THREAD_CREATION_FAILED;
    }

    if (status == SUCCESS)
    {
        /* Sync : Wait for the threads to complete before exiting */
        pthread_join(thread2, &result_ptr);
        pthread_join(thread1, &result_ptr);

        /* All set - Exit now */
        pthread_exit(NULL);
    }

    return status;
}

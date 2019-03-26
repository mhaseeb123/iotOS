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
 
#include "dbmanager.h"
typedef mutex LOCK;
/* Database Filename */
const string filepath = "db";
const string filename = "db/iotdb.txt";

/* Keep track of last event to determine 
   if user entered or left the house */
int lastEvent = -1;

/* Don't let multiple threads mess with db */
LOCK db_lock;

vector<string> getList(string type)
{
	db_lock.lock();
	
	vector<string> sendev;
	if (type == "")
		return sendev;

	ifstream file(filename);

	if (!file.is_open())
		return sendev;

	string line;
	while (getline(file, line))
	{
		istringstream iss(line);
		string token;
		if (getline(iss, token, ' ') && token.compare(type))
		{
			while (getline(iss, token, ' '))
			{
				sendev.push_back(token);
			}
		}
	}
	db_lock.unlock();

	return sendev;
}

vector<string> getStates()
{
	db_lock.lock();
	vector<string> states;

	ifstream file(filename);

	if (!file.is_open())
	{
		printf("getState: Couldn't open the file.\n");
		return states;
	}

	string line;
	while (getline(file, line, '\n'))
	{
		istringstream iss(line);
		string token;
		if (getline(iss, token, '\t') && token.compare("states:") == 0)
		{
			while (getline(iss, token, '\t'))
			{
				states.push_back(token);
			}
			break;
		}
	}

	file.close();
	db_lock.unlock();
	
	return states;
}

//Keep a vector of states.
//Only update the entries that changed.
//Leave the rest unchanged.
//Pass the entire vector as input.
void setStates(vector<string> states)
{
	db_lock.lock();
	if (states.size() <= 0)
		return;

	string newStr;
	newStr = "states:";
	for (unsigned int i = 0; i < states.size(); i++)
	{
		newStr.append("\t");
		newStr.append(states[i]);
	}

	ifstream filein(filename); //File to read from
	ofstream fileout("db/fileout.txt"); //Temporary file
	if(!filein || !fileout)
	{
		cout << "Error opening files!" << endl;
		return;
	}

	string strTemp;
	string token;
	//bool found = false;
	while(getline(filein, strTemp))
	{
		istringstream iss(strTemp);
		getline(iss, token, '\t');
		if (token.compare("states:") == 0)
		{
			strTemp = newStr;
			//found = true;
		}
		strTemp += "\n";
		fileout << strTemp;
		//if(found) break;
	}

	remove(filename.c_str());
	rename("db/fileout.txt", filename.c_str());
	
	db_lock.unlock();
}

void setState(int device_id, int new_state)
{
	if (device_id > 5 || device_id < 0)
		return;
	
	string localvalue;
	switch (device_id)
	{
		case 0:
			localvalue = to_string(new_state);
			break;
			
		case 1:
			localvalue = (new_state == 0) ? "Undetected" : "Detected";
			break;
		
		case 2:
			localvalue = (new_state == 0) ? "Closed" : (new_state == 1) ? "Open" : "Weird State";
			break;
			
		case 3:
			localvalue = (new_state == 0) ? "Unauthenticated" : "Authenticated";
			break;
			
		case 4:
			localvalue = (new_state == 0) ? "OFF" : "ON";
			break;
			
		case 5:
			localvalue = (new_state == 0) ? "OFF" : "ON";
			break;
			
		default:
			localvalue = "INVISIBLE";
	}
	
	vector<string> temp_states = getStates();
	temp_states[device_id] = localvalue;
	setStates(temp_states);
}

//get the last count lines of log file
string* getHistory(int count)
{
	db_lock.lock();
	string *history = new string[count];

	ifstream file(filename);

	if (!file.is_open())
		return history;

	string line;
	int i = 0;
	while (getline(file, line))
	{
		history[i++ % count] = line;
	}

	file.close();
	db_lock.unlock();

	return history;
}

void logEntry(long long timeStamp, int lamport, int device_id, int value, string comment)
{
	if (device_id > 5 || device_id < 0)
		return;
	
	string localsendev;
	string localvalue;
	switch (device_id)
	{
		case 0:
			localsendev = "TEMP";
			localvalue = to_string(value);
			break;
		
		case 1:
			localsendev = "MOTION";
			localvalue = (value == 0) ? "Undetected" : "Detected";
			
			if (value > 0)
			{
				if ( lastEvent == DOOR_OPENED || lastEvent == DOOR_CLOSED)
				{
					comment = "User entered the house.";
				}
				lastEvent = MOTION_DETECTED;
			}
			break;
		
		case 2:
			localsendev = "DOOR";
			localvalue = (value == 0) ? "Closed" : (value == 1) ? "Open" : "Weird State";
			
			if (value == 0)
				lastEvent = DOOR_CLOSED;
			
			if (value == 1)
			{
				if (lastEvent == MOTION_DETECTED)
					comment = "User left the house.";
				
				lastEvent = DOOR_OPENED;
			}
			break;
			
		case 3:
			localsendev = "KEY";
			localvalue = (value == 0) ? "Unauthenticated" : "Authenticated";
			break;
			
		case 4:
			localsendev = "BULB";
			localvalue = (value == 0) ? "OFF" : "ON";
			break;
			
		case 5:
			localsendev = "OUTLET";
			localvalue = (value == 0) ? "OFF" : "ON";
			break;
			
		default:
			localsendev = "GHOST";
			localvalue = "INVISIBLE";
	}
	
	vector<string> temp_history;
	temp_history.push_back(to_string(timeStamp));
	temp_history.push_back(to_string(lamport));
	temp_history.push_back(localsendev);
	temp_history.push_back(localvalue);
	temp_history.push_back(comment);
	
	log(temp_history);
}
void log(vector<string> history)
{
	db_lock.lock();
	if (history.size() <= 0)
		return;

	string newStr;
	for (unsigned int i = 0; i < history.size(); i++)
	{
		newStr.append(history[i]);
		newStr.append("\t");
	}

	ofstream ofs;
	ofs.open (filename, std::ofstream::out | std::ofstream::app);

	ofs << newStr << "\n";

	ofs.close();
	db_lock.unlock();
}







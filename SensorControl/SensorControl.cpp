
//Author: Arttu Lehtonen, arttu.lehtonen@aalto.fi, Github: ArttuD

#include <iostream>
#include <string>
#include <vector>
#include "SerialClass.h"	// Library described above

#include <zmq.h>
#include <zmq_utils.h>


//Memory management
char incomingData[256] = ""; //storage to read line
int dataLength = 255; // Max size of received Data

char topic[10]; // topci of published data
char datatoSend[100]; // data to send

int readResult = 0; // size of incoming data

int start = 0; //Flag to skip the first unit that is often corrupted
int flag = 0; //Flag to read time

//data storages for B and time
std::string dataStorageB;
std::vector<float> dataVectorB;

std::string dataStorageTime;
std::vector<float> dataVectorTime;


int main()
{
	//Init message
	zmq_msg_t part1;
	zmq_msg_t part2;

	//Socket to talk with
	void* context = zmq_ctx_new();
	void* publisher = zmq_socket(context, ZMQ_PUB);
	int rc = zmq_bind(publisher, "tcp://130.233.164.32:5550");

	void* subscriber = zmq_socket(context, ZMQ_SUB);
	int Subrc = zmq_connect(subscriber, "tcp://130.233.164.32:5551");
	zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", NULL);

	
	//Connect and check if connection found
	Serial* SP = new Serial("\\\\.\\COM9"); //This should not be hard coded
	if (SP->IsConnected())
		printf("We're connected\n");

	//turn on current sources
	for (int i = 0; i < 3; ++i)
	{
		SP->WriteData("o", 1);
		Sleep(1);
	}
	int i = 0;
	std::cout << "waiting start" << std::endl;
	zmq_msg_t message;
	zmq_msg_init(&message);
	zmq_msg_recv(&message, subscriber, 0);
	zmq_msg_close(&message);
	zmq_close(subscriber);
	std::cout << "Reading" << std::endl;

	while (i<=650)
	{	
		//Read Arduino serial port
		readResult = SP->ReadData(incomingData, dataLength);
		
		// Write zero to the end of data 
		incomingData[readResult] = 0;

		//Process to unpack and discard corrupted packages
		for (int j = 0; j < readResult; ++j)
		{
			//First read elements related to time and after "7" change to B
			if (flag == 0 && incomingData[j] == '/') {
				flag = 1;
			}
			//Save time 
			else if (flag == 0) {
				dataStorageTime.push_back(incomingData[j]);
			}
			//Save to B until hit \r
			else if ((incomingData[j] != (char)'\n') && (incomingData[j] != (char)'\r') && (flag == 1)) {
				dataStorageB.push_back(incomingData[j]);
			}
			//save collected variables when faced "\n"
			else if ((incomingData[j] == (char)'\n') && (flag == 1)) {
				if (start > 0) {

					//Publish
					//std::cout << "publishing" << std::endl;
					//Topic message

					sprintf_s(topic, "%d\n", (int)0);
					zmq_msg_init_size(&part1, sizeof(topic));
					memcpy(zmq_msg_data(&part1), topic, sizeof(topic));

					//Data message
					sprintf_s(datatoSend, "%f,%f\n", std::stof(dataStorageB), std::stof(dataStorageTime));
					zmq_msg_init_size(&part2, sizeof(datatoSend));
					memcpy(zmq_msg_data(&part2), datatoSend, sizeof(datatoSend));

					//Save char to be published
					zmq_msg_send(&part1, publisher, ZMQ_SNDMORE);
					zmq_msg_send(&part2, publisher, 0);
					
					//close messages
					zmq_msg_close(&part2);
					zmq_msg_close(&part1);

					//Clear
					dataStorageB.clear();
					dataStorageTime.clear();
					break;
				}
				else if (start == 0) {

					// Skip the first corrupted package
					start += 1;
					dataStorageB.clear();
					dataStorageTime.clear();
					flag = 0;

				}
			}
		}
		++i;
		start = 0;
		flag = 0;
		//std::cout << "new Round" << std::endl;
		//wait 10 µs before next round
		Sleep(100);
	}
	
	//turn off the current sources
	for (int i = 0; i < 3; ++i) {
		SP->WriteData("q", 1);
		Sleep(1);
	}
	//close
	zmq_close(publisher);
	zmq_ctx_destroy(context);
	return 0;
}

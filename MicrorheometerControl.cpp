
// Author: Arttu Lehtonen, arttu.lehtonen@aalto.fi, Github: ArttuD

#include <iostream>
#include <chrono>
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <NIDAQmx.h>
#include "Header.hpp"

#include <zmq.h>
#include <zmq_utils.h>


// Error handler
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

//Define call backs
int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void* callbackData);
int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void* callbackData);

//Define Classes
PID feedback((double)0.1, (double)1.0, (double)1.0, (double)1.0, (double)1.0, (double)10.0);

/*
//void subscriberThred(void* pipe, float data1, float data2, float data3, float data4, float data5);
void PubThread(float data1, float data2, float data3, float data4, float data5);
*/

//Functions
void generateSinCurrent(double offset, double gradient, double* downData, int sampleFrequency);

// NI constants
int32       error = 0;
TaskHandle  taskHandleRead = 0;
TaskHandle  taskHandleWrite = 0;
char        errBuff[2048] = { '\0' };

double mValue;
double results;
int sFreq = 1000;

double* datap;
double* resultsp = &results;
double* mValuep = &mValue;
double	readData[1000];

int iterRounds = 0;
char topic[10];
char datatoSend[100];

void* publisher;
void* subscriber;
void* Pubcontext;

int doneFlag = 0;

//constants
#define PI	3.1415926535



int main(void)
{
	std::cout << "Init System...";

	Pubcontext = zmq_ctx_new();
	publisher = zmq_socket(Pubcontext, ZMQ_PUB);
	int Pubrc = zmq_bind(publisher, "tcp://130.233.164.32:5552");

	subscriber = zmq_socket(Pubcontext, ZMQ_SUB);
	int Subrc = zmq_connect(subscriber, "tcp://130.233.164.32:5551");
	zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", NULL);
	std::cout << "waiting start" << std::endl;


	//Recerve memory for variables
	datap = new double[60 * sFreq]; //Current sequency

	//Generate current sequence
	generateSinCurrent((double)0.75, (double)1.25, datap, sFreq);

	
	auto begin = std::chrono::high_resolution_clock::now();

	void* Pubcontext = zmq_ctx_new();

	//Task preparation: 

	// DAQmx Configure Code
	DAQmxErrChk(DAQmxCreateTask("", &taskHandleWrite)); //Analog write
	DAQmxErrChk(DAQmxCreateTask("", &taskHandleRead)); //Analog read

	// configure channels
	DAQmxErrChk(DAQmxCreateAOVoltageChan(taskHandleWrite, "Dev1/ao0", "", -10.0, 10.0, DAQmx_Val_Volts, NULL));

	DAQmxErrChk(DAQmxCreateAIVoltageChan(taskHandleRead, "Dev1/ai0", "", DAQmx_Val_Diff, -10.0, 10.0, DAQmx_Val_Volts, NULL));

	//time input
	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleRead, NULL, 30000.0, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 30000*60));

	//Send to processor that system is ready
	std::cout << "Ready to start the task" << std::endl;

	//Sample at 10kHz but process 1k Hz, shutdown when ready
	DAQmxErrChk(DAQmxRegisterEveryNSamplesEvent(taskHandleRead, DAQmx_Val_Acquired_Into_Buffer, 30, 0, EveryNCallback, NULL));
	
	DAQmxErrChk(DAQmxRegisterDoneEvent(taskHandleRead, 0, DoneCallback, NULL));
	
	std::cout << "waiting start" << std::endl;
	zmq_msg_t message;
	zmq_msg_init(&message);
	zmq_msg_recv(&message, subscriber, 0);
	zmq_msg_close(&message);
	//Start tasks
	DAQmxErrChk(DAQmxStartTask(taskHandleWrite));
	DAQmxErrChk(DAQmxStartTask(taskHandleRead));
	
	//Wait until done
	printf("Processing press any key to interupt\n");
	std::getchar();

	if (doneFlag == 1)
	{
		return 0;
	}
		

Error:
	std::cout << "Error occured!" << std::endl;
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandleRead != 0 || taskHandleWrite != 0) {
		
		delete[] datap;
		delete resultsp;
		delete mValuep;
		DAQmxStopTask(taskHandleWrite);
		DAQmxClearTask(taskHandleWrite);
		DAQmxStopTask(taskHandleRead);
		DAQmxClearTask(taskHandleRead);
	}
	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	printf("End of program, press Enter key to quit\n");
	return 0;
}


int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void* callbackData)
{
	int32   error = 0;
	char    errBuff[2048] = { '\0' };

	std::cout << "Task Done! \nClose by pressing any key" << std::endl;

	DAQmxStopTask(taskHandleWrite);
	DAQmxStopTask(taskHandleRead);
	DAQmxClearTask(taskHandleRead);
	DAQmxClearTask(taskHandleWrite);


	delete[] datap;
	delete resultsp;
	delete mValuep;
	
	doneFlag = 1;

Error:
	if (DAQmxFailed(error)) {
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
		DAQmxClearTask(taskHandle);
		printf("DAQmx Error: %s\n", errBuff);
	}
	return 0;
}

int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void* callbackData)
{
	int32       error = 0;
	char        errBuff[2048] = { '\0' };
	int32       read = 0;
	float64		average = 0;
	float time;
	zmq_msg_t part1;
	zmq_msg_t part2;
	
	double valuetoSave = *(datap + iterRounds);

	// DAQmx Read Code
	DAQmxErrChk(DAQmxReadAnalogF64(taskHandle, 30, 0, DAQmx_Val_GroupByChannel, readData, 1000, &read, NULL));

	time = (float)iterRounds * 1 / 1000;

	// Calc average
	for (int i = 0; i < read; ++i) {
		average += readData[i];
	}

	*mValuep = average / (float)read;

	//PID
	*resultsp = feedback.update((datap + iterRounds), mValuep);

	//Write value
	DAQmxErrChk(DAQmxWriteAnalogScalarF64(taskHandleWrite, 1, -1, *resultsp, NULL));
	
	
	//Publish
	sprintf_s(topic, "%d\n", (int)2);
	zmq_msg_init_size(&part1, sizeof(topic));
	memcpy(zmq_msg_data(&part1), topic, sizeof(topic));


	sprintf_s(datatoSend, "%f,%f,%f,%f\n", *(datap + iterRounds),*mValuep,*resultsp, time);
	zmq_msg_init_size(&part2, sizeof(datatoSend));
	memcpy(zmq_msg_data(&part2),datatoSend, sizeof(datatoSend));


	//Save char to be published
	zmq_msg_send(&part1,publisher, ZMQ_SNDMORE);
	//myfile << dataToSend;
	zmq_msg_send(&part2, publisher, 0);
	zmq_msg_close(&part2);
	zmq_msg_close(&part1);

	++iterRounds;

Error:
	if (DAQmxFailed(error)) {
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);
		printf("DAQmx Error: %s\n", errBuff);
	}
	return 0;
}


void generateSinCurrent(double offset, double gradient, double* downData, int sampleFrequency) {
	//Coil 1 or 2 
	double peak;
	if (gradient < 0.0) {
		peak = (double)-0.5;
	}
	else {
		peak = (double)2.0;
	}

	int numberOfSamples = 60 * sampleFrequency;

	//Generate current sequency
	for (int i = 0; i <= numberOfSamples; ++i) {
		// Calibration peaks
		if (i < 10 * sampleFrequency) {
			if (i > 5 * sampleFrequency && i < 5.5 * sampleFrequency) {
				*(downData + i) = peak;
			}
			else if (i > 6 * sampleFrequency && i < 6.5 * sampleFrequency) {
				*(downData + i) = peak;
			}
			else if (i > 7 * sampleFrequency && i < 7.5 * sampleFrequency) {
				*(downData + i) = peak;
			}
			else {
				*(downData + i) = (double)0.;
			}
			//Magnetization time 10 -> 20s
		}
		else if (i < 10 * sampleFrequency) {
			*(downData + i) = 0.;
		}
		else if (i < 20 * sampleFrequency) {
			*(downData + i) = offset;
		}
		//Sinusoidal period
		else if (i < 50 * sampleFrequency) {
			*(downData + i) = offset + gradient * std::sin(0.05 * 2 * PI * ((double)i - 20 * (double)sampleFrequency) * 1 / sampleFrequency);
		}
		//Calibration peaks 
		else if (i < 60 * sampleFrequency) {
			if (i > 55 * sampleFrequency && i < 55.5 * sampleFrequency) {
				*(downData + i) = peak;
			}
			else if (i > 56 * sampleFrequency && i < 56.5 * sampleFrequency) {
				*(downData + i) = peak;
			}
			else if (i > 57 * sampleFrequency && i < 57.5 * sampleFrequency) {
				*(downData + i) = peak;
			}
			else {
				*(downData + i) = (double)0.;
			}
		}
		else {
			*(downData + i) = (double)0.;
		}
	}
}

/*
void PubThread(float data1, float data2, float data3, float data4, float data5)
{

	void* publisher = zmq_socket(Pubcontext, ZMQ_PUB);
	int Pubrc = zmq_connect(Pubcontext, "tcp://localhost:5556");

	sprintf_s(dataPub, "%s/%f/%f/%f/%f\n", 1, data1, data2, data3, data4, data5);
	zmq_send(publisher, dataPub, sizeof(dataPub), 0);
}
*/
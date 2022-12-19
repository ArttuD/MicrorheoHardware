
#include <iostream>
#include <vector>
#include <cmath>

#include <zmq.h>
#include <zmq_utils.h>

#include <pylon/AviCompressionOptions.h>
#include <pylon/PylonIncludes.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <inttypes.h>
#include "opencv2/opencv.hpp"
#include <fstream>

#define __STDC_FORMAT_MACROS

// Struct to transfer coordinates
struct trackInfo
{
	int x;
	int y;
};

//Global to communicate over functions
cv::Point oldCenter(-1,-1);
trackInfo dataSend;

std::vector<cv::Point> points;
std::vector<std::vector<cv::Point> > contours;
std::vector<cv::Vec4i> hierarchy;

//Number of current loop 
int iRound = -1;
int clicks = 0; //Clicks to limit call back
int yMax; //Frame edge
int xMax; //Frame edge

char topic[10];
char datatoSend[100];

cv::VideoWriter writer;

//functions
void mouse_callback(int  event, int  x, int  y, int  flag, void* param);
void tracker(cv::Mat frame); //track particle return {x,y}

int main()
{	
	//Pylon related parameters used in Main
	Pylon::CGrabResultPtr ptrGrabResults;
	Pylon::CPylonImage pylonImage;
	Pylon::CImageFormatConverter converter;
	int64 timeStamp;
	int totalSkipped = 0; //Skipped image counter
	zmq_msg_t part1;
	zmq_msg_t part2;
	/*
	place holder to save
	std::ofstream myfile;
	myfile.open("C:/Users/lehtona6/projects/Own_projects/ArttuWork/HardwareMain/CameraControl/example.csv");
	myfile << "x,y,time";
	*/

	void* context = zmq_ctx_new();
	void* publisher = zmq_socket(context, ZMQ_PUB);
	int Pubrc = zmq_bind(publisher, "tcp://130.233.164.32:5551");
	
	
	//Open Window to config parameters
	Pylon::PylonInitialize();

	try
	{
		//Create camera object
		Pylon::CInstantCamera camera(Pylon::CTlFactory::GetInstance().CreateFirstDevice());
		std::cout << "Using device " << camera.GetDeviceInfo().GetModelName() << std::endl;

		//Open Camera
		camera.Open();
		std::cout << "Succesfully opened camera!" << std::endl;
		
		camera.MaxNumBuffer = 200;
		camera.OutputQueueSize = 200;

		//Get Camera parameter nodes and enable frame rate tuning
		GenApi::INodeMap &nodeMap = camera.GetNodeMap();
		Pylon::CBooleanParameter(nodeMap, "AcquisitionFrameRateEnable").SetValue(true);

		//get camera parameters
		Pylon::CFloatParameter gainVal(nodeMap, "Gain");
		Pylon::CFloatParameter exposureVal(nodeMap, "ExposureTime");
		Pylon::CIntegerParameter width(nodeMap, "Width");
		Pylon::CIntegerParameter height(nodeMap, "Height");
		Pylon::CEnumParameter pixelFormat(nodeMap, "PixelFormat");
		Pylon::CPixelTypeMapper pixelTypeMapper(&pixelFormat);
		Pylon::CFloatParameter frameR(nodeMap, "AcquisitionFrameRate");
		
		//Set configuration to crop
		frameR.TrySetValue(5.);
		std::cout << "----" << std::endl;
		std::cout << frameR.GetValue(true, true) << std::endl;
		std::cout << "----"  << std::endl;
		gainVal.TrySetValue(0.);
		std::cout << gainVal.GetValue() << std::endl;
		std::cout << "----" << std::endl;
		exposureVal.TrySetValue(15000.);
		std::cout << exposureVal.GetValue() << std::endl;
		std::cout << "----" << std::endl;


		//Set pixel type to color or mono
		if (pixelFormat.GetValue() == "Mono8")
		{
			Pylon::EPixelType aviPixelType = Pylon::PixelType_Mono8;
			converter.OutputPixelFormat = Pylon::PixelType_Mono8;
		}
		else {
			Pylon::EPixelType aviPixelType = Pylon::PixelType_BGR8packed;
			converter.OutputPixelFormat = Pylon::PixelType_BGR8packed;
		}
		
		//Create sliders for tuning
		int frameRateSlider = (int)frameR.GetValue(true, true);
		int exposureSlider = (int)exposureVal.GetValue(true, true);
		int gainSlider = (int)gainVal.GetValue(true, true);

		cv::namedWindow("Trackbars", cv::WINDOW_AUTOSIZE);
		cv::createTrackbar("Frame Rate", "Trackbars", &frameRateSlider, 55);
		cv::createTrackbar("Exposure", "Trackbars", &exposureSlider, 100000);
		cv::createTrackbar("Gain", "Trackbars", &gainSlider, 24);

		std::cout << "Please, tune the image" << std::endl;

		cv::Mat imgM;
		cv::Mat reImgM;
		cv::Mat imgMCopy;
		Pylon::CGrabResultPtr ptrGrabResults;
		Pylon::CPylonImage pylonImage;
		
		char key;

		camera.StartGrabbing(Pylon::GrabStrategy_LatestImages);
		
		while (camera.IsGrabbing())
		{
			camera.RetrieveResult(1000, ptrGrabResults, Pylon::TimeoutHandling_ThrowException);
			
			if (ptrGrabResults->GrabSucceeded()) {

				//Move from the pointer to the image buffer
				converter.Convert(pylonImage, ptrGrabResults);

				//Retrieve the image to opencv matrix
				imgM = cv::Mat(ptrGrabResults->GetHeight(), ptrGrabResults->GetWidth(), CV_8UC3, (uint8_t*)pylonImage.GetBuffer());
				
				// imgDisplay = imgMResize.clone();
				cv::resize(imgM, reImgM, cv::Size(600, 400), cv::INTER_LINEAR);


				cv::imshow("Trackbars", reImgM);

				//Close if q is pressed
				key = (char)cv::waitKey(1);

				exposureVal.SetValue((float)exposureSlider);
				frameR.SetValue((float)frameRateSlider);
				gainVal.SetValue((float)gainSlider);

				if (key == 'q')
				{
					std::cout << "Tuning complete, please proceed to measurements" << std::endl;
					points.push_back(cv::Point(0, 0));
					points.push_back(cv::Point(ptrGrabResults->GetWidth(), ptrGrabResults->GetHeight()));
					
					tracker(imgM);
					
					camera.StopGrabbing(); // //Clears buffer as well
					cv::destroyAllWindows();
					pylonImage.Release();
					break;
				}
				
			}
		}

		
		//Set Frame rate and number of images to grab for actual measurement
		frameR.SetValue(50.);
		int c_countOfImagesToGrab = 65*50 ;
		
		Pylon::CGrabResultPtr ptrGrabChunk;
		uint64 prev = 0;


		++iRound;
		
		sprintf_s(topic, "%d\n", (int)1);
		zmq_msg_init_size(&part1, sizeof(topic));
		memcpy(zmq_msg_data(&part1), topic, sizeof(topic));

		//Then, wait for a start command
		std::cout << "Press any key to start the measurement recording" << std::endl;
		std::getchar();
		
		//Save char to be published
		std::cout << "Waiting to aqcuire: " << c_countOfImagesToGrab << " images !" << std::endl;
		
		zmq_msg_send(&part1, publisher, 0);
		zmq_msg_close(&part1);
		
		
		std::cout << "Started Aquiring\nfps of last frame / number of skipped images"  << std::endl;
		//start acquire and put grabbed images into a que until recovered
		camera.StartGrabbing(c_countOfImagesToGrab,Pylon::GrabStrategy_LatestImages);

		while (camera.IsGrabbing())
		{
			//Retrieve results
			camera.RetrieveResult(1000, ptrGrabChunk, Pylon::TimeoutHandling_ThrowException);
			totalSkipped += ptrGrabChunk->GetNumberOfSkippedImages();
			if (ptrGrabChunk->GrabSucceeded()) 
			{

				//Move from smart pointer to pylon image with correct pixel format
				converter.Convert(pylonImage, ptrGrabChunk);

				//Recover timestamp and number of skipped images
				timeStamp = ptrGrabChunk->GetTimeStamp();

				//Convert to openCv Image for tracker and viz
				imgM = cv::Mat(ptrGrabChunk->GetHeight(), ptrGrabChunk->GetWidth(), CV_8UC3, (uint8_t*)pylonImage.GetBuffer());
				writer.write(imgM);
				tracker(imgM);
				
				++iRound;
				
				//Update few times
				if(iRound %1000 == 0){
					std::cout << timeStamp - prev << " / " << totalSkipped << std::endl;
				}

				prev = timeStamp;
				//Publish
				
				sprintf_s(topic, "%d\n", (int)1);
				zmq_msg_init_size(&part1, sizeof(topic));
				memcpy(zmq_msg_data(&part1), topic, sizeof(topic));


				sprintf_s(datatoSend, "%d,%d,%llu\n", (int)dataSend.x, (int)dataSend.y, timeStamp);
				zmq_msg_init_size(&part2, sizeof(datatoSend));
				memcpy(zmq_msg_data(&part2), datatoSend, sizeof(datatoSend));

				//Save char to be published
				zmq_msg_send(&part1, publisher, ZMQ_SNDMORE);
				//myfile << dataToSend;
				zmq_msg_send(&part2, publisher, 0);
				zmq_msg_close(&part2);
				zmq_msg_close(&part1);
				
			}
		}
		std::cout << "Task done! \nClosing..." << std::endl;
		std::cout << "Total time running: " << timeStamp << "(ms) \nImages skipped: " << totalSkipped << " qty." << std::endl;
		
		//Close
		camera.StopGrabbing();
		cv::destroyAllWindows();
		pylonImage.Release();
		
		//myfile.close();
		zmq_close(publisher);
		zmq_ctx_destroy(context);


		//imageFunction();
		//delete nodemap;
		return 0;
	}
	catch (const Pylon::GenericException& e)
	{
		std::cout << "Error occured!" << std::endl;

		cv::destroyAllWindows();
		pylonImage.Release();

		return 0;
	}

}


void tracker(cv::Mat frame)
{
	/* Function crops the frame and uses openCV otsu to bin threshold image
	During tuning, image is displayed and user selects the interest area by clicking
	1. left-top and 2. right-bottom of the rectangle
	*/
	
	// Declear
	cv::Mat grayFrame;
	double maxArea = 0;
	int maxAreaContourId = -1;

	//cv::Mat orgFrame = frame.clone();
	//Crop
	frame = frame(cv::Range(points[0].y, points[1].y), cv::Range(points[0].x, points[1].x));
	// Conver BW
	cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
	//Threshold
	long double thres = cv::threshold(grayFrame, grayFrame, 0, 255, cv::THRESH_OTSU);
	if (iRound == -1)
	{	//First round to display and choose crop

		xMax = points[1].x;
		yMax = points[1].y;

		//Init Video writer
		
		int codec = cv::VideoWriter::fourcc('M', 'P', '4', 'V');
		double fps =50;
		std::string filename = "live.mp4";
		cv::Size sizeFrame(xMax, yMax);
		writer.open(filename, codec, fps, sizeFrame, true);

		//Display and wait for 2 clicks
		cv::namedWindow("main", cv::WINDOW_AUTOSIZE);
		cv::setMouseCallback("main", mouse_callback);
		cv::imshow("main", grayFrame);
		char keyboard = cv::waitKey(0);
		cv::rectangle(grayFrame, points[0], points[1], cv::Scalar(0, 255, 0), 10, 8);
		cv::imshow("main", grayFrame);
		keyboard = cv::waitKey(0);
		cv::destroyAllWindows();

		clicks = 0;
		//Center of the retangle

		oldCenter.x = (int)abs(floor((points[1].x - points[0].x) / 2)) + points[0].x;
		oldCenter.y = (int)abs(floor((points[1].y - points[0].y) / 2)) + points[0].y;

	}
	else {
			//During imaging, detect and choose largest contour.
			cv::findContours(grayFrame, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
			if (contours.size() < 1) {
				dataSend.x = dataSend.x;
				dataSend.y = dataSend.y;
				std::cout << "nothing detected" << std::endl;
			} 
			else
			{
				for (int j = 0; j < contours.size(); j++) {
					double newArea = cv::contourArea(contours.at(j));
					if (newArea > maxArea) {
						maxArea = newArea;
						maxAreaContourId = j;
					}
				}

				maxArea = 0;

				//std::cout << "size: " << contours.size() << " index " << maxAreaContourId << std::endl;
				//cv::drawContours(orgFrame, contours, maxAreaContourId,cv::Scalar(0,0,255), 5, cv::LINE_8, hierarchy, 0);
				
				//Moments and find the center
				cv::Moments M = cv::moments(contours.at(maxAreaContourId));

				if (M.m00 == 0) {
					M.m00 = 1;
				}

				cv::Point center(M.m01 / M.m00 + points[0].x, M.m10 / M.m00 + points[0].y);

				//Check that the point is inside the frame
				if (center.x < 0) {
					center.x = 0;
				}
				if (center.y < 0) {
					center.y = 0;
				}
				if (center.x > xMax) {
					center.x = xMax;
				}
				if (center.y > yMax) {
					center.y = yMax;
				}

				//New points for crop retangle
				points[0].x += center.x - oldCenter.x;
				points[0].y += center.y - oldCenter.y;
				points[1].x += center.x - oldCenter.x;
				points[1].y += center.y - oldCenter.y;

				//Check that inside the image
				if (points[0].x < 0) {
					points[0].x = 0;
				}
				if (points[0].y < 0) {
					points[0].y = 0;
				}
				if (points[1].x > xMax) {
					points[1].x = xMax;
				}
				if (points[1].y > yMax) {
					points[1].y = yMax;
				}

				//Update coordinates
				oldCenter.x = center.x;
				oldCenter.y = center.y;

				dataSend.x = center.x;
				dataSend.y = center.y;
				/*
				* Some old debug stuff..
				if (iRound % 1 == 0) {
					cv::circle(orgFrame, center, 1, cv::Scalar(0, 0, 255), 100, 8, 0);
					cv::rectangle(orgFrame, points[1], points[0], cv::Scalar(0, 255, 0), 10, 8);
					cv::imshow("main", orgFrame);
					char keyboard = cv::waitKey(0);
					cv::destroyAllWindows();
				}
				*/
			}
	}
}

void mouse_callback(int  event, int  x, int  y, int  flag, void* param)
{
	/*Callback collecting click coordinates*/
	if (event == cv::EVENT_LBUTTONDOWN)
	{
		if (clicks < 2) {
			// Store point coordinates
			points[clicks].x = x;
			points[clicks].y = y;
			++clicks;
		}
		else {
			std::cout << "You have already chosen the object" << std::endl;
		}
	}
}




/*
struct createConfig(Pylon::CInstantCamera device, int frameRate, int gain)
{
	std::cout << "Please, tune the image" << std::endl;
	cv::namedWindow("Tuning", cv::WINDOW_AUTOSIZE);

	cv::createTrackbar("Exposure", "main", &frameRate, 10000000);
	cv::createTrackbar("Gain", "main", &gain, 24);

	device.StartGrabbing(Pylon::GrabStrategy_LatestImages);

	while (device.IsGrabbing())
	{
		device.RetrieveResult(100, ptrGrabResults, Pylon::TimeoutHandling_ThrowException);
		if(ptrGrabResults -> GrabSucceeded()){
			//Move from the pointer to the image buffer
			converter.Convert(pylonImage, ptrGrabResults);
			//Retrieve the image to opencv matrix
			imgM = cv::Mat(ptrGrabResults -> GetHeight(), ptrGrabResults -> GetWidth(), CV_8UC3, pylonImage.GetBuffer());
			//resize and display
			cv::resize(imgM, imgM, cv::Size(600, 400), cv::INTER_LINEAR);
			cv::imshow("Tune", imgM);

			//Close if q is pressed
			static int key = (cv::waitKey(1) & 0xFF);
			if (key == 'q');
			{
				std::cout << "Tuning complete, please proceed to measurements" << std::endl;
				device.StopGrabbing();
				cv::destroyAllWindows();
			}
		}
	}
}
*/

/*
void imageFunction();
{
	int recImg = 0;
	//char charLine[];
	camera.StartGrabbing(80 * frameR, Pylon::GrabStrategy_LastImages);
	while(camera.IsGrabbing())
	{
		camera.RetrieveResult(100, ptrGrabResults, Pylon::TimeoutHandling_ThrowException);
		if (ptrGrabResults->GrabSucceeded())
		{
			recImg += 1;
			converter.Convert(pylonImage, ptrGrabResults);
			openCvImage = cv::Mat(ptrGrabResults->GetHeight(), ptrGrabResults->GetWidth(), CV_8UC3, (uint8_t*)pylonImage.GetBuffer());

			// Send to tracker thread tracker

			// How to publish images?

		}
		else {
			std::cout << "Error occured while printing" << std::endl;
			break;
		}
	}
}
*/
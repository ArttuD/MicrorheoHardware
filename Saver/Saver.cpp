

#include <iostream>
#include <fstream>
#include <zmq.h>
#include <vector>
#include <String>

#include <zmq_utils.h>
#include <zmq.h>


char topic[10];
char dataRec[100];

float data1, data2, data3, data4;
int data5, data6;
unsigned long long data7;

int _; //catch scan output

int main()
{

    //Create subscriber socket and conntect it on local host
    void* context = zmq_ctx_new();
    void* subscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(subscriber, "tcp://130.233.164.32:6660");

    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE,"",NULL);
    
    //Opem folders to save
    std::ofstream MgFile("ggData.csv");
    std::ofstream CurrentFile("currentData.csv");
    std::ofstream trackerFile("trackerData.csv");

    bool flag = false;
    std::cout << "Created saver" << std::endl;
    while (true) {
        while (1) {
            //std::cout << "Waiting Data" << std::endl;
            zmq_msg_t message;
            zmq_msg_init(&message);
            zmq_msg_recv(&message, subscriber, 0);
            //  Process the message frame
            if (!flag) {
                flag = true;
                auto msg = zmq_msg_data(&message);
                auto msgSize = zmq_msg_size(&message);
                memcpy(topic, msg, msgSize);
                //std::cout << "Received topic: " << (int)topic[0] << " size " << msgSize << std::endl;
                zmq_msg_close(&message);
            }
            else {

                auto msg = zmq_msg_data(&message);
                auto msgSize = zmq_msg_size(&message);
                memcpy(dataRec, msg, msgSize);
                //std::cout << "Received data: " << dataRec << std::endl;
                //Figure how topics work
                switch ((int)topic[0])
                {
                case 0:
                    //std::cout << "In topic 0" << std::endl;
                    _ = sscanf_s(dataRec, "%f,%f\n", &data1, &data2);
                    break;
                case 49:
                    //std::cout << "In topic 1" << std::endl;
                    _ = sscanf_s(dataRec, "%d,%d,%llu\n", &data5, &data6, &data7);
                    break;
                case 50:
                    //std::cout << "In topic 2" << std::endl;
                    _ = sscanf_s(dataRec, "%f,%f,%f,%f\n", &data1, &data2, &data3, &data4);
                    break;
                /*
                default:
                    std::cout << "Incorrect packages" << std::endl;
                    break;
                 */
                }
            zmq_msg_close(&message);
            }
            if (!zmq_msg_more(&message)) {
                zmq_msg_close(&message);
                flag = false;
                break;      //  Last message frame
            }
        }
   
    }

    MgFile.close();
    CurrentFile.close();
    trackerFile.close();
    zmq_close(subscriber);
    zmq_ctx_destroy(context);

    return 0;
}
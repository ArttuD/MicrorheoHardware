
/*
I)
Receives data from 
0) Mg Sensor [B, time] - Magnetic field
1) Camera [x,y,time] - particle location
2) Current control - Input, Measured, Corrected output

II)
Compares B value to initial
 -> increase current sensing value in currentControl.py if needed

 III)
Sends these further to Visualizer and Saver

*/


#include <iostream>
#include <vector>

#include <zmq.h>
#include <zmq_utils.h>



int main(int argc, char* argv[])
{
    //create contex
    void *ctx = zmq_ctx_new();

    //Publish for viz, saver, and deal to field analysis
    void* publisher = zmq_socket(ctx, ZMQ_XPUB);
    zmq_bind(publisher, "tcp://130.233.164.32:6660");
    std::cout << "Publisher Created" << std::endl;

    //Create Mg sensor socket
    void* subscriber = zmq_socket(ctx, ZMQ_XSUB);
    zmq_connect(subscriber, "tcp://130.233.164.32:5550");
    zmq_connect(subscriber, "tcp://130.233.164.32:5551");
    zmq_connect(subscriber, "tcp://130.233.164.32:5552");
    std::cout << "subs Bound" << std::endl;
    zmq_proxy(subscriber, publisher, NULL);

    zmq_close(subscriber);
    zmq_close(publisher);
    zmq_ctx_destroy(ctx);
    return 0;
}
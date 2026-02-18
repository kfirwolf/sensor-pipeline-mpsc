#include "sensor_worker.h"
#include "fake_sensor_source.h"
#include "sensor_source.h"
#include "global_queue.h"
#include "fake_frame_parser.h"
#include "measurement.h"
#include <thread>
#include <iostream>
#include <chrono>

void consumer(global_queue<measurement> &gq) {
    measurement ms;
    while (gq.pop(ms)) {
        std::cout << " q.pop(ms), measurement:  ";
        for (const uint8_t &element : ms.payload) {
            std::cout << (int)element << " ";  
        }
       std::cout << "" << std::endl; 
    }
}

int main(int argc, char const *argv[]) {

    fake_sensor_source f_sensor;
    global_queue<measurement> q(64);
    fake_frame_parser parser;
    std::thread consumer_th(consumer, std::ref(q));

    sensor_worker sensor(1, 256, f_sensor, parser, q);
    
    sensor.start();
    std::cout << " global queue capacity: " << q.capacity() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << " \nbreak from while " << std::endl;
    
    sensor.stop();
    q.shutdown();
    std::cout << " \nafter sensor.stop() and queue shotdown" << std::endl;
    if (consumer_th.joinable()) {
        std::cout << " \nconsumer_th.joinable()" << std::endl;
        consumer_th.join();
        std::cout << " \n afterconsumer_th.join()" << std::endl;
    }
/////////////////////////////////////////////////////////////////////////

    return 0;
}

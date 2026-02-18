#ifndef _SENSOR_SOURCE_H_
#define _SENSOR_SOURCE_H_

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

class sensor_source
{
public:
    virtual ~sensor_source() = default;
    
    /*           
        Reads up to buf_len bytes into buf.
        Blocks until data is available or an error occurs.
        Returns:
        >0  : bytes read
        =0  : permanent end-of-stream (device closed / shutdown)
        <0  : transient error (retry) 
    */     
    virtual ssize_t read_bytes(uint8_t* buf, size_t buf_len) = 0; //pure virtual (no impl)
    virtual int stop_request() = 0;
};

#endif

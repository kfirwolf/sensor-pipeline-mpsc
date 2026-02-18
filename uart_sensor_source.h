#include "sensor_source.h"
#include "unique_fd.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string>
#include <stdexcept>
#include <system_error>
#include <cerrno>
#include <poll.h>
#include <sys/eventfd.h>
#include <cstdint>

/*
    std::system_error for syscall failures
    std::runtime_error for “your own” logic errors (like invalid config)
*/

enum class data_bits {seven = 7, eight = 8};
enum class parity {N, E, O};
enum class stop_bits {one , two};

struct uart_config {
    std::string device;
    int baud_rate;
    data_bits d_bits;
    parity par;
    stop_bits s_bits;
};

class uart_sensor_source : public sensor_source
{
private:
    unique_fd u_stopfd;
    unique_fd u_fd;

    static speed_t to_speed(int baud) {
        switch(baud) {
            case 9600:
                return B9600;
            case 19200:
                return B19200;
            case 38400:
                return B38400;
            case 57600:
                return B57600;    
            case 115200:
                return B115200;
            case 230400:
                return B230400;  
            case 576000:
                return B576000;       
            case 921600:
                return B921600;
            default:
                throw std::invalid_argument("unsuported baud rate");            
        }
    }

public:
    uart_sensor_source(const uart_config& uart_conf) {

        /*open file descriptor:
            O_RDWR: you can read/write (needed for UART config sometimes)
            O_NOCTTY: don’t let this serial device become your controlling terminal
            O_CLOEXEC: prevents fd leaks if your process ever exec()’s
            O_NONBLOCK: guarantees read() won’t block (works nicely with your poll() loop)
        */
        int tmp_fd  = open(uart_conf.device.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK); //bit field : 256 + 2 = 258 100000010
        if (tmp_fd < 0) {
            throw std::system_error(errno,std::generic_category() ,"open file error ");
        }

        u_fd.reset(tmp_fd);

        tmp_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); //linux system call ( 0 is init counter value => not readable yet,  flags: EFD_NONBLOCK => read not block, EFD_CLOEXEC => not leak to child process )
        if (tmp_fd < 0) {
            throw std::system_error(errno,std::generic_category() ,"open file error ");
        }    

        u_stopfd.reset(tmp_fd);

        //configure uart
        struct termios termio {};
        //it can also be wrote as: struct termios termio; std::memset(&termio, 0, sizeof(termio)); 

        if (tcgetattr(u_fd.get(), &termio) != 0) {
            throw std::system_error(errno,std::generic_category() , "tcgetattr failed ");
        }

        //set raw mode
        cfmakeraw(&termio);

        //set baud_rate
        speed_t sp = to_speed(uart_conf.baud_rate);
        cfsetispeed(&termio, sp);
        cfsetospeed(&termio, sp);        

        //clear the c_c bits (bit 4 and 5)
        termio.c_cflag &= ~CSIZE; 

        //set data bits 
        termio.c_cflag |= (uart_conf.d_bits == data_bits::eight)? CS8 : CS7;

        //set parity
        switch (uart_conf.par) {
            case(parity::N):
                termio.c_cflag &= ~(PARENB | PARODD);
                break;
            case(parity::E):
                termio.c_cflag &= ~PARODD;
                termio.c_cflag |= PARENB;
                break;
            case(parity::O):
                termio.c_cflag |= PARODD;
                termio.c_cflag |= PARENB;
                break;
                
            default:
                throw std::invalid_argument("unsuported parity");
        }

        //set stop bits
        if (uart_conf.s_bits == stop_bits::one) {
            termio.c_cflag &= ~CSTOPB;
        }
        else {
            termio.c_cflag |= CSTOPB;
        }

        termio.c_cflag |= (CLOCAL | CREAD);
    
        //no need timeouts with the correct mech of poll + read (with a eventfd for stopping)
        termio.c_cc[VMIN]  = 0;
        termio.c_cc[VTIME] = 0;        
        

        //set the configuration
        if (tcsetattr(u_fd.get(), TCSANOW, &termio) != 0) {
            throw std::system_error(errno,std::generic_category() ,"tcsetattr failed ");
        }
    }

    virtual ssize_t read_bytes(uint8_t* buf, size_t buf_len) override {
        pollfd plfd[2]{};
        plfd[0].fd = u_fd.get();
        plfd[1].fd = u_stopfd.get();
        plfd[0].events = POLLIN;
        plfd[1].events = POLLIN;    
    
        while (true) {

            int rc = poll(&plfd[0], 2, -1); //block until:  data in Rx buffer / stop request

            if (rc < 0) {
                if (errno == EINTR) continue;
                return -1;
            }

            //stop request
            if (plfd[1].revents & POLLIN) {
                uint64_t v;
                read(u_stopfd.get(), &v, sizeof(v)); //set event_fd back to 0 so next call to poll it will block. 
                return 0; // signal stop
            }               

            //data to read
            if (plfd[0].revents & POLLIN) {
                ssize_t ret = read(plfd[0].fd, buf, buf_len);  
                
                if (ret >= 0) {
                    return ret;
                }

                // errors types
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    // EWOULDBLOCK and EINTR: poll said readable, but read raced. retry.
                    //EINTR: signal interrupted read, retry
                    continue;
                }

                return -1;
            }

            //(stop event: event_fd error cases
            if (plfd[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return -1;
            }

            // UART error cases
            if (plfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return -1;
            }
        }
        
        return 0;
    }

    virtual int stop_request() override {
        //setting event_fd counter > 0 will result in poll function return (in the read_bytes func)
        uint64_t eventfd_counter = 1;
        ssize_t ret = write(u_stopfd.get(), &eventfd_counter, sizeof(eventfd_counter));

        if (ret != sizeof(uint64_t)) {
            return -1;
        }

        return 0;
    }
};



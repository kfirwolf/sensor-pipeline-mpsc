#include <unistd.h>

class unique_fd
{
private:
    int fd;
    static constexpr int default_fd = -1;
public:

    unique_fd() : fd(default_fd) {

    }

    explicit unique_fd(int input_fd) : fd(input_fd) {

    }

    ~unique_fd() {
        if (fd < 0) {
            return;
        }
        close(fd);
    }    

    unique_fd(const unique_fd& rhs) = delete;

    unique_fd(unique_fd&& rhs) noexcept {
     
        fd = rhs.fd;
        rhs.fd = -1;
    }

    unique_fd& operator=(const unique_fd& u_fd) = delete;
    
    unique_fd& operator=(unique_fd&& rhs_u_fd) noexcept {
        if (this == &rhs_u_fd) {
            return *this;
        }

        if (fd >= 0) {
            close(fd);
        }

        fd = rhs_u_fd.fd;
        rhs_u_fd.fd = -1;
        return *this;
    }

    void reset(int ne_fd) noexcept {

        if (fd == ne_fd) {
            return;
        }

        if (fd >= 0) {
            close(fd);
        }

        fd = ne_fd;
    }

    int get() const noexcept {
        return fd;
    }

    int release() noexcept {

        int ret_fd = fd;
        fd = -1;

        return ret_fd;
    }
};

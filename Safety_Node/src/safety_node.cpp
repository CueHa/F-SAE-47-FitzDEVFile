#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <sstream>
 
enum class State {
    OFF,
    READY,
    DRIVING,
    EMERGENCY,
    FINISHED
};
 
class SafetyFSM {
public:
    SafetyFSM() : current_state_(State::OFF) {
        std::cout << "[INFO] Safety FSM started in OFF state.\n";
    }
 
    ~SafetyFSM() {
        if (serial_port_ != -1) {
            close(serial_port_);
            std::cout << "[INFO] UART port closed.\n";
        }
    }
 
    void run() {
        while (current_state_ != State::FINISHED) {
            std::string state_str = stateToString(current_state_);
            std::cout << "[INFO] Current state: " << state_str << "\n";
 
            send_uart_message(state_str);
            process_uart_input();
 
            State new_state = determine_fsm_state();
            if (new_state != current_state_) {
                std::cout << "[INFO] Transitioning from " << stateToString(current_state_) << " to " << stateToString(new_state) << "\n";
                current_state_ = new_state;
            }
 
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
 
        std::cout << "[INFO] FSM reached FINISHED state. Exiting.\n";
    }
 
    bool setup_uart(const std::string& port = "/dev/ttyUSB0", int baud = B115200) {
        serial_port_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (serial_port_ < 0) {
            std::cerr << "[ERROR] Failed to open UART port: " << port << "\n";
            return false;
        }
 
        termios tty{};
        if (tcgetattr(serial_port_, &tty) != 0) {
            std::cerr << "[ERROR] tcgetattr failed.\n";
            return false;
        }
 
        cfsetospeed(&tty, baud);
        cfsetispeed(&tty, baud);
 
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
 
        tty.c_lflag = 0;
        tty.c_oflag = 0;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 10;
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
 
        if (tcsetattr(serial_port_, TCSANOW, &tty) != 0) {
            std::cerr << "[ERROR] tcsetattr failed.\n";
            return false;
        }
 
        std::cout << "[INFO] UART initialized on " << port << "\n";
        return true;
    }
 
private:
    State current_state_;
    int serial_port_ = -1;
    std::string uart_buffer_;
 
    // State variables
    bool ASMS_active_ = false;
    bool breaks_engaged_ = false;
    bool throttle_engaged_ = false;
    bool estop_engaged_ = false;
    bool kill_switch_engaged_ = false;
    bool ebs_engaged_ = false;
    bool ASB_ok_ = false;
    bool TS_active_ = false;
    std::string mission_status_ = "none";
 
    std::string stateToString(State s) {
        switch (s) {
            case State::OFF: return "OFF";
            case State::READY: return "READY";
            case State::DRIVING: return "DRIVING";
            case State::EMERGENCY: return "EMERGENCY";
            case State::FINISHED: return "FINISHED";
            default: return "UNKNOWN";
        }
    }
 
    void send_uart_message(const std::string& message) {
        if (serial_port_ != -1) {
            std::string msg = message + "\n";
            write(serial_port_, msg.c_str(), msg.length());
        }
    }
 
    void process_uart_input() {
        if (serial_port_ == -1) return;
 
        char buf[256];
        int bytes_read = read(serial_port_, buf, sizeof(buf));
        if (bytes_read > 0) {
            uart_buffer_.append(buf, bytes_read);
 
            size_t pos;
            while ((pos = uart_buffer_.find('\n')) != std::string::npos) {
                std::string message = uart_buffer_.substr(0, pos);
                uart_buffer_.erase(0, pos + 1);
 
                std::cout << "[INFO] Received UART message: '" << message << "'\n";
 
                if (message.find('=') != std::string::npos && message.find(',') != std::string::npos) {
                    std::unordered_map<std::string, bool> flags;
                    std::stringstream ss(message);
                    std::string pair;
 
                    while (std::getline(ss, pair, ',')) {
                        size_t eq_pos = pair.find('=');
                        if (eq_pos != std::string::npos) {
                            std::string key = pair.substr(0, eq_pos);
                            bool value = pair[eq_pos + 1] == '1';
                            flags[key] = value;
                        }
                    }
 
                    ASMS_active_ = flags["ASMS"];
                    breaks_engaged_ = flags["BRK"];
                    throttle_engaged_ = flags["THR"];
                    estop_engaged_ = flags["ESTOP"];
                    kill_switch_engaged_ = flags["KILL"];
                    ebs_engaged_ = flags["EBS"];
                    ASB_ok_ = flags["ASB"];
                    TS_active_ = flags["TS"];
                }
            }
        }
    }
 
    State determine_fsm_state() {
        if (kill_switch_engaged_) return State::EMERGENCY;
 
        if (current_state_ == State::DRIVING && estop_engaged_)
            return State::EMERGENCY;
 
        if (ebs_engaged_) {
            if (mission_status_ != "none" && !throttle_engaged_) {
                return (current_state_ == State::DRIVING) ? State::FINISHED : State::EMERGENCY;
            } else {
                return State::EMERGENCY;
            }
        } else if (mission_status_ != "none" && ASMS_active_ && ASB_ok_ && TS_active_) {
            if (!estop_engaged_) {
                return (current_state_ == State::READY) ? State::DRIVING : State::EMERGENCY;
            } else if (breaks_engaged_) {
                return (current_state_ == State::DRIVING) ? State::READY : State::EMERGENCY;
            } else {
                return State::OFF;
            }
        } else {
            return State::OFF;
        }
    }
};
 
// Entry point
int main() {
    SafetyFSM fsm;
    if (!fsm.setup_uart("/dev/ttyUSB0")) {
        return 1;
    }
    fsm.run();
    return 0;
}
 
 
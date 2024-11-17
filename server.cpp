#include <netdb.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <array>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <thread>

// TODO:
// - remove trailing and leading whitespace from key/value
// - add command/key/value size validation
// - ensure keys/values are alphanumeric
#define PORT "3490"

class NetworkException : public std::runtime_error {
    public:
        explicit NetworkException(const std::string& message) : std::runtime_error(message) {}
};

class ConnectionException : public NetworkException {
    public:
        explicit ConnectionException(const std::string& message)
            : NetworkException(message) {}
};

class MessageException : public NetworkException {
    public:
        explicit MessageException(const std::string& message)
            : NetworkException(message) {}
};

class TimeoutException : public NetworkException {
    public:
        explicit TimeoutException(const std::string& message)
            : NetworkException(message) {}
};

class KeyValueStore {
public:
    KeyValueStore(const std::string& path = "store.txt") : filepath(path) {
        load();
    }

    std::string set(const std::string& key, const std::string& value) {
        store.insert({key, value});
        save();
        return "Added " + key + " and value " + value + "\n";
    }

    std::string get(const std::string& key) const {
        auto it = store.find(key);
        if (it != store.end()) {
            return it->second;
        }
        return "Key: " + key + " not found." ;
    }

    std::string remove(const std::string& key) {
        store.erase(key);
        save();
        return "Removed key";
    }

    std::string printStore() const {
        std::stringstream ss;
        for (const auto& pair : store) {
            ss << "[KEY]: " << pair.first << "\t[VALUE]: " << pair.second << std::endl;
        }
        return ss.str();
    }

private:
    std::unordered_map<std::string, std::string> store;
    std::string filepath;

    bool save() const {
        try {
            std::ofstream file(filepath);
            if (!file.is_open()) {
                return false;
            }

            for (const auto& pair : store) {
                file << pair.first << "\t" << pair.second << "\n";
                
                if (file.fail()) {
                    return false;
                }
            }

            file.flush();
            return true;
        }
        catch (const std::exception& e) {
            return false;
        }
    }

    bool load() {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                return false;
            }

            std::string line;
            while (std::getline(file, line)) {

                size_t tab_pos = line.find('\t');

                std::string key = line.substr(0, tab_pos);
                std::string value = line.substr(tab_pos + 1);

                store[key] = value;
            }

            return true;
        }
        catch (const std::exception& e) {
            return false;
        }
}
};

enum class Command {
    SET,
    GET,
    REMOVE,
    PRINT,
    UNKNOWN
};

class CommandParser {
public:
    CommandParser(KeyValueStore& store) : m_store(store) {}
    CommandParser(const CommandParser&) = delete;
    CommandParser& operator=(const CommandParser&) = delete;

    std::string parseAndExecute(const std::string& command) {
        if (command.empty() || std::all_of(command.begin(), command.end(), isspace)) {
            return "ERROR: Empty command";
        }

        std::vector<std::string> tokens = splitString(command, ' ');

        if (tokens.empty()) return "Invalid command";

        Command cmd = stringToCommand(tokens[0]);
        switch (cmd) {
            case Command::SET:
                return handleSetCommand(tokens);
            case Command::GET:
                return handleGetCommand(tokens);
            case Command::REMOVE:
                return handleRemoveCommand(tokens);
            case Command::PRINT:
                return m_store.printStore();
            case Command::UNKNOWN:
                return "Unknown command: " + tokens[0] + "\n";
        }
        return "ERROR: Unhandled command\n"; // this should be unreachable
    }

private:
    std::string handleSetCommand(const std::vector<std::string>& tokens) {
        if (tokens.size() != 3) {
            return "ERROR: SET command requires exactly 2 arguments (key, value)\n";
        }
        return m_store.set(tokens[1], tokens[2]);
    }
    std::string handleGetCommand(const std::vector<std::string>& tokens) {
        if (tokens.size() != 2) {
            return "ERROR: GET command requires exactly 1 argument (key)\n";
        }
        return m_store.get(tokens[1]);
    }
    std::string handleRemoveCommand(const std::vector<std::string>& tokens) {
        if (tokens.size() != 2) {
            return "ERROR: REMOVE command requires exactly 1 argument (key)\n";
        }
        return m_store.remove(tokens[1]);
    }

    std::vector<std::string> splitString(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::stringstream ss(str);

        while (std::getline(ss, token, delimiter)) {
            if (!token.empty()) { 
                tokens.push_back(token);
            }
        }

        return tokens;
    }

    Command stringToCommand(std::string& cmd) {
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
        if (cmd == "SET") return Command::SET;
        if (cmd == "GET") return Command::GET;
        if (cmd == "REMOVE") return Command::REMOVE;
        if (cmd == "PRINT") return Command::PRINT;
        return Command::UNKNOWN;
    }

    KeyValueStore& m_store;
};


class Server {
private:
    int m_sock_fd;
    std::string m_port;
    static const size_t BUF_SIZE = 100;
    static const int BACKLOG = 10;

    KeyValueStore m_store;
    CommandParser m_parser;

    struct addrinfo m_hints;
    struct sockaddr_storage m_client_addr;

    // See: https://beej.us/guide/bgnet/html/#client-server-background
    void setupServer() {
        struct addrinfo *servInfo, *p;
        int status;
        if ((status = getaddrinfo(NULL, m_port.c_str(), &m_hints, &servInfo))) {
            throw ConnectionException("Failed to setup server: " + std::string(gai_strerror(status)));
        }

        for (p = servInfo; p != NULL; p = p->ai_next) {
            if ((m_sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                continue;
            }
            
            if (bind(m_sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
                close(m_sock_fd);
                continue;
            }
            break;
        }

        if (p == NULL) {
            freeaddrinfo(servInfo);
            throw ConnectionException("Failed to bind server socket");
        }

        freeaddrinfo(servInfo);

        if (listen(m_sock_fd, BACKLOG) == -1) {
            throw ConnectionException("Failed to listen on socket: " + std::string(strerror(errno)));
        }
    }


    void handleClient(int client_fd) {
        std::array<char, BUF_SIZE> recv_buf; // each thread needs it's own receive buffer
        
        while(true) {
            try {
                auto num_bytes = recv(client_fd, recv_buf.data(), BUF_SIZE-1, 0);
                if (num_bytes == -1) {
                    throw MessageException("Error receiving message: " + std::string(strerror(errno)));
                }
                if (num_bytes == 0) {
                    throw NetworkException("Client disconnected");
                }
                
                recv_buf[num_bytes] = '\0';
                std::string message(recv_buf.data(), num_bytes);
                std::cout << "Received: " << message << std::endl;

                if (message == "quit") {
                    std::cout << "Client requested quit" << std::endl;
                    break;
                }

                std::string response = m_parser.parseAndExecute(message);
                if (send(client_fd, response.c_str(), response.length(), 0) == -1) {
                    throw MessageException("Error sending message: " + std::string(strerror(errno)));
                }
            }
            catch (const NetworkException& e) {
                std::cout << "Client connection ended: " << e.what() << std::endl;
                break;
            }
            catch (const std::exception& e) {
                std::cerr << "Error handling client: " << e.what() << std::endl;
                break;
            }
        }
        close(client_fd);
    }

public:
    Server(std::string port = PORT) 
        : m_sock_fd(-1)
        , m_port(port)
        , m_hints{}
        , m_parser{m_store}
    {
        m_hints.ai_family = AF_INET;
        m_hints.ai_socktype = SOCK_STREAM;
        m_hints.ai_flags = AI_PASSIVE;
    }

    ~Server() {
        stop();
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start() {
        setupServer();
        std::cout << "Server listening on port " << m_port << std::endl;
        
        while(true) {
            try {
                socklen_t addr_size = sizeof(m_client_addr);
                int client_fd = accept(m_sock_fd, (struct sockaddr*)&m_client_addr, &addr_size);

                std::string con_ip;
                con_ip.resize(INET_ADDRSTRLEN);
                inet_ntop(m_client_addr.ss_family, (struct sockaddr*)&m_client_addr, &con_ip[0], INET_ADDRSTRLEN);
                std::cout << "Server got connection from " << con_ip << std::endl;

                if (client_fd == -1) {
                    throw ConnectionException("Failed to accept connection: " + std::string(strerror(errno)));
                }

                std::thread([this, client_fd]() {
                    this->handleClient(client_fd);
                }).detach();
            }
            catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }

    void stop() {
        if (m_sock_fd >= 0) {
            close(m_sock_fd);
            m_sock_fd = -1;
        }
    }
};

int main() {
    try {
        Server server;
        server.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

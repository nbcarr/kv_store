# Key-Value Store Server
A multi-threaded TCP server implementing a persistent key-value store, built in C++

## Features
- Multi-threaded client handling
- Persistent storage
- Thread-safe operations
- Custom command protocol
- Error handling and graceful shutdown

## Building
```bash
g++ -std=c++11 -pthread server.cpp -o server

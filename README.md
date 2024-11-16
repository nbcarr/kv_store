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
```

## Usage
Start the server, which listens on port `3490` by default:
```bash
./server
```

See python_client.py for sample client that connects to the server:
```bash
python3 python_client.py
```

## Protocol
The server accepts the following commands:

SET <key> <value>: Store a value

GET <key>: Retrieve a value

REMOVE <key>: Delete a value

PRINT: Display all key-value pairs


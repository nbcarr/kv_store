import socket

class Client:
    def __init__(self, host, port=3490):
        self.host = host
        self.port = port
        self.sock = None

    def connect_to_server(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"Connected to {self.host} on port {self.port}")
        except socket.error as e:
            print(f"Connection failed: {e}")
            self.sock = None

    def send_message(self, message):
        if self.sock is None:
            print("No connection to server.")
            return

        try:
            self.sock.sendall(message.encode())
        except socket.error as e:
            print(f"Error sending message: {e}")

    def receive_message(self):
        if self.sock is None:
            return
        try:
            response = self.sock.recv(100)
            return response.decode()
        except socket.error as e:
            print(f"Error receiving message: {e}")
            return None

    def run_main_loop(self):
        if self.sock is None:
            print("Cannot start main loop; no server connection.")
            return

        try:
            while True:
                message = input("Enter message: ")
                if message.lower() == "quit":
                    self.send_message("quit")
                    print("Quitting...")
                    break

                self.send_message(message)
                response = self.receive_message()
                if response:
                    print(f"Response: {response}")
                else:
                    print("No response from server or connection closed.")
                    break
        finally:
            self.stop()

    def stop(self):
        if self.sock:
            self.sock.close()
            print("Connection closed.")

if __name__ == "__main__":
    client = Client("localhost")
    client.connect_to_server()
    client.run_main_loop()

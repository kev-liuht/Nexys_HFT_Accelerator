import socket
from itch_parser import ITCHParser
import logging

LOGGING_LEVEL = logging.DEBUG # Change to logging.INFO for less verbose logging
LOGGING_FORMAT = '%(asctime)s - %(levelname)s - %(message)s'
logging.basicConfig(format=LOGGING_FORMAT, level=LOGGING_LEVEL)

def main():
    server_ip = 'localhost'
    server_port = 12345  # Replace with the appropriate port number
    parser = ITCHParser()
    # Create a TCP/IP socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # Connect the socket to the server
        client_socket.connect((server_ip, server_port))
        print(f"Connected to {server_ip}:{server_port}")

        while True:
            # Receive data from the server
            data = client_socket.recv(1024)
            if not data:
                break
            while data:
                # Parse the message length
                length = int.from_bytes(data[:2], byteorder='big')
                message = data[2:2+length]
                # Decode the message
                message_type_code = message[0:1]
                message_data = message[1:]

                decoded_message = parser.decode_message(message_type_code, message_data)
                if decoded_message is not None:
                    if LOGGING_LEVEL == logging.DEBUG:
                        parser.print_human_readable_message(decoded_message)
                logging.debug(f"message_type_code: {message_type_code}, message_data: {message_data}")

                # Move to the next message
                data = data[2+length:]

    except Exception as e:
        print(f"An error occurred: {e}")

    finally:
        # Close the socket
        client_socket.close()
        print("Connection closed")

if __name__ == "__main__":
    main()
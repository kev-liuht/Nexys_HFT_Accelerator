import socket
from itch_parser import ITCHParser
import logging
import os
import argparse

LOGGING_LEVEL = logging.INFO  # Change to logging.INFO for less verbose logging
LOGGING_FORMAT = '%(asctime)s - %(levelname)s - %(message)s'
logging.basicConfig(format=LOGGING_FORMAT, level=LOGGING_LEVEL)

# TCP SERVER CONFIG
SERVER_IP = '192.168.1.11'
SERVER_PORT = 22

OUTPUT_FILE_DEFAULT = 'data/01302019.NASDAQ_ITCH50_AFECXDU.bin'

def main(output_file):
    server_ip = SERVER_IP
    server_port = SERVER_PORT
    parser = ITCHParser()
    # Create a TCP/IP socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    try:
        # Connect the socket to the server
        client_socket.connect((server_ip, server_port))
        print(f"Connected to {server_ip}:{server_port}")
        num_bytes_to_skip = 0
        with open(output_file, 'wb') as output_file_handle:
            while True:
                # Receive data from the server
                data = client_socket.recv(1024 * 1024)
                if not data:
                    break
                while data:
                    # Skip bytes if necessary. Some messages are split across multiple data packets. Skipping incomplete messages.
                    if num_bytes_to_skip > 0:
                        if num_bytes_to_skip > len(data):
                            num_bytes_to_skip -= len(data)
                            break
                        else:
                            data = data[num_bytes_to_skip:]
                            num_bytes_to_skip = 0
                    # Parse the message length
                    length = int.from_bytes(data[:2], byteorder='big')
                    if length > len(data[2:]):
                        logging.debug(f"Expected message length: {length}, actual length: {len(data[2:])}")
                        num_bytes_to_skip = length - len(data[2:])
                    message = data[2:2 + length]
                    # Decode the message
                    message_type_code = message[0:1]
                    message_data = message[1:]

                    logging.debug(f"message_type_code: {message_type_code}, message length: {length}. ")

                    decoded_message = parser.decode_message(message_type_code, message_data)
                    if decoded_message is not None:
                        if LOGGING_LEVEL == logging.DEBUG:
                            parser.print_human_readable_message(decoded_message)

                    if message_type_code in [b'A', b'F', b'E', b'C', b'X', b'D', b'U']:
                        full_message = length.to_bytes(2, byteorder='big') + message
                        output_file_handle.write(full_message)

                    # Move to the next message
                    data = data[2 + length:]

    except Exception as e:
        print(f"An error occurred: {e}")

    finally:
        # Close the socket
        client_socket.close()
        print("Connection closed")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='TCP client to receive and save ITCH data.')
    parser.add_argument('-o', '--output_file', type=str, default=OUTPUT_FILE_DEFAULT, help='Output file name')
    args = parser.parse_args()
    main(args.output_file)
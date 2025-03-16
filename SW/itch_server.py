import gzip
from pathlib import Path
from struct import unpack
from datetime import datetime
import socket
import logging
import sys
import time

# Set up logging
LOGGING_LEVEL = logging.DEBUG
logging.basicConfig(level=LOGGING_LEVEL)

# Define data path and file name as constants
DATA_PATH = Path('data')
SOURCE_FILE_ZIPPED = '01302019.NASDAQ_ITCH50.gz'  # Example zipped file
SOURCE_FILE = 'output.bin'                       # Our filtered file

# Ensure data directory exists
DATA_PATH.mkdir(parents=True, exist_ok=True)
FILE_NAME = DATA_PATH / SOURCE_FILE

# Check if the data file exists, if not, inform user to download (and ideally unzip)
if not FILE_NAME.exists():
    print(f"Please ensure the NASDAQ ITCH50 data file '{SOURCE_FILE}' exists in the '{DATA_PATH}' directory.")
    print(f"You may need to download and unzip '{SOURCE_FILE_ZIPPED}' to '{SOURCE_FILE}' in '{DATA_PATH}'.")
    print("Exiting program.")
    sys.exit(1)

# TCP SERVER CONFIG
HOST = '192.168.1.11'
PORT = 22

def read_messages(file_path):
    """
    Read all messages from the given file_path and return them as a list of bytes.
    Each message is structured as:
        - 2 bytes: Big-endian unsigned short (the total message length, including message_type byte).
        - 1 byte : message type code.
        - (length - 1) bytes: message data.
    """
    messages = []
    with open(file_path, 'rb') as f:
        while True:
            # Read first 2 bytes to get message length
            length_bytes = f.read(2)
            if len(length_bytes) < 2:
                # End of file or partial read => no more full messages
                break

            message_length = unpack('>H', length_bytes)[0]
            # Next byte is the message type
            message_type = f.read(1)
            if len(message_type) < 1:
                break  # incomplete message, EOF

            # Remaining bytes in this message
            remaining_len = message_length - 1
            message_data = f.read(remaining_len)
            if len(message_data) < remaining_len:
                break  # incomplete, EOF

            # Combine into a single bytes object
            full_message = length_bytes + message_type + message_data
            messages.append(full_message)

    return messages

def main():
    """
    Main function to send ITCH file data to a TCP client.
    - Sends first N-1 messages automatically with a 0.5 second delay between each.
    - Pauses and waits for user [ENTER] before sending the final message.
    """
    # Prepare TCP server
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # Allow immediate reuse
        sock.bind((HOST, PORT))
        sock.listen(1)
        print(f"Listening on {HOST}:{PORT}")

        # Server runs in a loop, accepting connections
        while True:
            conn, addr = sock.accept()
            print(f"Connected to {addr}")

            # Read all messages from the file (so each new client starts from the beginning)
            messages = read_messages(FILE_NAME)
            total_msgs = len(messages)
            print(f"Loaded {total_msgs} messages from file.")

            message_index = 0

            # Send the first (N-1) messages with a 0.5s delay
            while message_index < total_msgs - 1:
                full_message = messages[message_index]
                try:
                    conn.sendall(full_message)
                    logging.debug(f"Sent message {message_index}, length={len(full_message)}")
                except BrokenPipeError:
                    print("Client disconnected unexpectedly.")
                    break

                message_index += 1
                # Wait 0.5 seconds before sending the next
                time.sleep(0.5)

            # Now send the LAST message, waiting for user input first
            if message_index == total_msgs - 1:
                # Only do this if there's indeed a last message
                print("Press [ENTER] to send the final message.")
                try:
                    user_input = sys.stdin.readline()
                    # If user closes input, break
                    if not user_input:
                        print("No more console input. Stopping.")
                        conn.close()
                        break
                except EOFError:
                    print("EOF on stdin. Stopping.")
                    conn.close()
                    break

                # Send the final message
                final_message = messages[message_index]
                try:
                    conn.sendall(final_message)
                    logging.debug(f"Sent FINAL message {message_index}, length={len(final_message)}")
                except BrokenPipeError:
                    print("Client disconnected unexpectedly.")

            conn.close()
            print("Connection closed.")

if __name__ == "__main__":
    main()

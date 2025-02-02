import gzip
from pathlib import Path
from struct import unpack
from collections import namedtuple
from datetime import datetime
import socket

# Define data path and file name as constants
DATA_PATH = Path('data')
SOURCE_FILE_ZIPPED = '01302019.NASDAQ_ITCH50.gz'  # Example zipped file
SOURCE_FILE = '01302019.NASDAQ_ITCH50.bin'  # Example unzipped file

# Ensure data directory exists
DATA_PATH.mkdir(parents=True, exist_ok=True)
FILE_NAME = DATA_PATH / SOURCE_FILE

# Check if the data file exists, if not, inform user to download (and ideally unzip)
if not FILE_NAME.exists():
    print(f"Please ensure the NASDAQ ITCH50 data file '{SOURCE_FILE}' exists in the '{DATA_PATH}' directory.")
    print(f"You may need to download and unzip '{SOURCE_FILE_ZIPPED}' to '{SOURCE_FILE}' in '{DATA_PATH}'.")
    print("Exiting program.")
    exit()

# TCP SERVER CONFIG
HOST = 'localhost'
PORT = 12345

def main():
    '''
    Main function to send ITCH file data to a TCP client
    '''
    with open(FILE_NAME, 'rb') as file, socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # Allow immediate reuse
        sock.bind((HOST, PORT))
        sock.listen(1)
        print(f"Listening on {HOST}:{PORT}")
        count = 0
        while True:
            conn, addr = sock.accept()
            print(f"Connected to {addr}")
            count = 0

            while count < 10:
                # Read message length (2 bytes, unsigned short)
                length_bytes = file.read(2)
                if not length_bytes:
                    break  # End of file

                message_length = unpack('>H', length_bytes)[0]  # Big-endian unsigned short
                message_type_code = file.read(1)  # Message Type (1 byte)
                message_data = file.read(message_length - 1)  # Remaining message data

                if not message_type_code or not message_data:
                    break  # Incomplete message, possibly end of file

                # Send the message length, type, and data to the TCP client
                try:
                    conn.sendall(length_bytes + message_type_code + message_data)
                    print(f"Sent message {count}")
                    print(f"Message: {message_type_code + message_data}")
                    count += 1
                except BrokenPipeError:
                    print("Client disconnected.")
                    break

    print("Finished sending ITCH file.")


# --- Main execution ---
if __name__ == "__main__":
    main()
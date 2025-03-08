import gzip
from pathlib import Path
from struct import unpack
from collections import namedtuple
from datetime import datetime
import socket
import logging

# Set up logging
LOGGING_LEVEL = logging.DEBUG
logging.basicConfig(level=LOGGING_LEVEL)

# Define data path and file name as constants
DATA_PATH = Path('data')
SOURCE_FILE_ZIPPED = '01302019.NASDAQ_ITCH50.gz'  # Example zipped file
SOURCE_FILE = '01302019.NASDAQ_ITCH50.bin'  # Example unzipped file

### if filtered data is available, use it ###
### run get_test_data_tcp_client.py to generate the filtered file ###
# SOURCE_FILE = '01302019.NASDAQ_ITCH50_AFECXDU.bin'  # Example filtered file

SOURCE_FILE = 'output.bin'  # Example filtered file

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
HOST = '192.168.1.11'
PORT = 22

data = [
    [    
                  0x00, 0x24, 0x41,    # Header: Length high, Length low, Type 'A'
          # Dummy bytes until byte counter reaches first meaningful offset
          0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
          0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 
          # Meaningful bytes (assuming the offsets as in your design)
          # Byte offset 0x0F (15): order_ref_num[31:24]
          0xDE, 
          # Byte offset 0x10: order_ref_num[23:16]
          0xAD, 
          # Byte offset 0x11: order_ref_num[15:8]
          0xBE, 
          # Byte offset 0x12: order_ref_num[7:0]
          0xEF,
          # Byte offset 0x13: buy_sell (0 = Buy)
          0x00,
          # Byte offset 0x14-0x17: num_shares
          0x00, 0x00, 0x10, 0x20,
          # Byte offset 0x18-0x1B: stock_id
          0x00, 0x00, 0x03, 0xE8,
          # Dummy bytes until price offset (if any gap is needed)
          0xBB, 0xBB, 0xBB, 0xBB,
          # Byte offset 0x20-0x23: price
          0x00, 0x00, 0x27, 0x10
    ], # Order Add Message
    [
        0x00, 0x28, 0x46, 0x22, 0x03, 0x00, 0x00, 0x0D, 
          0x18, 0xC4, 0x4A, 0x0A, 0xB2, 0x00, 0x00, 0x00, 
          0x00, 0x00, 0x00, 0x17, 0x84, 0x42, 0x00, 0x00, 
          0x00, 0x64, 0x5A, 0x56, 0x5A, 0x5A, 0x54, 0x20, 
          0x20, 0x20, 0x00, 0x01, 0x5F, 0x90, 0x4C, 0x45, 
          0x48, 0x4D
    ],
    [
        0x00, 0x1F, 0x45, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 
          0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0x00, 0xBB, 
          0xCC, 0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x10, 0x01, 
          0x20, 0x64, 0x5A, 0x56, 0x5A, 0x5A, 0x54, 0x20, 
          0x20
    ],
    [
        0x00, 0x24, 0x43, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 
          0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0x00, 0xBB, 
          0xCC, 0x10, 0xDB, 0x52, 0x32, 0x08, 0x30, 0x21, 
          0x20, 0x64, 0x5A, 0x56, 0x5A, 0x5A, 0x54, 0x20, 
          0x20, 0x59, 0x5A, 0x56, 0x5A, 0x5A
    ],
    [0x00, 0x24, 0x43, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 
          0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0x00, 0xBB,
          0x08, 0x10, 0xDB, 0x52, 0x32, 0xCC, 0x30, 0x21, 
          0x20, 0x64, 0x5A, 0x56, 0x5A, 0x5A, 0x54, 0x20, 
          0x20, 0x4E, 0x5A, 0x56, 0x5A, 0x5A
    ],
    [          0x00, 0x17, 0x58, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 
          0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0x00, 0xBB, 0xCC,
          0x08, 0x75, 0x35, 0x99, 0x32, 0xAA, 0x30, 0x21, 
          0x20
    ],
    [0x00, 0x13, 0x44, 0x20, 0x7E, 0x00, 0x00, 0x0D, 
            0x18, 0xC3, 0x99, 0xF5, 0x14, 0x00, 0x00, 0x00, 
            0x00, 0x00, 0x00, 0x03, 0xA8
    ]

]

expected_output = [
    "0x010000271000001020deadbeef000003e8",
    "0x0100015f9000000064000017845a565a5a",
    "0x040000000000100120cafebabe00000000",
    "0x045a565a5a0830212010db523200000000",
    "0x0",
    "0x0200000000aa3021207535993200000000",
    "0x080000000000000000000003a800000000",
]
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
            i = 0
            # while count < 10:
            while True:
                try:
                    full_message = bytes(data[i])
                    conn.sendall(full_message)
                    logging.debug(f"Sent message {i}")
                    logging.debug(f"Message: 0x{full_message.hex()}")
                    logging.debug(f"Expected: 0x{expected_output[i]}")
                    count += 1
                except BrokenPipeError:
                    print("Client disconnected.")
                    break
                # Check for user input to reset count
                if count == 1:
                    print("Press [ENTER] to continue sending messages.")
                    try:
                        if input() == '':
                            count = 0
                            i = i+1

                    except EOFError:
                        pass  # Handle end of input stream gracefully

    print("Finished sending ITCH file.")


# --- Main execution ---
if __name__ == "__main__":
    main()
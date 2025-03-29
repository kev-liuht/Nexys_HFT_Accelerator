import gzip
from pathlib import Path
from struct import unpack
from datetime import datetime
import socket
import logging
import sys
import time
import threading
import select

from ouch_parser import OUCHParser

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
# HOST = 'localhost'
# PORT = 22
def reverse_endian_bytes(data: bytes) -> bytes:
    if len(data) % 4 != 0:
        raise ValueError("Input length must be divisible by 4")
    
    result = bytearray()
    
    for i in range(0, len(data), 4):
        chunk = data[i:i+4]
        reversed_chunk = chunk[::-1]  # Reverse the byte order
        result.extend(reversed_chunk)
    
    return bytes(result)

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

def handle_incoming_message(message):
    """
    Handles an incoming message from the client.
    Decodes the message using the OUCHParser and logs the decoded message.
    """
    if len(message) != 196:
        logging.warning(f"Expected 196 bytes but got {len(message)}")
        return

    # Extract the first 4 bytes as the portfolio number
    portfolio_number = int.from_bytes(message[:4], 'big')
    logging.info(f"Portfolio number: {portfolio_number/10000}")
    # Proceed to parse every 49 bytes of the remaining data
    parser = OUCHParser()
    chunk_data = message[4:]
    chunk_size = 48
    for i in range(0, len(chunk_data), chunk_size):
        chunk = chunk_data[i:i+chunk_size]
        if len(chunk) < chunk_size:
            logging.warning(f"Incomplete OUCH message of size {len(chunk)}")
            break
        message_type_code = chunk[:1]
        message_data = chunk[1:]
        decoded_message = parser.decode_message(message_type_code, message_data)
        if decoded_message is not None:
            # parser.print_human_readable_message(decoded_message)
            # example of using the decoded message
            logging.info(f"OUCH ENTER ORDER MESSAGE: {decoded_message.Symbol}: Side={decoded_message.Side}, Quantity={decoded_message.Quantity}, Price={decoded_message.Price}")
            # logging.info(f"OUCH message Received: {decoded_message}")
        else:
            logging.warning(f"Unknown message type: {message_type_code}")




    # print(f"Received message: {message!r}")
    # print(f"Message length: {len(message)}")

def receive_nonblocking(conn):
    """
    Receives incoming messages from the client in a non-blocking manner.
    Uses select to check for available data and passes any received data
    to the message handler.
    """
    # Set socket to non-blocking mode
    conn.setblocking(False)
    while True:
        # Use select with a short timeout for non-blocking behavior
        ready_to_read, _, _ = select.select([conn], [], [], 0.1)
        if ready_to_read:
            try:
                data = conn.recv(4096)
                if data:
                    data = reverse_endian_bytes(data)
                    logging.debug(f"Received data: {data.hex()}")
                    handle_incoming_message(data)
                else:
                    # No data indicates the client closed the connection.
                    logging.debug("No data received. Client may have disconnected.")
                    break
            except BlockingIOError:
                # No data available right now
                continue
            except Exception as e:
                logging.error(f"Error while receiving data: {e}")
                break
        else:
            # No ready sockets; sleep briefly to avoid a tight loop.
            time.sleep(0.1)
    logging.debug("Exiting receive thread.")

def main():
    """
    Main function to send ITCH file data to a TCP client.
    - Sends first N-1 messages automatically with a 0.5 second delay between each.
    - Starts a thread to non-blockingly receive incoming messages from the client.
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

            # Start a thread to receive incoming messages non-blockingly
            recv_thread = threading.Thread(target=receive_nonblocking, args=(conn,), daemon=True)
            recv_thread.start()

            # Read all messages from the file (so each new client starts from the beginning)
            messages = read_messages(FILE_NAME)
            total_msgs = len(messages)
            print(f"Loaded {total_msgs} messages from file.")

            message_index = 0

            # Send the messages with a 0.5s delay, pausing every 18 messages
            while message_index < total_msgs - 1:
                full_message = messages[message_index]
                try:
                    conn.sendall(full_message)
                    logging.debug(f"Sent message {message_index}, length={len(full_message)}")
                except BrokenPipeError:
                    print("Client disconnected unexpectedly.")
                    break

                message_index += 1
                # # Check if current index is a multiple of 18 to pause
                # if message_index % 80 == 0:
                #     print("Press [ENTER] to send the next 80 messages.")
                #     try:
                #         input()  # Wait for user input
                #     except EOFError:
                #         print("EOF on stdin. Stopping.")
                #         conn.close()
                #         break

                # sleep time for send 
                # (can be modified for faster sends, but will potentialy crash the system)
                time.sleep(0.1)

            # # Now send the LAST message, waiting for user input first
            # if message_index % 18 == 0:
            #     # Only do this if there's indeed a last message
            #     print("Press [ENTER] to send the next 20 messages.")
            #     try:
            #         user_input = sys.stdin.readline()
            #         # If user closes input, break
            #         if not user_input:
            #             print("No more console input. Stopping.")
            #             conn.close()
            #             break
            #     except EOFError:
            #         print("EOF on stdin. Stopping.")
            #         conn.close()
            #         break
            #
            #     # Send the final message
            #     final_message = messages[message_index]
            #     try:
            #         conn.sendall(final_message)
            #         logging.debug(f"Sent FINAL message {message_index}, length={len(final_message)}")
            #     except BrokenPipeError:
            #         print("Client disconnected unexpectedly.")

            conn.close()
            print("Connection closed.")

if __name__ == "__main__":
    main()

import socket
import struct
from itch_parser import ITCHParser
from Orderbook import OrderBookManager
from TaParser import TaParser
from CovUpdate import CovarianceUpdateStack
from QrDecompLinSolver import QRDecompLinSolver
from OrderGen import OrderGenerator
import logging

LOGGING_LEVEL = logging.INFO  # Change to logging.INFO for less verbose logging
LOGGING_FORMAT = '%(asctime)s - %(levelname)s - %(message)s'
logging.basicConfig(format=LOGGING_FORMAT, level=LOGGING_LEVEL)

# TCP SERVER CONFIG
# SERVER_IP = '192.168.1.11'
SERVER_IP = 'localhost'
SERVER_PORT = 22
SIDE_BID = 0
SIDE_ASK = 1

def main():
    server_ip = SERVER_IP
    server_port = SERVER_PORT
    parser = ITCHParser()
    orderbook = OrderBookManager()
    ta_parser = TaParser()
    ta_cov = CovarianceUpdateStack()
    ta_qr = QRDecompLinSolver()
    ta_og = OrderGenerator()
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # >>> ADDED: Counter to track how many order-related messages we've processed.
    order_count = 0
    PUBLISH_THRESHOLD = 20

    try:
        client_socket.connect((server_ip, server_port))
        print(f"Connected to {server_ip}:{server_port}")
        num_bytes_to_skip = 0

        while True:
            data = client_socket.recv(1024 * 1024)
            if not data:
                break

            while data:
                if num_bytes_to_skip > 0:
                    if num_bytes_to_skip > len(data):
                        num_bytes_to_skip -= len(data)
                        break
                    else:
                        data = data[num_bytes_to_skip:]
                        num_bytes_to_skip = 0

                if len(data) < 2:
                    break  # incomplete data; wait for next recv

                length = int.from_bytes(data[:2], byteorder='big')
                if length > len(data[2:]):
                    logging.debug(f"Expected message length: {length}, actual length: {len(data[2:])}")
                    num_bytes_to_skip = length - len(data[2:])
                    break

                message = data[2 : 2 + length]
                message_type_code = message[0:1]
                message_data = message[1:]

                logging.debug(f"message_type_code: {message_type_code}, message length: {length}")

                decoded_message = parser.decode_message(message_type_code, message_data)
                if decoded_message is not None:
                    if LOGGING_LEVEL == logging.DEBUG:
                        parser.print_human_readable_message(decoded_message)

                    # Handle order-related messages:
                    if message_type_code in [b'A', b'F', b'E', b'C', b'X', b'D', b'U']:
                        # For demonstration, we assume a single 'stock_id' of 0
                        # If your ITCH message includes a real stock -> stock_id mapping,
                        # extract that to pass in.

                        stock_id = decoded_message.StockID
                        side = decoded_message.BuySellIndicator
                        price = decoded_message.Price
                        quantity = decoded_message.Shares
                        order_id = decoded_message.OrderReferenceNumber

                        # a) Add Order
                        if message_type_code in [b'A']:
                            orderbook.add_order(
                                stock_id=stock_id,
                                order_id=order_id,
                                price=price,
                                quantity=quantity,
                                side=side
                            )

                        # b) Cancel
                        elif message_type_code == b'X':
                            orderbook.cancel_order(
                                stock_id=stock_id,
                                order_id=order_id,
                                cancel_qty=quantity
                            )

                        # c) Execute
                        elif message_type_code == b'E':
                            orderbook.execute_order(
                                stock_id=stock_id,
                                order_id=order_id,
                                execute_qty=quantity
                            )

                        # d) Delete
                        elif message_type_code == b'D':
                            orderbook.delete_order(
                                stock_id=stock_id,
                                order_id=order_id
                            )

                        # >>> ADDED: Increase counter and check threshold
                        order_count += 1
                        if order_count >= PUBLISH_THRESHOLD:
                            # Every 20 messages, do a publish
                            # Publish the entire snapshot for all stocks
                            snapshot = orderbook.publish_snapshot()
                            logging.info("Order book: ")
                            for entry in snapshot:
                                logging.info(entry)  # or orderbook.publish_snapshot()
                            
                            # TA Parser
                            market_prices = ta_parser.update(snapshot)
                            market_prices2 = ta_parser.update2(snapshot)
                            logging.info(f"TA Parser: {market_prices}")

                            # Covariance Update
                            K, proceed = ta_cov.update(market_prices2)
                            logging.info(f"Covariance Matrix: {K}\tProceed: {proceed}")
                            
                            if proceed:
                                # QR Decomposition and linear solver
                                weights, proceed_2 = ta_qr.solve(K)
                                if proceed_2:
                                    logging.info(f"QR: Solved weights: {weights}")

                                    # Order Generation
                                    output_blob = ta_og.order_gen(weights, market_prices2, market_prices)
                                    logging.info(f"Order Generation (hex): {output_blob.hex()}")

                                    # Send the order generation msg to the server
                                    client_socket.send(output_blob)
                                    print(f"Packet sent to server.")
                                else:
                                    logging.info("division by zero occured")
                            order_count = 0

                data = data[2 + length :]

    except Exception as e:
        print(f"An error occurred: {e}")

    finally:
        client_socket.close()
        print("Connection closed")

if __name__ == "__main__":
    main()

import gzip
from pathlib import Path
from struct import unpack
from collections import namedtuple
from datetime import datetime
import socket

class ITCHParser:
    def __init__(self):
        # --- Message Definitions ---
        # Define formats based on ITCH 5.0 specification
        self.FORMATS = {
            ('integer', 2): 'H',  # Unsigned short (2 bytes)
            ('integer', 4): 'I',  # Unsigned int (4 bytes)
            ('integer', 6): '6s',  # 6 bytes - not standard integer type, often used for time
            ('integer', 8): 'Q',  # Unsigned long long (8 bytes)
            ('alpha', 1): 's',    # char (1 byte)
            ('alpha', 2): '2s',   # string (2 bytes)
            ('alpha', 4): '4s',   # string (4 bytes)
            ('alpha', 8): '8s',   # string (8 bytes)
            ('price_4', 4): 'I',  # Price (4 bytes - check precision in spec)
            ('price_8', 8): 'Q',  # Price (8 bytes - check precision in spec)
        }

        # --- Message Type Definitions ---
        self.message_definitions = {
            'S': {
                'format': '>cHH6sc',
                'fields': [
                    ('MessageType', ('alpha', 1)),
                    ('StockLocate', ('integer', 2)),
                    ('TrackingNumber', ('integer', 2)),
                    ('Timestamp', ('integer', 6)),
                    ('EventCode', ('alpha', 1)),
                ],
                'class': namedtuple('SystemEventMessage', ['MessageType', 'StockLocate', 'TrackingNumber', 'Timestamp', 'EventCode'])
            },
            'R': {
                'format': '>cHH6s8sccIcc2scccccIc',
                'fields': [
                    ('MessageType', ('alpha', 1)),
                    ('StockLocate', ('integer', 2)),
                    ('TrackingNumber', ('integer', 2)),
                    ('Timestamp', ('integer', 6)),
                    ('Stock', ('alpha', 8)),
                    ('MarketCategory', ('alpha', 1)),
                    ('FinancialStatusIndicator', ('alpha', 1)),
                    ('RoundLotSize', ('integer', 4)),
                    ('RoundLotsOnly', ('alpha', 1)),
                    ('IssueClassification', ('alpha', 1)),
                    ('IssueSubType', ('alpha', 2)),
                    ('Authenticity', ('alpha', 1)),
                    ('ShortSaleThresholdIndicator', ('alpha', 1)),
                    ('IpoFlag', ('alpha', 1)),
                    ('LuldReferencePriceTier', ('alpha', 1)),
                    ('EtpFlag', ('alpha', 1)),
                    ('EtpLeverageFactor', ('integer', 4)),
                    ('InverseIndicator', ('alpha', 1)),
                ],
                'class': namedtuple('StockDirectoryMessage', ['MessageType', 'StockLocate', 'TrackingNumber', 'Timestamp', 'Stock', 'MarketCategory', 'FinancialStatusIndicator', 'RoundLotSize', 'RoundLotsOnly', 'IssueClassification', 'IssueSubType', 'Authenticity', 'ShortSaleThresholdIndicator', 'IpoFlag', 'LuldReferencePriceTier', 'EtpFlag', 'EtpLeverageFactor', 'InverseIndicator'])
            },
            'A': {
                'format': '>cHH6sQsI8sI',
                'fields': [
                    ('MessageType', ('alpha', 1)),
                    ('StockLocate', ('integer', 2)),
                    ('TrackingNumber', ('integer', 2)),
                    ('Timestamp', ('integer', 6)),
                    ('OrderReferenceNumber', ('integer', 8)),
                    ('BuySellIndicator', ('alpha', 1)),
                    ('Shares', ('integer', 4)),
                    ('Stock', ('alpha', 8)),
                    ('Price', ('price_4', 4)),
                ],
                'class': namedtuple('AddOrderMessage', ['MessageType', 'StockLocate', 'TrackingNumber', 'Timestamp', 'OrderReferenceNumber', 'BuySellIndicator', 'Shares', 'Stock', 'Price'])
            },
            'X': {
                'format': '>cHIQI',
                'fields': [
                    ('MessageType', ('alpha', 1)),
                    ('StockLocate', ('integer', 2)),
                    ('TrackingNumber', ('integer', 2)),
                    ('Timestamp', ('integer', 8)),
                    ('OrderReferenceNumber', ('integer', 8)),
                    ('SharesToCancel', ('integer', 4)),
                ],
                'class': namedtuple('OrderCancelMessage', ['MessageType', 'StockLocate', 'TrackingNumber', 'Timestamp', 'OrderReferenceNumber', 'SharesToCancel'])
            },
            'D': {
                'format': '>cHH6sQ',
                'fields': [
                    ('MessageType', ('alpha', 1)),
                    ('StockLocate', ('integer', 2)),
                    ('TrackingNumber', ('integer', 2)),
                    ('Timestamp', ('integer', 6)),
                    ('OrderReferenceNumber', ('integer', 8)),
                ],
                'class': namedtuple('OrderDeleteMessage', ['MessageType', 'StockLocate', 'TrackingNumber', 'Timestamp', 'OrderReferenceNumber'])
            },
            'E': {
                'format': '>cHH6sQIQ',
                'fields': [
                    ('MessageType', ('alpha', 1)),
                    ('StockLocate', ('integer', 2)),
                    ('TrackingNumber', ('integer', 2)),
                    ('Timestamp', ('integer', 6)),
                    ('OrderReferenceNumber', ('integer', 8)),
                    ('ExecutedShares', ('integer', 4)),
                    ('MatchNumber', ('integer', 8)),
                ],
                'class': namedtuple('OrderExecutedMessage', ['MessageType', 'StockLocate', 'TrackingNumber', 'Timestamp', 'OrderReferenceNumber', 'ExecutedShares', 'MatchNumber'])
            }
        }

    def convert_time(self, stamp: bytes):
        time = datetime.fromtimestamp(int.from_bytes(stamp, byteorder='big') * 1e-9)
        return time.strftime('%H:%M:%S.%f')

    def decode_message(self, message_type_code: bytes, message_data: bytes):
        """
        Decodes a raw message based on its type code.

        Args:
            message_type_code: The message type code (single byte).
            message_data: The raw message data (bytes).

        Returns:
            A namedtuple representing the decoded message, or None if the type is unknown.
        """
        message_type = message_type_code.decode('ascii')
        if message_type not in self.message_definitions:
            return None  # Unknown message type

        definition = self.message_definitions[message_type]
        format_string = definition['format']
        fields_def = definition['fields']
        message_class = definition['class']

        # Prepend the message type to the message data so that the unpacked buffer matches the format.
        full_message = message_type_code + message_data
        try:
            message_unpacked = unpack(format_string, full_message)
        except Exception as e:
            print(f"Error unpacking message type '{message_type}': {e}")
            return None

        kwargs = {}
        for i, field in enumerate(fields_def):
            name = field[0]
            format_type = field[1]
            value = message_unpacked[i]

            if format_type[0] == 'alpha':
                try:
                    kwargs[name] = value.decode('ascii').strip()  # Decode bytes to string for alpha fields
                except UnicodeDecodeError:
                    kwargs[name] = value  # Keep as bytes if decoding fails
            elif format_type[0] in ('price_4', 'price_8'):
                kwargs[name] = value / 10000  # Assuming 4 decimal places for price; adjust if needed
            else:
                kwargs[name] = value

        return message_class(**kwargs)

    def print_human_readable_message(self, message):
        """
        Prints a decoded message in a human-readable format.

        Args:
            message: A namedtuple representing a decoded message.
        """
        if message:
            message_type = type(message).__name__
            print(f"--- {message_type} ---")
            for field_name in message._fields:
                if field_name == 'Timestamp':
                    value = self.convert_time(getattr(message, field_name))
                else:
                    value = getattr(message, field_name)
                print(f"  {field_name}: {value}")
        else:
            print("Unknown Message Type or Decoding Error")

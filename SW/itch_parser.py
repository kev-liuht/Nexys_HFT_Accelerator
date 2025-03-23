#!/usr/bin/env python3
from struct import unpack
from collections import namedtuple
from datetime import datetime

class ITCHParser:
    def __init__(self):
        # For demonstration, we keep the other message definitions as-is or remove them.
        # We focus on 'A' since that's what your encoder writes.

        self.message_definitions = {

            # -----------------------------------------------------------------
            # CUSTOM 'A' MESSAGE to match your 36-byte layout
            # -----------------------------------------------------------------
            #
            # Layout after 2-byte length:
            #   1 byte  -> MessageType (e.g. 'A')
            #   14 bytes-> Dummy1 (0xAA)
            #   4 bytes -> OrderReferenceNumber
            #   1 byte  -> BuySellIndicator (0=Buy or 1=Sell)
            #   4 bytes -> Shares
            #   4 bytes -> StockID
            #   4 bytes -> Dummy2 (0xBB)
            #   4 bytes -> Price
            #
            # => total = 36 bytes
            #
            'A': {
                'format': '>c14sIBII4sI',
                'fields': [
                    ('MessageType',           ('alpha',   1)),  # e.g. 'A'
                    ('Dummy1',                ('alpha',  14)),  # 14 filler bytes
                    ('OrderReferenceNumber',  ('integer', 4)),
                    ('BuySellIndicator',      ('alpha',   1)),  # can decode as '\x00' or '\x01'
                    ('Shares',                ('integer', 4)),
                    ('StockID',               ('integer', 4)),
                    ('Dummy2',                ('alpha',   4)),  # 4 filler bytes
                    ('Price',                 ('integer', 4)),
                ],
                'class': namedtuple(
                    'AddOrderMessage',
                    [
                        'MessageType',
                        'Dummy1',
                        'OrderReferenceNumber',
                        'BuySellIndicator',
                        'Shares',
                        'StockID',
                        'Dummy2',
                        'Price'
                    ]
                )
            },
            'E': {
                'format': '>c14sIBII4sI',
                'fields': [
                    ('MessageType', ('alpha', 1)),  # e.g. 'A'
                    ('Dummy1', ('alpha', 14)),  # 14 filler bytes
                    ('OrderReferenceNumber', ('integer', 4)),
                    ('BuySellIndicator', ('alpha', 1)),  # can decode as '\x00' or '\x01'
                    ('Shares', ('integer', 4)),
                    ('StockID', ('integer', 4)),
                    ('Dummy2', ('alpha', 4)),  # 4 filler bytes
                    ('Price', ('integer', 4)),
                ],
                'class': namedtuple(
                    'AddOrderMessage',
                    [
                        'MessageType',
                        'Dummy1',
                        'OrderReferenceNumber',
                        'BuySellIndicator',
                        'Shares',
                        'StockID',
                        'Dummy2',
                        'Price'
                    ]
                )
            },
            'D': {
                'format': '>c14sIBII4sI',
                'fields': [
                    ('MessageType', ('alpha', 1)),  # e.g. 'A'
                    ('Dummy1', ('alpha', 14)),  # 14 filler bytes
                    ('OrderReferenceNumber', ('integer', 4)),
                    ('BuySellIndicator', ('alpha', 1)),  # can decode as '\x00' or '\x01'
                    ('Shares', ('integer', 4)),
                    ('StockID', ('integer', 4)),
                    ('Dummy2', ('alpha', 4)),  # 4 filler bytes
                    ('Price', ('integer', 4)),
                ],
                'class': namedtuple(
                    'AddOrderMessage',
                    [
                        'MessageType',
                        'Dummy1',
                        'OrderReferenceNumber',
                        'BuySellIndicator',
                        'Shares',
                        'StockID',
                        'Dummy2',
                        'Price'
                    ]
                )
            },
            'X': {
                'format': '>c14sIBII4sI',
                'fields': [
                    ('MessageType', ('alpha', 1)),  # e.g. 'A'
                    ('Dummy1', ('alpha', 14)),  # 14 filler bytes
                    ('OrderReferenceNumber', ('integer', 4)),
                    ('BuySellIndicator', ('alpha', 1)),  # can decode as '\x00' or '\x01'
                    ('Shares', ('integer', 4)),
                    ('StockID', ('integer', 4)),
                    ('Dummy2', ('alpha', 4)),  # 4 filler bytes
                    ('Price', ('integer', 4)),
                ],
                'class': namedtuple(
                    'AddOrderMessage',
                    [
                        'MessageType',
                        'Dummy1',
                        'OrderReferenceNumber',
                        'BuySellIndicator',
                        'Shares',
                        'StockID',
                        'Dummy2',
                        'Price'
                    ]
                )
            },
            # ... (you can define other messages if you wish)
        }

    def convert_time(self, stamp: bytes):
        # Not relevant for the custom 'A' message, but included for completeness
        time_val = int.from_bytes(stamp, byteorder='big')  # e.g., nanosec
        dt = datetime.fromtimestamp(time_val * 1e-9)
        return dt.strftime('%H:%M:%S.%f')

    def decode_message(self, message_type_code: bytes, message_data: bytes):
        """
        Decodes a raw message based on its type code.
        The first byte of message_data is appended to unify the structure for unpacking.
        """
        message_type = message_type_code.decode('ascii', errors='ignore')
        if message_type not in self.message_definitions:
            # Unknown or unsupported message
            return None

        definition = self.message_definitions[message_type]
        fmt = definition['format']
        fields_def = definition['fields']
        message_class = definition['class']

        # Combine the 1-byte message_type with message_data
        full_message = message_type_code + message_data  # e.g. 1 + 35 = 36 bytes

        try:
            unpacked = unpack(fmt, full_message)
        except Exception as e:
            print(f"Error unpacking message type '{message_type}': {e}")
            return None

        # Map each unpacked field to the named fields
        kwargs = {}
        for i, (field_name, (type_kind, _)) in enumerate(fields_def):
            val = unpacked[i]
            if type_kind == 'alpha':
                # Try to decode as ASCII
                try:
                    val = val.decode('ascii', errors='ignore').strip()
                except:
                    pass
            elif type_kind.startswith('price_'):
                # If you had a 'price_4' or 'price_8' you might do val / 10000
                pass
            # else type 'integer', we just keep val as is
            kwargs[field_name] = val

        return message_class(**kwargs)

    def print_human_readable_message(self, message):
        """
        Prints a decoded message in a human-readable format.
        """
        if message is None:
            print("Unknown or invalid message.")
            return

        msg_type = type(message).__name__
        print(f"--- {msg_type} ---")
        for field_name in message._fields:
            value = getattr(message, field_name)
            print(f"  {field_name}: {value}")


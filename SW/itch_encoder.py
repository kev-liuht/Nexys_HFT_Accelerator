import csv
import sys
import os


def csv_to_bin(csv_file_path, bin_file_path):
    """
    Reads rows from the CSV and writes a .bin file with
    the specific byte structure required for your ITCH-like messages.

    CSV columns:
        Header        - single character, e.g. 'A'
        order_ref_num - integer (fits in 4 bytes)
        buy_sell      - 'B' or 'S'
        num_shares    - integer (fits in 4 bytes)
        stock_id      - integer (fits in 4 bytes)
        price         - integer (fits in 4 bytes)
    """

    # Open the CSV and the output bin file
    with open(csv_file_path, mode='r', newline='') as csv_file, \
            open(bin_file_path, mode='wb') as bin_file:

        reader = csv.DictReader(csv_file)

        for row in reader:
            # Parse fields from CSV
            header_char = row['Header'].strip()  # 'A', 'B', ...
            order_ref_num = int(row['order_ref_num'])
            buy_sell_flag = row['buy_sell'].strip()  # 'B' or 'S'
            num_shares = int(row['num_shares'])
            stock_id = int(row['stock_id'])
            price = int(row['price'])

            # Convert buy_sell to the byte value (0 = Buy, 1 = Sell)
            if buy_sell_flag.upper() == 'B':
                buy_sell_val = 0
            else:
                buy_sell_val = 1

            ############################################
            # Build the byte array of length 38 (as in example)
            ############################################

            # 0) Pre-allocate 38 zeroed bytes
            msg = [0x00] * 38

            # 1) Bytes [0..1] => length (high byte, low byte)
            #    In your example, 0x00, 0x24 (which is decimal 36),
            #    but your example data actually totals 38 bytes.
            #    You can keep it as 0x00, 0x24 to match your snippet exactly.
            msg[0] = 0x00
            msg[1] = 0x24  # 36 decimal, matching your snippet

            # 2) Byte [2] => ASCII code for Header (e.g. 'A' -> 0x41)
            msg[2] = ord(header_char)

            # 3) Bytes [3..16] => dummy 0xAA
            #    (That's 14 bytes: indices 3..16 inclusive is 14 positions)
            for i in range(3, 17):
                msg[i] = 0xAA

            # 4) Bytes [17..20] => order_ref_num in big-endian
            #    i.e. order_ref_num[31:24], [23:16], [15:8], [7:0]
            msg[17] = (order_ref_num >> 24) & 0xFF
            msg[18] = (order_ref_num >> 16) & 0xFF
            msg[19] = (order_ref_num >> 8) & 0xFF
            msg[20] = (order_ref_num) & 0xFF

            # 5) Byte [21] => buy_sell (0=Buy,1=Sell)
            msg[21] = buy_sell_val

            # 6) Bytes [22..25] => num_shares (big-endian)
            msg[22] = (num_shares >> 24) & 0xFF
            msg[23] = (num_shares >> 16) & 0xFF
            msg[24] = (num_shares >> 8) & 0xFF
            msg[25] = (num_shares) & 0xFF

            # 7) Bytes [26..29] => stock_id (big-endian)
            msg[26] = (stock_id >> 24) & 0xFF
            msg[27] = (stock_id >> 16) & 0xFF
            msg[28] = (stock_id >> 8) & 0xFF
            msg[29] = (stock_id) & 0xFF

            # 8) Bytes [30..33] => dummy 0xBB
            for i in range(30, 34):
                msg[i] = 0xBB

            # 9) Bytes [34..37] => price (big-endian)
            msg[34] = (price >> 24) & 0xFF
            msg[35] = (price >> 16) & 0xFF
            msg[36] = (price >> 8) & 0xFF
            msg[37] = (price) & 0xFF

            # Convert to bytes and write to file
            bin_file.write(bytes(msg))

    print(f"Binary data successfully written to '{bin_file_path}'")


def main():


    csv_file_path = 'data/test.csv'
    bin_file_path = 'data/output.bin'

    # Ensure CSV exists
    if not os.path.isfile(csv_file_path):
        print(f"Error: CSV file '{csv_file_path}' does not exist.")
        sys.exit(1)

    csv_to_bin(csv_file_path, bin_file_path)


if __name__ == "__main__":
    main()




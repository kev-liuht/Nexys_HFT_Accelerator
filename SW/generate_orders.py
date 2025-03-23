import pandas as pd
import random
import csv
import os

def generate_orders_from_config(config_csv_path, output_csv_path, num_orders_per_config=41):
    """
    Reads a configuration CSV specifying stock_id, buy/sell, min_price, max_price, tick.
    Generates random orders and writes them to output_csv_path in the specified format.
    """
    # Read the configuration
    config_df = pd.read_csv(config_csv_path)

    # Prepare a list to hold final rows
    final_rows = []

    # We'll keep an incrementing order_ref_num
    order_ref = 1

    # For each config row, generate 'num_orders_per_config' random orders
    for idx, row in config_df.iterrows():
        stock_id = row["stock_id"]
        side = row["buy_sell"]  # 'B' or 'S'
        min_price = int(row["min_price"])
        max_price = int(row["max_price"])
        tick = int(row["tick"])

        # Build a list of valid prices to choose from (all multiples of tick between min and max)
        valid_prices = range(min_price, max_price + 1, tick)

        # Generate orders
        for _ in range(num_orders_per_config):
            # Choose a random price from valid_prices
            price = random.choice(list(valid_prices))
            # Random quantity
            num_shares = random.randint(10, 100)

            # Build one row of data. Set Header to "A" (can be changed as needed)
            row_data = [
                "A",            # Header
                order_ref,      # order_ref_num
                side,           # buy_sell
                num_shares,     # num_shares
                stock_id,       # stock_id
                price           # price
            ]
            final_rows.append(row_data)
            order_ref += 1

    # Shuffle the entire list so the rows are in fully random order
    random.shuffle(final_rows)

    # Write the final CSV file with the required header
    with open(output_csv_path, mode="w", newline="") as f:
        writer = csv.writer(f)
        # Write column headers
        writer.writerow(["Header", "order_ref_num", "buy_sell", "num_shares", "stock_id", "price"])
        # Write all rows
        writer.writerows(final_rows)


if __name__ == "__main__":
    # Example usage:
    config_csv = "data/config.csv"  # path to your config (stock_id, buy_sell, min_price, max_price, tick)
    output_csv = "data/test.csv"    # where you want the generated orders to go

    generate_orders_from_config(config_csv, output_csv, num_orders_per_config=100)
    print(f"Generated random orders saved to: {output_csv}")

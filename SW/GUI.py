import time
import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os

def read_ouch_csv(csv_path):
    """
    Reads the entire CSV of OUCH events.
    Returns a DataFrame with columns:
      Timestamp, Portfolio, Symbol, Side, Quantity, Price
    """
    if not os.path.isfile(csv_path):
        # Return empty DataFrame if not found
        return pd.DataFrame(columns=["Timestamp","Portfolio","Symbol","Side","Quantity","Price"])
    df = pd.read_csv(csv_path)
    return df

def compute_portfolio(df):
    """
    Given a DataFrame of 'Enter Order' events,
    compute final net holdings (shares) of each symbol.

    We'll assume:
     - 'B' => buy => shares += Quantity
     - 'S' => sell => shares -= Quantity
     - This function does NOT track partial fills or realized P/L.
    """
    holdings = {}
    for _, row in df.iterrows():
        symbol = row["Symbol"]
        side   = row["Side"]
        qty    = int(row["Quantity"])
        price  = float(row["Price"])

        if symbol not in holdings:
            holdings[symbol] = {"shares": 0, "cost": 0.0}

        if side == "B":
            holdings[symbol]["shares"] += qty
            holdings[symbol]["cost"]   += qty * price
        elif side == "S":
            holdings[symbol]["shares"] -= qty
            holdings[symbol]["cost"]   -= qty * price

    return holdings

def plot_holdings(holdings):
    """
    Bar chart of net shares by symbol (current snapshot).
    """
    if not holdings:
        print("No holdings to plot.")
        return

    symbols = list(holdings.keys())
    share_counts = [holdings[s]["shares"] for s in symbols]

    plt.bar(symbols, share_counts)
    plt.title("Current Holdings by Symbol")
    plt.xlabel("Symbol")
    plt.ylabel("Net Shares")

def compute_holdings_over_time(df):
    """
    Builds a DataFrame indexed by Timestamp with columns = symbols,
    representing net shares over time.

    Steps:
      1) Convert 'Timestamp' to datetime
      2) Sort by time
      3) Accumulate net shares after each trade row
    """
    if df.empty:
        return pd.DataFrame()

    df = df.copy()
    df["Timestamp"] = pd.to_datetime(df["Timestamp"])
    df = df.sort_values("Timestamp").reset_index(drop=True)

    running_shares = {}
    snapshots = []

    for _, row in df.iterrows():
        symbol = row["Symbol"]
        side   = row["Side"]
        qty    = int(row["Quantity"])

        if symbol not in running_shares:
            running_shares[symbol] = 0

        if side == "B":
            running_shares[symbol] += qty
        else:  # side == "S"
            running_shares[symbol] -= qty

        # snapshot after this row
        snapshot_dict = dict(running_shares)  # copy current state
        snapshot_dict["Timestamp"] = row["Timestamp"]
        snapshots.append(snapshot_dict)

    history_df = pd.DataFrame(snapshots)
    history_df = history_df.set_index("Timestamp").fillna(0)
    return history_df

def plot_portfolio_over_time(history_df):
    """
    Plots lines of net shares over time for each symbol.
    """
    if history_df.empty:
        print("No time history to plot.")
        return

    for symbol in history_df.columns:
        plt.plot(history_df.index, history_df[symbol], label=symbol)

    plt.title("Net Shares Over Time")
    plt.xlabel("Time")
    plt.ylabel("Shares")
    plt.legend(loc="best")

def plot_total_portfolio(df):
    """
    Plots the 'Portfolio' column over time as a single line.
    We assume the CSV file's 'Portfolio' column is your total
    holdings in dollars at each row.

    If your 'Portfolio' column is instead a code/ID, adjust accordingly.
    """
    if df.empty:
        print("No portfolio data to plot.")
        return

    df2 = df.copy()
    df2["Timestamp"] = pd.to_datetime(df2["Timestamp"])
    df2.sort_values("Timestamp", inplace=True)

    # Convert Portfolio to float (if needed)
    df2["Portfolio"] = df2["Portfolio"].astype(float)

    plt.plot(df2["Timestamp"], df2["Portfolio"], label="Total Portfolio")
    plt.title("Total Portfolio Value Over Time")
    plt.xlabel("Time")
    plt.ylabel("Portfolio ($)")
    plt.legend(loc="best")

def compute_prices_over_time(df):
    """
    For each Symbol, track how its Price changes over time and
    build a time series DataFrame. Each row is the timestamp of
    a new event, with columns for each symbol's most recent price.
    """
    if df.empty:
        return pd.DataFrame()

    df = df.copy()
    df["Timestamp"] = pd.to_datetime(df["Timestamp"])
    df.sort_values("Timestamp", inplace=True)

    # Keep track of the most recent price for each symbol
    last_prices = {}
    snapshots = []

    for _, row in df.iterrows():
        symbol = row["Symbol"]
        price  = float(row["Price"])

        # Update the current symbol's latest price
        last_prices[symbol] = price

        # Record a snapshot of all known prices so far
        snapshot_dict = dict(last_prices)
        snapshot_dict["Timestamp"] = row["Timestamp"]
        snapshots.append(snapshot_dict)

    price_df = pd.DataFrame(snapshots)
    price_df = price_df.set_index("Timestamp")
    # Forward-fill in case we want a continuous line
    price_df = price_df.fillna(method="ffill").fillna(0)
    return price_df

def plot_price_over_time(price_df):
    """
    Line chart showing each symbol's price over time, but ONLY
    for data points above $100. Points at or below $100 are excluded.
    """
    if price_df.empty:
        print("No price data to plot.")
        return

    for symbol in price_df.columns:
        # Create a mask that selects rows where price_df[symbol] is > 100
        mask = price_df[symbol] > 100
        # Plot only the subset of data above $100
        plt.plot(price_df.index[mask], price_df[symbol][mask], label=symbol)

    plt.title("Symbol Prices Over Time ")
    plt.xlabel("Time")
    plt.ylabel("Price")
    plt.legend(loc="best")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", default="ouch_events.csv", help="Path to the OUCH events CSV.")
    parser.add_argument("--interval", type=float, default=2.0, help="Polling interval in seconds.")
    args = parser.parse_args()

    plt.ion()  # interactive mode
    plt.show()

    last_count = 0  # how many rows we processed last time

    while True:
        df = read_ouch_csv(args.csv)
        row_count = len(df)

        # Only update the plots if we've got at least 4 new rows
        if (row_count - last_count) >= 4:
            print(f"Detected {row_count - last_count} new rows, updating plots...")
            last_count = row_count

            # 1) Compute the final net holdings for bar chart
            holdings = compute_portfolio(df)

            # 2) Compute net shares over time
            history_df = compute_holdings_over_time(df)

            # 3) Compute prices over time
            price_df = compute_prices_over_time(df)

            # Clear the figure
            plt.clf()

            # ----------------------------
            # Subplot (1) - Current Holdings (bar chart)
            # ----------------------------
            plt.subplot(4, 1, 1)
            plot_holdings(holdings)

            # ----------------------------
            # Subplot (2) - Net Shares Over Time (line chart)
            # ----------------------------
            plt.subplot(4, 1, 2)
            plot_portfolio_over_time(history_df)

            # ----------------------------
            # Subplot (3) - Total Portfolio Over Time (line chart)
            # 'Portfolio' column in the CSV
            # ----------------------------
            plt.subplot(4, 1, 3)
            plot_total_portfolio(df)

            # ----------------------------
            # Subplot (4) - Price Over Time (line chart)
            # ----------------------------
            plt.subplot(4, 1, 4)
            plot_price_over_time(price_df)

            plt.tight_layout()
            plt.pause(0.01)  # refresh

        time.sleep(args.interval)

if __name__ == "__main__":
    main()

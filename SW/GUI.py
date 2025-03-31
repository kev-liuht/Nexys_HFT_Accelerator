import time
import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os

def read_ouch_csv(csv_path):
    if not os.path.isfile(csv_path):
        # Return empty DataFrame if not found
        return pd.DataFrame(columns=["Timestamp","Portfolio","Symbol","Side","Quantity","Price"])
    df = pd.read_csv(csv_path)
    return df

def compute_portfolio(df):
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

def plot_holdings(ax, holdings):
    """
    Bar chart of net shares by symbol (current snapshot),
    drawn on the given Axes object 'ax'.
    """
    ax.clear()
    ax.set_title("Current Holdings by Symbol")
    ax.set_xlabel("Symbol")
    ax.set_ylabel("Net Shares")

    if not holdings:
        ax.text(0.5, 0.5, "No holdings", ha="center", va="center", transform=ax.transAxes)
        return

    symbols = list(holdings.keys())
    share_counts = [holdings[s]["shares"] for s in symbols]
    ax.bar(symbols, share_counts)

def compute_holdings_over_time(df):
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

        snapshot_dict = dict(running_shares)  # copy current state
        snapshot_dict["Timestamp"] = row["Timestamp"]
        snapshots.append(snapshot_dict)

    history_df = pd.DataFrame(snapshots)
    history_df = history_df.set_index("Timestamp").fillna(0)
    return history_df

def plot_portfolio_over_time(ax, history_df):
    ax.clear()
    ax.set_title("Net Shares Over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Shares")

    if history_df.empty:
        ax.text(0.5, 0.5, "No data", ha="center", va="center", transform=ax.transAxes)
        return

    for symbol in history_df.columns:
        ax.plot(history_df.index, history_df[symbol], label=symbol)
    ax.legend(loc="best")

def plot_total_portfolio(ax, df):
    ax.clear()
    ax.set_title("Total Portfolio Value Over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Portfolio ($)")

    if df.empty:
        ax.text(0.5, 0.5, "No portfolio data", ha="center", va="center", transform=ax.transAxes)
        return

    df2 = df.copy()
    df2["Timestamp"] = pd.to_datetime(df2["Timestamp"])
    df2.sort_values("Timestamp", inplace=True)

    # Convert Portfolio to float (if needed)
    df2["Portfolio"] = df2["Portfolio"].astype(float, errors="ignore")

    ax.plot(df2["Timestamp"], df2["Portfolio"], label="Total Portfolio")
    ax.legend(loc="best")

def compute_prices_over_time(df):
    if df.empty:
        return pd.DataFrame()

    df = df.copy()
    df["Timestamp"] = pd.to_datetime(df["Timestamp"])
    df.sort_values("Timestamp", inplace=True)

    last_prices = {}
    snapshots = []

    for _, row in df.iterrows():
        symbol = row["Symbol"]
        price  = float(row["Price"])
        last_prices[symbol] = price

        snapshot_dict = dict(last_prices)
        snapshot_dict["Timestamp"] = row["Timestamp"]
        snapshots.append(snapshot_dict)

    price_df = pd.DataFrame(snapshots)
    price_df = price_df.set_index("Timestamp")
    # Forward-fill
    price_df = price_df.fillna(method="ffill").fillna(0)
    return price_df

def plot_price_over_time(ax, price_df):
    ax.clear()
    ax.set_title("Symbol Prices Over Time (Above $100)")
    ax.set_xlabel("Time")
    ax.set_ylabel("Price")

    if price_df.empty:
        ax.text(0.5, 0.5, "No price data", ha="center", va="center", transform=ax.transAxes)
        return

    for symbol in price_df.columns:
        mask = price_df[symbol] > 100
        ax.plot(price_df.index[mask], price_df[symbol][mask], label=symbol)

    ax.legend(loc="best")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", default="ouch_events.csv", help="Path to the OUCH events CSV.")
    parser.add_argument("--interval", type=float, default=2.0, help="Polling interval in seconds.")
    args = parser.parse_args()

    # Turn on interactive mode
    plt.ion()

    # Create two figures: fig1 for the first three charts, fig2 for price-over-time
    fig1 = plt.figure("Main Figures", figsize=(8, 10))
    fig2 = plt.figure("Price Over Time", figsize=(8, 4))

    # Create Axes in fig1 (3 subplots stacked vertically)
    ax1 = fig1.add_subplot(3,1,1)  # holdings
    ax2 = fig1.add_subplot(3,1,2)  # net shares over time
    ax3 = fig1.add_subplot(3,1,3)  # total portfolio over time

    # Create Axes in fig2 (just 1 subplot)
    ax4 = fig2.add_subplot(1,1,1)  # price-over-time

    plt.show()  # Show both windows

    last_count = 0  # how many rows we processed last time

    while True:
        df = read_ouch_csv(args.csv)
        row_count = len(df)

        # Only update the plots if we've got at least 4 new rows
        if (row_count - last_count) >= 4:
            print(f"Detected {row_count - last_count} new rows, updating plots...")
            last_count = row_count

            # 1) Compute final net holdings
            holdings = compute_portfolio(df)

            # 2) Compute net shares over time
            history_df = compute_holdings_over_time(df)

            # 3) Compute prices over time
            price_df = compute_prices_over_time(df)

            # Update Figure 1
            plt.figure(fig1.number)
            plot_holdings(ax1, holdings)
            plot_portfolio_over_time(ax2, history_df)
            plot_total_portfolio(ax3, df)
            fig1.tight_layout()

            # Update Figure 2
            plt.figure(fig2.number)
            plot_price_over_time(ax4, price_df)
            fig2.tight_layout()

            # Refresh both figures
            plt.pause(0.01)

        time.sleep(args.interval)

if __name__ == "__main__":
    main()

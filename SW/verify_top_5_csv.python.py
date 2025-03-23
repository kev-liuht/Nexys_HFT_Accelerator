import pandas as pd


def get_top_5_prices(csv_file_path):
    # 1. Read the CSV file
    df = pd.read_csv(csv_file_path)

    # 2. Group by (stock_id, buy_sell, price) and sum num_shares
    group_cols = ["stock_id", "buy_sell", "price"]
    agg_df = (
        df.groupby(group_cols, as_index=False)
            .agg({"num_shares": "sum"})
    )

    # 3. For each stock_id and side, pick top 5
    results = []
    for stock_id in agg_df["stock_id"].unique():
        # Filter out just this stock_id
        df_stock = agg_df[agg_df["stock_id"] == stock_id]

        # Separate buy (B) and sell (S)
        df_buy = df_stock[df_stock["buy_sell"] == "B"].copy()
        df_sell = df_stock[df_stock["buy_sell"] == "S"].copy()

        # For buys, sort descending by price and take top 5
        df_buy.sort_values(by="price", ascending=False, inplace=True)
        df_buy_top5 = df_buy.head(5)

        # For sells, sort ascending by price and take top 5
        df_sell.sort_values(by="price", ascending=True, inplace=True)
        df_sell_top5 = df_sell.head(5)

        results.append((stock_id, 'B', df_buy_top5))
        results.append((stock_id, 'S', df_sell_top5))

    # 4. Print results
    for stock_id, side, subset in results:
        if subset.empty:
            continue
        side_name = "BUY" if side == "B" else "SELL"
        print(f"\n=== Stock ID: {stock_id}, {side_name} ===")
        for _, row in subset.iterrows():
            print(f"Price = {row['price']}, Quantity = {row['num_shares']}")


if __name__ == "__main__":
    # Provide the path to your CSV file here
    csv_path = "data/test.csv"
    get_top_5_prices(csv_path)
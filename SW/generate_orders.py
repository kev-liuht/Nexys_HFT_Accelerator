import pandas as pd
import random
import csv
import math

def generate_orders_injected_cancels(
    config_csv_path,
    output_csv_path,
    num_orders_per_config=5,
    max_injected_events_after_each_add=3,
    random_seed=None
):
    """
    1) For each row in config, generate `num_orders_per_config` 'A' (Add) orders
       with a price determined by linear interpolation between `start_price_a`
       and `start_price_b`, plus a Gaussian noise of `noise_pct`%. The order_ref_num
       is ascending (1,2,3,...) for every new Add.

    2) Immediately after each 'A', randomly insert up to `max_injected_events_after_each_add`
       partial-cancel (X/E) or delete (D) events referencing any active order.

    3) Write all events (A, X, E, D) to `output_csv_path` in the order generated.
    """

    if random_seed is not None:
        random.seed(random_seed)

    # Read your config CSV
    config_df = pd.read_csv(config_csv_path)

    # This is the final list of events in the exact order they are generated
    final_events = []

    # We'll assign a strictly ascending order_ref_num to each new "A" event:
    next_order_ref = 1

    # For tracking each order's state:
    # active_orders[order_ref] = {
    #     "stock_id": str or int,
    #     "buy_sell": "B" or "S",
    #     "remaining_shares": int,
    #     "deleted": bool
    # }
    active_orders = {}

    def create_event(
        header_type,
        order_ref_num,
        side,
        num_shares,
        stock_id,
        price
    ):
        """Helper to build a row in the required format."""
        return [
            header_type,     # e.g. 'A', 'X', 'E', or 'D'
            order_ref_num,
            side,
            num_shares,
            stock_id,
            price
        ]

    # ---------------------------------------------------------------
    # Main logic: for each row in config, we generate `num_orders_per_config`
    # 'A' events. The price for each 'A' is determined by linear interpolation
    # from start_price_a -> start_price_b, plus Gaussian noise of noise_pct%.
    # ---------------------------------------------------------------
    for _, cfg_row in config_df.iterrows():
        stock_id = cfg_row["stock_id"]
        side = cfg_row["buy_sell"]      # 'B' or 'S'
        min_price = int(cfg_row["min_price"])
        max_price = int(cfg_row["max_price"])
        tick = int(cfg_row["tick"])

        # New columns for linear + noise
        start_price_a = float(cfg_row["start_price_a"])
        start_price_b = float(cfg_row["start_price_b"])
        noise_pct = float(cfg_row["noise_pct"])  # e.g. 5.0 means 5%

        # Generate 'num_orders_per_config' prices via linear interpolation
        # from start_price_a to start_price_b.
        # Example: if num_orders_per_config=4,
        # i = 0 -> fraction=0.0 -> price = start_price_a
        # i = 1 -> fraction=0.33 -> ...
        # i = 2 -> fraction=0.66 -> ...
        # i = 3 -> fraction=1.0 -> price = start_price_b
        for i in range(num_orders_per_config):
            fraction = i / max(num_orders_per_config - 1, 1)  # avoids div-by-zero if =1
            base_price = start_price_a + (start_price_b - start_price_a) * fraction

            # Add Gaussian noise. The stdev is (noise_pct% of current base_price).
            stdev = base_price * (noise_pct / 100.0)
            noise = random.gauss(0, stdev) if stdev > 0 else 0.0

            # Final price, rounded and optionally clamped to [min_price, max_price]
            raw_price = base_price + noise
            final_price = int(round(raw_price))

            # If you still want to enforce min/max bounds (optional):
            final_price = max(min_price, min(max_price, final_price))

            # Now ensure the final_price is aligned to 'tick' if you want (optional)
            # One approach is to move final_price to the nearest multiple of tick:
            #   final_price = (final_price // tick) * tick
            # but that can cause all sorts of collisions if tick is large. Do as needed.
            # For example:
            final_price = (final_price // tick) * tick

            # 1) Create the A event
            order_ref = next_order_ref
            next_order_ref += 1

            shares = random.randint(10, 100)  # or pick a shares logic as you wish

            a_event = create_event(
                header_type="A",
                order_ref_num=order_ref,
                side=side,
                num_shares=shares,
                stock_id=stock_id,
                price=final_price
            )
            final_events.append(a_event)

            # Store in active orders
            active_orders[order_ref] = {
                "stock_id": stock_id,
                "buy_sell": side,
                "remaining_shares": shares,
                "deleted": False
            }

            # 2) After adding a new order, possibly inject random X/E/D events
            num_injected = random.randint(0, max_injected_events_after_each_add)
            for _injected_idx in range(num_injected):
                # Filter active orders that can still be canceled
                candidates = [
                    (oref, info)
                    for oref, info in active_orders.items()
                    if (not info["deleted"]) and (info["remaining_shares"] > 0)
                ]
                if not candidates:
                    break  # no valid orders to cancel/delete

                # Pick one at random
                chosen_order_ref, chosen_info = random.choice(candidates)

                # Decide if it's partial-cancel (X/E) or full delete (D)
                if random.random() < 0.8:
                    # partial-cancel 80% of the time
                    cancel_type = random.choice(["X", "E"])
                    max_cancelable = chosen_info["remaining_shares"]
                    cancel_qty = random.randint(1, max_cancelable)

                    cevent = create_event(
                        header_type=cancel_type,
                        order_ref_num=chosen_order_ref,
                        side=chosen_info["buy_sell"],
                        num_shares=cancel_qty,
                        stock_id=chosen_info["stock_id"],
                        price=0
                    )
                    final_events.append(cevent)

                    chosen_info["remaining_shares"] -= cancel_qty
                    # If that zeroes out the order, it's effectively done,
                    # but not "deleted" (X/E is partial cancel).
                else:
                    # full delete 20% of the time
                    devent = create_event(
                        header_type="D",
                        order_ref_num=chosen_order_ref,
                        side=chosen_info["buy_sell"],
                        num_shares=0,
                        stock_id=chosen_info["stock_id"],
                        price=0
                    )
                    final_events.append(devent)

                    chosen_info["remaining_shares"] = 0
                    chosen_info["deleted"] = True

    # ---------------------------------------------------------
    # Write the final CSV in the *exact* order we built it
    # ---------------------------------------------------------
    with open(output_csv_path, mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Header", "order_ref_num", "buy_sell", "num_shares", "stock_id", "price"])
        writer.writerows(final_events)

    print(f"Generated file: {output_csv_path}")
    print("Adds have ascending order_ref_num, prices are linearly interpolated + noise, X/E/D are interspersed.")
# Example usage
if __name__ == "__main__":
    config_csv = "data/config.csv"
    output_csv = "data/test.csv"

    generate_orders_injected_cancels(
        config_csv_path=config_csv,
        output_csv_path=output_csv,
        num_orders_per_config=40,
        max_injected_events_after_each_add=2,
        random_seed=42
    )

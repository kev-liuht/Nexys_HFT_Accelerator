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
    1) We read each row in `config_csv_path` to get stock parameters. Instead of
       generating all orders for row0 then row1, we do a "parallel" or round-robin approach:
         for i in [0..num_orders_per_config-1]:
             for each config row:
                 create Add #i for that row
                 optionally inject random cancels
       so that the final event sequence is interleaved across stocks.

    2) The price for each 'A' is determined by linear interpolation between
       start_price_a -> start_price_b plus Gaussian noise of noise_pct%.

    3) Cancels (X/E) or Deletes (D) can be interspersed after each Add, referencing
       any still-active order. When we do a full Delete (D), we bias the selection
       toward older orders by using a weighted random choice proportional to their "age".
    """

    if random_seed is not None:
        random.seed(random_seed)

    # -------------------------
    # Load config data into a DataFrame
    # -------------------------
    config_df = pd.read_csv(config_csv_path)
    # Convert it into a list of dicts for convenience
    configs = []
    for _, row in config_df.iterrows():
        configs.append({
            "stock_id":      row["stock_id"],
            "buy_sell":      row["buy_sell"],      # 'B' or 'S'
            "min_price":     int(row["min_price"]),
            "max_price":     int(row["max_price"]),
            "tick":          int(row["tick"]),
            "start_price_a": float(row["start_price_a"]),
            "start_price_b": float(row["start_price_b"]),
            "noise_pct":     float(row["noise_pct"])
        })

    # -------------------------
    # Prepare to build the final event list
    # -------------------------
    final_events = []
    next_order_ref = 1
    current_event_index = 0  # increments for each event we add

    # Track active orders: {order_ref: { ... }}
    # We'll store creation_event_index to track how long it has been in the market
    active_orders = {}

    def create_event(header_type, order_ref_num, side, num_shares, stock_id, price):
        """Helper to build a row in the required format."""
        return [
            header_type,     # e.g. 'A', 'X', 'E', or 'D'
            order_ref_num,
            side,
            num_shares,
            stock_id,
            price
        ]

    # A helper to perform a weighted random choice (for older-order bias).
    # weights[i] corresponds to the candidate at index i in `candidates`.
    def weighted_random_choice(candidates, weights):
        # If all weights are zero, fallback to uniform random choice
        total_weight = sum(weights)
        if total_weight <= 0:
            return random.choice(candidates)

        r = random.uniform(0, total_weight)
        upto = 0
        for c, w in zip(candidates, weights):
            upto += w
            if r <= upto:
                return c

        # Fallback (shouldn't normally reach here if weights > 0)
        return candidates[-1]

    # -------------------------
    # Round-robin loop:
    #   Outer loop = num_orders_per_config
    #   Inner loop = each config row
    # -------------------------
    for i in range(num_orders_per_config):
        # fraction for price interpolation = i / (num_orders_per_config - 1)
        # We guard against dividing by zero if num_orders_per_config=1
        fraction = 0.0
        if num_orders_per_config > 1:
            fraction = i / (num_orders_per_config - 1)

        for cfg in configs:
            stock_id      = cfg["stock_id"]
            side          = cfg["buy_sell"]   # 'B' or 'S'
            min_price     = cfg["min_price"]
            max_price     = cfg["max_price"]
            tick          = cfg["tick"]
            start_price_a = cfg["start_price_a"]
            start_price_b = cfg["start_price_b"]
            noise_pct     = cfg["noise_pct"]

            # -----------------------
            # Calculate the base interpolated price
            # -----------------------
            base_price = start_price_a + (start_price_b - start_price_a) * fraction

            # Gaussian noise: stdev is noise_pct% of base_price
            stdev = base_price * (noise_pct / 100.0)
            noise = random.gauss(0, stdev) if stdev > 0 else 0.0

            raw_price = base_price + noise
            # Round to integer
            final_price = int(round(raw_price))
            # Clamp to [min_price, max_price]
            final_price = max(min_price, min(max_price, final_price))
            # Snap to nearest tick
            if tick > 0:
                final_price = (final_price // tick) * tick

            # -----------------------
            # Create the 'A' (Add) event
            # -----------------------
            order_ref = next_order_ref
            next_order_ref += 1

            shares = random.randint(10, 100)

            a_event = create_event(
                header_type="A",
                order_ref_num=order_ref,
                side=side,
                num_shares=shares,
                stock_id=stock_id,
                price=final_price
            )
            final_events.append(a_event)
            current_event_index += 1

            # Mark this order as active
            active_orders[order_ref] = {
                "stock_id": stock_id,
                "buy_sell": side,
                "remaining_shares": shares,
                "deleted": False,
                "creation_event_index": current_event_index
            }

            # (Optional logic) skip injection ~50% of the time
            if random.random() < 0.5:
                continue

            # -----------------------
            # Inject random cancels (X/E or D) after the Add
            # -----------------------
            num_injected = random.randint(0, max_injected_events_after_each_add)
            for _injected_idx in range(num_injected):
                # Filter orders that can be canceled (not deleted and shares>0)
                candidates = [
                    (oref, info)
                    for oref, info in active_orders.items()
                    if (not info["deleted"]) and (info["remaining_shares"] > 0)
                ]
                if not candidates:
                    break

                # Choose partial cancel vs full delete
                if random.random() < 0.2:
                    # partial cancel (X/E) ~90% of the time, pick purely at random
                    chosen_order_ref, chosen_info = random.choice(candidates)

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
                    current_event_index += 1

                    chosen_info["remaining_shares"] -= cancel_qty
                else:
                    # full delete ~10% of the time
                    # Weighted choice by age: older orders more likely to be chosen
                    ages = []
                    for (oref, info) in candidates:
                        # Add +1 to avoid zero weighting
                        age = (current_event_index - info["creation_event_index"]) + 1
                        ages.append(age)

                    chosen_order_ref, chosen_info = weighted_random_choice(candidates, ages)

                    devent = create_event(
                        header_type="D",
                        order_ref_num=chosen_order_ref,
                        side=chosen_info["buy_sell"],
                        num_shares=0,
                        stock_id=chosen_info["stock_id"],
                        price=0
                    )
                    final_events.append(devent)
                    current_event_index += 1

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
    print("Add events are interleaved in a round-robin across config rows, with random X/E/D in between.")
    print("Full deletes (D) are now biased to remove older orders first.")

# Example usage
if __name__ == "__main__":
    config_csv = "data/config.csv"
    output_csv = "data/test.csv"

    generate_orders_injected_cancels(
        config_csv_path=config_csv,
        output_csv_path=output_csv,
        num_orders_per_config=100,
        max_injected_events_after_each_add=3,
        # random_seed=42
        random_seed=45
    )

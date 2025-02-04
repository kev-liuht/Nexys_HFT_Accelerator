class OrderRecord:
    """
    Simple structure to hold order info in a 'BRAM-like' array.
    """
    __slots__ = ('order_id', 'side', 'price', 'quantity', 'active')

    def __init__(self, order_id='', side='', price=0.0, quantity=0, active=False):
        self.order_id = order_id
        self.side = side
        self.price = price
        self.quantity = quantity
        self.active = active


class BRAMOrderBook:
    """
    Each stock will have a fixed-size list (acting like BRAM).
    We'll store up to MAX_ORDERS records in it.

    For simplicity:
    - `stock_data[stock_id]` = {
        'orders': [OrderRecord, OrderRecord, ... up to MAX_ORDERS],
        'next_index': int
      }
    """

    def __init__(self, max_orders=10000):
        self.MAX_ORDERS = max_orders
        self.stock_data = {}  # stock_id -> { 'orders': [...], 'next_index': int }

    def initialize_stock(self, stock_id):
        """
        Initialize a new array for a given stock if it doesn't already exist.
        """
        if stock_id not in self.stock_data:
            # Create a list of inactive OrderRecord placeholders
            orders_array = [OrderRecord() for _ in range(self.MAX_ORDERS)]
            self.stock_data[stock_id] = {
                'orders': orders_array,
                'next_index': 0
            }

    def add_order(self, stock_id, order_id, side, price, quantity):
        """
        Add an order for a given stock sequentially in the array.
        """
        # Make sure we have a storage array for this stock.
        self.initialize_stock(stock_id)

        data = self.stock_data[stock_id]
        idx = data['next_index']

        # Ensure we don't exceed maximum capacity
        if idx >= self.MAX_ORDERS:
            print(f"Cannot add order {order_id}: BRAM full for stock {stock_id}.")
            return

        # Place the order info into the array at `idx`
        data['orders'][idx].order_id = order_id
        data['orders'][idx].side = side
        data['orders'][idx].price = price
        data['orders'][idx].quantity = quantity
        data['orders'][idx].active = True

        # Move to the next position
        data['next_index'] += 1

        print(f"Added order {order_id} (side={side}, price={price}, qty={quantity}) at index {idx}")

    def cancel_order(self, stock_id, order_id, quantity=None):
        """
        Cancel (or partially reduce) an order by searching the array linearly.

        - If `quantity` is None, we fully remove the order.
        - Else, we reduce the existing order's quantity by `quantity`.
        """
        if stock_id not in self.stock_data:
            print(f"Stock {stock_id} not found.")
            return

        data = self.stock_data[stock_id]
        orders_array = data['orders']

        # Linear scan to find matching order_id
        for i, record in enumerate(orders_array):
            if record.active and record.order_id == order_id:
                # Found the active order
                if quantity is None:
                    # Fully cancel
                    record.active = False
                    record.quantity = 0
                    print(f"Order {order_id} fully canceled at index {i}.")
                else:
                    # Reduce quantity
                    if record.quantity <= quantity:
                        # If reduction is >= current qty, fully cancel
                        record.quantity = 0
                        record.active = False
                        print(f"Order {order_id} fully canceled (partial cancel used entire quantity) at index {i}.")
                    else:
                        record.quantity -= quantity
                        print(f"Order {order_id} reduced by {quantity}, remaining qty={record.quantity} at index {i}.")
                return

        # If we reach here, the order wasn't found
        print(f"Order {order_id} not found or already inactive for stock {stock_id}.")

    def publish(self, stock_id):
        """
        Print out all active orders for a stock, listing side, price, and quantity.
        This is effectively scanning the 'BRAM' for all active entries.
        """
        if stock_id not in self.stock_data:
            print(f"Stock {stock_id} not found.")
            return

        data = self.stock_data[stock_id]
        orders_array = data['orders']

        print(f"--- Publishing Order Book for stock {stock_id} ---")
        has_any = False
        for i, record in enumerate(orders_array):
            if record.active:
                has_any = True
                print(f"Index={i}: OrderID={record.order_id}, Side={record.side}, "
                      f"Price={record.price}, Qty={record.quantity}")
        if not has_any:
            print("No active orders.")
        print("------------------------------------------------")


# Example usage
if __name__ == "__main__":
    bram_ob = BRAMOrderBook(max_orders=10)

    # Add some orders
    bram_ob.add_order("AAPL", "o1", "bid", 135.50, 10)
    bram_ob.add_order("AAPL", "o2", "ask", 136.00, 5)
    bram_ob.add_order("AAPL", "o3", "ask", 137.50, 20)

    # Publish current orders
    bram_ob.publish("AAPL")

    # Cancel part of an order
    bram_ob.cancel_order("AAPL", "o3", quantity=5)

    # Publish again
    bram_ob.publish("AAPL")

    # Cancel the entire order
    bram_ob.cancel_order("AAPL", "o3")

    # Final state
    bram_ob.publish("AAPL")

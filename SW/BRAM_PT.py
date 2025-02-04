import math


class TreeOrderBook:
    def __init__(self, min_price, max_price, ticker):
        """
        :param min_price: float, the minimum possible price (X)
        :param max_price: float, the maximum possible price (Y)
        :param ticker: float, the minimum price increment
        """
        self.min_price = min_price
        self.max_price = max_price
        self.ticker = ticker

        # Number of distinct price levels
        n_float = (max_price - min_price) / ticker
        # e.g. if X=100, Y=100.3, ticker=0.1 => n_float=3.0 => n=4
        self.num_levels = int(round(n_float)) + 1

        # Bank of price registers:
        # price_quantity[i] = aggregated qty for price index i
        self.price_quantity = [0] * self.num_levels

        # Build a segment tree to track the best (max) price index with qty>0
        # For simplicity, store it in a 1-based array where root = index 1
        # The size of the segment tree array must be 2 * 2^ceil(log2(num_levels)).
        size_tree = 1
        while size_tree < self.num_levels:
            size_tree <<= 1
        self.tree_size = size_tree << 1  # 2 * size_tree
        # Each node in `tree` holds an index [0..num_levels-1] or -1 if none
        self.tree = [-1] * self.tree_size

        # An "order map" for quick lookup by order_id => {price_index, quantity}
        self.order_map = {}

        # Build an initial tree (all zero quantities => all -1 in leaves)
        self._build_tree()

    # --------------------------------------------------------------------------
    # Price <--> Index Mapping
    # --------------------------------------------------------------------------

    def _price_to_index(self, price):
        """
        Convert price -> integer index using min_price and ticker.
        e.g. if min_price=100, ticker=0.1, then price=100 => index=0
             price=100.1 => index=1, etc.
        """
        return int(round((price - self.min_price) / self.ticker))

    def _index_to_price(self, i):
        """
        Convert index -> float price
        """
        return self.min_price + i * self.ticker

    # --------------------------------------------------------------------------
    # Segment Tree Construction & Internal Methods
    # --------------------------------------------------------------------------

    def _build_tree(self):
        """ Initialize the segment tree leaves to -1 (since all qty=0). """
        # The leaves in this 1-based indexing scheme start around self.tree_size//2
        # However, we can just do a simple pass because all are -1 initially.
        # No real "building" needed if everything is -1. 
        pass

    def _leaf_start(self):
        """ Return the index in self.tree where leaves begin (1-based). """
        return self.tree_size // 2  # e.g. if tree_size=16, leaves start at index 8

    def _update_leaf_and_parents(self, price_idx):
        """
        Update the leaf corresponding to 'price_idx' in the segment tree,
        then move upwards updating parents.
        """
        # 1. Determine the leaf node in the tree
        leaf_start = self._leaf_start()
        node = leaf_start + price_idx  # this is the leaf node index in segment tree

        # 2. If quantity>0, store 'price_idx' there; else -1
        if self.price_quantity[price_idx] > 0:
            self.tree[node] = price_idx
        else:
            self.tree[node] = -1

        # 3. Bubble up: at each parent, set tree[parent] = the better child
        node //= 2  # move to parent
        while node >= 1:
            left_child = 2 * node
            right_child = left_child + 1

            left_idx = self.tree[left_child]
            right_idx = self.tree[right_child] if right_child < self.tree_size else -1

            # Combine logic for max-heap of indices that have qty>0
            # We pick whichever price index is higher, ignoring -1
            best_idx = -1
            if left_idx == -1 and right_idx == -1:
                best_idx = -1
            elif left_idx == -1:
                best_idx = right_idx
            elif right_idx == -1:
                best_idx = left_idx
            else:
                # choose max index
                if left_idx > right_idx:
                    best_idx = left_idx
                else:
                    best_idx = right_idx

            self.tree[node] = best_idx
            node //= 2

    # --------------------------------------------------------------------------
    # Public Methods for Orders
    # --------------------------------------------------------------------------

    def add_order(self, order_id, price, quantity):
        """
        Add a new order. 
        - Map price -> index
        - Increase aggregated quantity at that index
        - Update segment tree
        - Store order in order_map
        """
        idx = self._price_to_index(price)

        # Increase aggregated quantity
        self.price_quantity[idx] += quantity

        # Update segment tree
        self._update_leaf_and_parents(idx)

        # Store in order_map
        self.order_map[order_id] = {
            "price_index": idx,
            "quantity": quantity
        }

    def cancel_order(self, order_id, cancel_qty=None):
        """
        Cancel (or reduce) an existing order by `order_id`.
        If `cancel_qty` is None, remove the entire quantity.
        Otherwise, reduce by `cancel_qty`.
        """
        if order_id not in self.order_map:
            print(f"Order {order_id} not found.")
            return

        info = self.order_map[order_id]
        idx = info["price_index"]
        old_qty = info["quantity"]

        if cancel_qty is None or cancel_qty >= old_qty:
            # remove fully
            self.price_quantity[idx] -= old_qty
            # If that drives quantity negative (shouldn't normally happen),
            # clamp it to 0
            if self.price_quantity[idx] < 0:
                self.price_quantity[idx] = 0
            # remove from order_map
            del self.order_map[order_id]
            print(f"Order {order_id} fully canceled.")
        else:
            # partial cancel
            self.price_quantity[idx] -= cancel_qty
            if self.price_quantity[idx] < 0:
                self.price_quantity[idx] = 0
            self.order_map[order_id]["quantity"] = old_qty - cancel_qty
            print(f"Order {order_id} reduced by {cancel_qty}, remaining {old_qty - cancel_qty}")

        # update tree
        self._update_leaf_and_parents(idx)

    def get_best_price(self):
        """
        Return the current best (highest) price that has quantity > 0,
        or None if no active orders.
        """
        best_idx = self.tree[1]  # root of the tree, 1-based indexing
        if best_idx == -1:
            return None
        else:
            return self._index_to_price(best_idx)

    def get_best_quantity(self):
        """
        Return the quantity at the best price,
        or 0 if no active orders.
        """
        best_idx = self.tree[1]
        if best_idx == -1:
            return 0
        else:
            return self.price_quantity[best_idx]

    def print_order_map(self):
        """ Debug: print all orders in the dictionary. """
        print("Order Map:")
        for oid, data in self.order_map.items():
            p = self._index_to_price(data["price_index"])
            print(f"  ID={oid}, Price={p}, Qty={data['quantity']}")
        print()

    def print_price_levels(self):
        """ Debug: print the aggregated quantity at each price level. """
        print("Price Levels (index -> price -> qty):")
        for i, qty in enumerate(self.price_quantity):
            if qty > 0:
                price = self._index_to_price(i)
                print(f"  i={i}, price={price}, qty={qty}")
        print()

    def print_tree(self):
        """ Debug: print the segment tree array. """
        print("Segment Tree (1-based array):")
        for i in range(1, self.tree_size):
            if i < len(self.tree):
                print(f"  node={i}, best_idx={self.tree[i]}")
        print()


# -------------------------------------------------------------------------------
# Example Usage
# -------------------------------------------------------------------------------
if __name__ == "__main__":
    # Suppose price range is [100.0, 101.0] with ticker=0.1 => 11 price levels
    ob = TreeOrderBook(min_price=100.0, max_price=101.0, ticker=0.1)

    # Add some orders
    ob.add_order("A", 100.0, 5)  # price index=0
    ob.add_order("B", 100.7, 10)  # price index=7
    ob.add_order("C", 101.0, 1)  # price index=10

    ob.print_price_levels()
    ob.print_order_map()
    ob.print_tree()

    print("Best Price:", ob.get_best_price(), "Qty at best:", ob.get_best_quantity())
    print("----------")

    # Cancel part of order B
    ob.cancel_order("B", cancel_qty=3)
    print("After partial cancel of B:")
    print("Best Price:", ob.get_best_price(), "Qty at best:", ob.get_best_quantity())
    ob.print_price_levels()
    print("----------")

    # Cancel entire remaining quantity of B
    ob.cancel_order("B")
    print("After fully canceling B:")
    print("Best Price:", ob.get_best_price(), "Qty at best:", ob.get_best_quantity())
    ob.print_price_levels()
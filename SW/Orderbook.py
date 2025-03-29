#!/usr/bin/env python3
import math

# -------------------------------------------------------------------------
# Constants (adjust these as desired)
# -------------------------------------------------------------------------

NUM_STOCKS = 4
MIN_PRICE_INIT = [1000000, 1000000, 1000000, 1000000]  # Example initialization
TICK_INIT = [2500, 2500, 2500, 2500]  # Example initialization
MAX_ORDER_NUM = 1024
MAX_LEVELS = 256

SIDE_BID = 0
SIDE_ASK = 1


# For 136-bit 'message' fields (in the original code):
#   Bits 0..31    -> stock_id
#   Bits 32..63   -> order_ref_num
#   Bits 64..95   -> num_shares
#   Bits 96..127  -> price
#   Bits 128..131 -> order_type (4 bits)
#   Bit  132      -> buy_sell (1 bit: 0=bid, 1=ask)
#   Bits 133..135 -> unused

# Mapping to the hardware code's "order_type" usage:
#   0x1 -> Add
#   0x2 -> Cancel partial
#   0x4 -> Execute partial
#   0x8 -> Delete entire

# -------------------------------------------------------------------------
# Utility Functions
# -------------------------------------------------------------------------
def choose_preferred(idxA, idxB, side):
    """
    For BIDs, prefer the higher price index.
    For ASKs, prefer the lower price index.
    0xFFFFFFFF (4294967295) indicates no valid price at that node.
    """
    NEG_ONE = 0xFFFFFFFF
    if idxA == NEG_ONE:
        return idxB
    if idxB == NEG_ONE:
        return idxA

    if side == SIDE_BID:
        return idxA if idxA > idxB else idxB
    else:  # side == SIDE_ASK
        return idxA if idxA < idxB else idxB


# -------------------------------------------------------------------------
# Classes
# -------------------------------------------------------------------------
class TreeOrderBook:
    """
    Segment Tree–based structure for a single side (BID or ASK) of a single stock.
    - segment_tree: holds the "best index" (either min or max index, depending on side).
    - price_quantity: quantity at each price index.
    """

    def __init__(self, min_price=1000000, tick_size=10000, side=SIDE_BID):
        self.min_price = min_price
        self.tick_size = tick_size
        self.side = side
        self.num_levels = MAX_LEVELS

        self.max_price = self.min_price + self.tick_size * (self.num_levels - 1)

        # For a segment tree of size num_levels, we store it in an array of size 2*num_levels
        self.segment_tree = [0xFFFFFFFF] * (2 * self.num_levels)
        self.price_quantity = [0] * self.num_levels

    def price_to_index(self, price):
        diff = price - self.min_price
        idx = diff // self.tick_size
        if idx >= self.num_levels:
            # If out of range, clamp to last level
            idx = self.num_levels - 1
        return idx

    def index_to_price(self, idx):
        return self.min_price + idx * self.tick_size

    def bubble_up(self, leaf_idx):
        """
        Update segment tree nodes from a leaf up to the root.
        """
        # Convert leaf_idx (0..num_levels-1) to actual tree index (num_levels..2*num_levels-1)
        node = leaf_idx + self.num_levels
        # If we have a positive quantity at leaf_idx, set that index; otherwise, set to invalid.
        if self.price_quantity[leaf_idx] > 0:
            self.segment_tree[node] = leaf_idx
        else:
            self.segment_tree[node] = 0xFFFFFFFF

        # Move upward in the tree
        node >>= 1  # node //= 2
        while node > 0:
            left = self.segment_tree[node << 1]
            right = self.segment_tree[(node << 1) + 1]
            self.segment_tree[node] = choose_preferred(left, right, self.side)
            node >>= 1

    def add_quantity(self, price, quantity):
        """
        Add quantity at a given price, update the tree.
        """
        idx = self.price_to_index(price)
        self.price_quantity[idx] += quantity
        self.bubble_up(idx)

    def remove_quantity(self, idx, quantity):
        """
        Remove quantity from a given price index, update the tree.
        """
        if quantity > self.price_quantity[idx]:
            quantity = self.price_quantity[idx]  # can't remove more than we have
        self.price_quantity[idx] -= quantity
        self.bubble_up(idx)
        return quantity

    def get_top_5(self):
        """
        Return the top-5 price/quantity pairs for this side, according to 'best' definition.
        For BIDs, best is highest; for ASKs, best is lowest.
        We do a temporary removal from the segment tree to find the top 5, then restore.
        """
        saved_indices = []
        saved_qty = []
        # Extract top-5
        for _ in range(5):
            best_idx = self.segment_tree[1]
            if best_idx == 0xFFFFFFFF:
                # No more valid prices
                break
            current_qty = self.price_quantity[best_idx]
            saved_indices.append(best_idx)
            saved_qty.append(current_qty)

            # Temporarily remove so we can find the next best
            self.price_quantity[best_idx] = 0
            self.bubble_up(best_idx)

        # Restore them
        for idx, qty in zip(saved_indices, saved_qty):
            self.price_quantity[idx] = qty
            self.bubble_up(idx)

        # Format output
        result_prices = []
        result_qtys = []
        for i in range(5):
            if i < len(saved_indices):
                result_prices.append(self.index_to_price(saved_indices[i]))
                result_qtys.append(saved_qty[i])
            else:
                # If fewer than 5 found, fill with "worst" price + default quantity (1)
                if self.side == SIDE_BID:
                    # For BIDs, the "worst" is the min price index
                    worst_price = self.index_to_price(0)
                else:
                    # For ASKs, the "worst" is the max price index
                    worst_price = self.index_to_price(self.num_levels - 1)
                result_prices.append(worst_price)
                result_qtys.append(1)
        return result_prices, result_qtys


class OrderList:
    """
    Maintains the details of each active order:
      - order_valid[i]: boolean if order i is active
      - order_price_index[i]: stored price index in the tree
      - order_quantity[i]
      - order_ask_bid[i]: which side? (0=bid, 1=ask, 2=invalid, 3=deleted)
    """

    def __init__(self):
        self.MAX_ORDERS = MAX_ORDER_NUM
        self.order_valid = [False] * self.MAX_ORDERS
        self.order_price_index = [0] * self.MAX_ORDERS
        self.order_quantity = [0] * self.MAX_ORDERS
        # 0=bid, 1=ask, 2=never assigned, 3=deleted
        self.order_ask_bid = [2] * self.MAX_ORDERS

    def is_valid(self, order_id):
        if order_id >= self.MAX_ORDERS:
            return False
        return self.order_valid[order_id]

    def side_of(self, order_id):
        """
        Return the side (0 or 1) of this order, or 2 if invalid, 3 if previously deleted.
        """
        if order_id >= self.MAX_ORDERS:
            return 2
        if not self.order_valid[order_id]:
            return 2
        return self.order_ask_bid[order_id]

    def delete_order_completely(self, order_id):
        self.order_valid[order_id] = False
        self.order_quantity[order_id] = 0
        self.order_ask_bid[order_id] = 3


class StockOrderBook:
    """
    For each of the NUM_STOCKS, we have one bid TreeOrderBook and one ask TreeOrderBook,
    plus a single global OrderList that references all existing orders.
    """

    def __init__(self):
        self.bid_books = []
        self.ask_books = []
        self.order_list = OrderList()

        # Initialize each stock's bid/ask
        for i in range(NUM_STOCKS):
            min_p = MIN_PRICE_INIT[i]
            tick = TICK_INIT[i]
            bid_ob = TreeOrderBook(min_price=min_p, tick_size=tick, side=SIDE_BID)
            ask_ob = TreeOrderBook(min_price=min_p, tick_size=tick, side=SIDE_ASK)
            self.bid_books.append(bid_ob)
            self.ask_books.append(ask_ob)

    def add_order(self, stock_id, order_id, price, quantity, side):
        """
        Add an order to the correct side’s TreeOrderBook.
        side=0 (bid) or side=1 (ask).
        """
        if stock_id >= NUM_STOCKS:
            return

        if order_id >= self.order_list.MAX_ORDERS:
            return

        # Insert quantity
        if side == SIDE_BID:
            idx = self.bid_books[stock_id].price_to_index(price)
            self.bid_books[stock_id].price_quantity[idx] += quantity
            self.bid_books[stock_id].bubble_up(idx)
        else:
            idx = self.ask_books[stock_id].price_to_index(price)
            self.ask_books[stock_id].price_quantity[idx] += quantity
            self.ask_books[stock_id].bubble_up(idx)

        # Set info in the order list
        self.order_list.order_valid[order_id] = True
        self.order_list.order_price_index[order_id] = idx
        self.order_list.order_quantity[order_id] = quantity
        self.order_list.order_ask_bid[order_id] = side

    def cancel_order(self, stock_id, order_id, cancel_qty):
        """
        Cancel (reduce) some shares from an existing order. If cancel_qty is 0xFFFFFFFF,
        it means remove all.
        """
        if stock_id >= NUM_STOCKS:
            return

        if not self.order_list.is_valid(order_id):
            return

        side = self.order_list.order_ask_bid[order_id]
        idx = self.order_list.order_price_index[order_id]
        old_qty = self.order_list.order_quantity[order_id]

        if cancel_qty == 0xFFFFFFFF:
            remove_qty = old_qty
        else:
            remove_qty = min(cancel_qty, old_qty)

        # Remove from tree
        if side == SIDE_BID:
            actual_removed = self.bid_books[stock_id].remove_quantity(idx, remove_qty)
        else:
            actual_removed = self.ask_books[stock_id].remove_quantity(idx, remove_qty)

        new_qty = old_qty - actual_removed
        self.order_list.order_quantity[order_id] = new_qty
        if new_qty == 0:
            # Mark as deleted
            self.order_list.delete_order_completely(order_id)

    def get_top_5(self, stock_id, side):
        """
        Retrieve the top-5 price/quantity pairs from the specified side of stock_id.
        Returns a tuple of (price_list, qty_list).
        """
        if side == SIDE_BID:
            return self.bid_books[stock_id].get_top_5()
        else:
            return self.ask_books[stock_id].get_top_5()

    def publish_snapshot(self):
        """
        Return a snapshot of top-5 bids and asks for all stocks.
        The original HLS code wrote them out as a stream, but here we simply return them.
        Format:
            [
              {
                "stock": s,
                "ask_prices": [...5...],
                "ask_qty": [...5...],
                "bid_prices": [...5...],
                "bid_qty": [...5...]
              }, ...
            ]
        """
        result = []
        for s in range(NUM_STOCKS):
            ask_p, ask_q = self.ask_books[s].get_top_5()
            bid_p, bid_q = self.bid_books[s].get_top_5()
            result.append({
                "stock": s,
                "ask_prices": ask_p,
                "ask_qty": ask_q,
                "bid_prices": bid_p,
                "bid_qty": bid_q
            })
        return result


# -------------------------------------------------------------------------
# The main manager that processes 136-bit messages
# -------------------------------------------------------------------------
class OrderBookManager:
    def __init__(self):
        self.stock_order_book = StockOrderBook()
        self.initialized = True
        self.num_new_order = 0
        self.publish_threshold = 20  # Publish snapshot after 20 new "add" calls

    def process_message_136bit(self, in_data):
        """
        Process one 136-bit message, emulating the HLS logic.
        in_data: integer representing 136 bits.
        The breakdown (little-endian style):
          Bits 0..31    : stock_id
          Bits 32..63   : order_ref_num
          Bits 64..95   : num_shares
          Bits 96..127  : price
          Bits 128..131 : order_type
          Bit  132      : buy_sell
          Bits 133..135 : unused
        """
        stock_id = (in_data & 0xFFFFFFFF)
        order_ref_num = ((in_data >> 32) & 0xFFFFFFFF)
        num_shares = ((in_data >> 64) & 0xFFFFFFFF)
        price = ((in_data >> 96) & 0xFFFFFFFF)
        order_type = ((in_data >> 128) & 0xF)
        buy_sell = ((in_data >> 132) & 0x1)  # 0=bid, 1=ask

        # 0x1 -> Add order
        # 0x2 -> Cancel partial
        # 0x4 -> Execute partial (treated similarly to partial-cancel)
        # 0x8 -> Delete entire (treated similarly to full-cancel)
        if order_type == 0x1:
            # ADD
            self.stock_order_book.add_order(stock_id, order_ref_num, price, num_shares, buy_sell)
            self.num_new_order += 1

        elif order_type == 0x2:
            # PARTIAL CANCEL
            side_flag = self.stock_order_book.order_list.side_of(order_ref_num)
            if side_flag < 2:  # valid side
                self.stock_order_book.cancel_order(stock_id, order_ref_num, num_shares)

        elif order_type == 0x4:
            # EXECUTE (partial) => same logic as partial cancel
            side_flag = self.stock_order_book.order_list.side_of(order_ref_num)
            if side_flag < 2:
                self.stock_order_book.cancel_order(stock_id, order_ref_num, num_shares)

        elif order_type == 0x8:
            # DELETE (full cancel)
            side_flag = self.stock_order_book.order_list.side_of(order_ref_num)
            if side_flag < 2:
                self.stock_order_book.cancel_order(stock_id, order_ref_num, 0xFFFFFFFF)

        # Optionally publish after threshold
        if self.num_new_order >= self.publish_threshold:
            self.num_new_order = 0
            snap = self.stock_order_book.publish_snapshot()
            # Here, you could do whatever you want with the snapshot;
            # for now, we can just print or return it. We'll just print.
            print("=== Publishing Snapshot ===")
            for entry in snap:
                print(entry)
            print("===========================")

    # Provide direct ports for manual usage as well,
    # if you prefer not to pack them into 136-bit messages:

    def add_order(self, stock_id, order_id, price, quantity, side):
        """
        Manually add an order (0=bid, 1=ask).
        """
        self.stock_order_book.add_order(stock_id, order_id, price, quantity, side)
        self.num_new_order += 1
        # Possibly auto-publish
        if self.num_new_order >= self.publish_threshold:
            self.num_new_order = 0
            return self.stock_order_book.publish_snapshot()
        return None

    def cancel_order(self, stock_id, order_id, cancel_qty):
        """
        Manually cancel some or all of an order's shares.
        cancel_qty=0xFFFFFFFF => remove all
        """
        self.stock_order_book.cancel_order(stock_id, order_id, cancel_qty)

    def execute_order(self, stock_id, order_id, execute_qty):
        """
        Partial execution is logically the same as partial cancel from the book's perspective.
        """
        self.stock_order_book.cancel_order(stock_id, order_id, execute_qty)

    def delete_order(self, stock_id, order_id):
        """
        Full deletion is logically the same as full cancel.
        """
        self.stock_order_book.cancel_order(stock_id, order_id, 0xFFFFFFFF)

    def get_top_5(self, stock_id, side):
        """
        Retrieve top-5 price/qty for a given side on a given stock.
        """
        return self.stock_order_book.get_top_5(stock_id, side)

    def publish_snapshot(self):
        """
        Return full snapshot (top-5 bids and asks for each stock).
        """
        return self.stock_order_book.publish_snapshot()


# -------------------------------------------------------------------------
# Example usage / test
# -------------------------------------------------------------------------
if __name__ == "__main__":
    manager = OrderBookManager()

    # Example: create some add orders via the direct port
    # Add an order (stock_id=0, order_id=10, price=1010000, qty=50, side=0(bid))
    manager.add_order(stock_id=0, order_id=10, price=1010000, quantity=50, side=SIDE_BID)
    manager.add_order(stock_id=0, order_id=11, price=999000, quantity=20, side=SIDE_ASK)

    # Example: partial cancel (reducing an order by 10 shares)
    manager.cancel_order(stock_id=0, order_id=10, cancel_qty=10)

    # Example: execute part of an order
    manager.execute_order(stock_id=0, order_id=11, execute_qty=5)

    # Example: full delete of an order
    manager.delete_order(stock_id=0, order_id=10)

    # Manually retrieve top-5 for stock 0, side=bid
    top5_prices, top5_qtys = manager.get_top_5(stock_id=0, side=SIDE_BID)
    print("Top-5 BIDs for stock 0:")
    for p, q in zip(top5_prices, top5_qtys):
        print(f"  Price={p}, Qty={q}")

    # Publish the entire snapshot for all stocks
    snapshot = manager.publish_snapshot()
    print("Full snapshot:")
    for entry in snapshot:
        print(entry)

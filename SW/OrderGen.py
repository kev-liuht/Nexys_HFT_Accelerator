#!/usr/bin/env python3
import math

class OrderGenerator:
    """
    Order generation tracks the internal state of the account and produces OUCH orders 
    for each of the four stocks, as well as periodic updates to the internal portfolio 
    worth over time.

    Given a weight vector, current stock prices, and average prices, it:
      - Computes the portfolio value (cash + current holdings).
      - Determines target shares for each stock based on desired allocations.
      - Generates OUCH orders (48-byte messages) for buy/sell actions.
      - Outputs a binary blob: 4 bytes of portfolio value (fixed-point) 
        followed by 4 orders (4 x 48 bytes).
    """

    def __init__(self):
        # Internal state: holdings (shares) and cash (dollars)
        self.holdings = [0.0, 0.0, 0.0, 0.0]
        self.cash = 10000.0  # initially 10,000 dollars
        self.userRefNum = 1
        self.latched_weights = [0.0, 0.0, 0.0, 0.0]
        # Static symbols for each stock (8 characters, padded)
        self.symbols = ["AMD_    ", "JPM_    ", "CUST    ", "PG__    "]
        # Dummy ClOrdID as in the HLS code (14 characters)
        self.dummyClOrdID = "CLORD_ID001XXX"

    def float_to_fixedpt(self, value):
        """
        Convert a float dollar value to fixed-point (price * 10000).
        """
        return int(value * 10000)

    def pack_order(self, userRefNum, side, quantity, symbol, price):
        """
        Pack a 48-byte OUCH message into a bytearray.
        
        Message layout (each index represents one byte):
          0: 'O'
          1-4: userRefNum (big-endian)
          5: side (as a character)
          6-9: quantity (big-endian)
          10-17: symbol (8 characters)
          18-21: zeros
          22-25: price (fixed-point, big-endian)
          26: '0'
          27: 'Y'
          28: 'P'
          29: 'Y'
          30: 'N'
          31-44: dummy ClOrdID (14 characters)
          45-47: zeros
        """
        ORDER_MSG_BYTES = 48
        msg = bytearray(ORDER_MSG_BYTES)
        
        # Initialize entire message to zero (already done by bytearray)
        
        # Set header
        msg[0] = ord('O')
        msg[1] = (userRefNum >> 24) & 0xFF
        msg[2] = (userRefNum >> 16) & 0xFF
        msg[3] = (userRefNum >> 8) & 0xFF
        msg[4] = userRefNum & 0xFF
        
        # Side
        msg[5] = ord(side)
        
        # Quantity in big-endian
        msg[6] = (quantity >> 24) & 0xFF
        msg[7] = (quantity >> 16) & 0xFF
        msg[8] = (quantity >> 8) & 0xFF
        msg[9] = quantity & 0xFF
        
        # Copy symbol (8 characters)
        for i in range(8):
            msg[10 + i] = ord(symbol[i])
        
        # Bytes 18-21 remain 0
        
        # Price: convert to fixed-point and pack into 4 bytes (big-endian)
        price_fixed = self.float_to_fixedpt(price)
        msg[22] = (price_fixed >> 24) & 0xFF
        msg[23] = (price_fixed >> 16) & 0xFF
        msg[24] = (price_fixed >> 8) & 0xFF
        msg[25] = price_fixed & 0xFF
        
        # Static fields
        msg[26] = ord('0')
        msg[27] = ord('Y')
        msg[28] = ord('P')
        msg[29] = ord('Y')
        msg[30] = ord('N')
        
        # Dummy ClOrdID (14 characters)
        for i in range(14):
            msg[31 + i] = ord(self.dummyClOrdID[i])
        
        # Set final two bytes to 0 (and byte 47 remains 0)
        msg[45] = 0
        msg[46] = 0
        # msg[47] is already 0
        return msg

    def order_gen(self, weight_vals, stock_prices, real_prices):
        """
        Reads new weight and price values, computes the new portfolio value using
        the latest prices, and then generates OUCH orders based on the new weight vector.
        Internal state (holdings, cash, userRefNum, latched_weights) is updated.
        
        Parameters:
          weight_vals: 4-element list of floats (portfolio weights, 0-1)
          stock_prices: 4-element list of floats (current stock prices)
        
        Returns:
          A bytes object of 196 bytes:
            - 4 bytes of portfolio value (fixed-point, price*10000, big-endian)
            - 4 orders x 48 bytes each (OUCH order messages)
        """
        NUM_STOCKS = 4
        
        # Latch the new weight vector; do a NaN check first.
        for i in range(NUM_STOCKS):
            self.latched_weights[i] = self.latched_weights[i] if math.isnan(weight_vals[i]) else weight_vals[i]
            
        # self.latched_weights = weight_vals[:]
        
        # Compute portfolio value: cash + sum(holdings * stock_price)
        portfolio_value = self.cash
        for i in range(NUM_STOCKS):
            portfolio_value += self.holdings[i] * stock_prices[i]
        
        # Convert portfolio value to fixed-point.
        portfolio_fixed = self.float_to_fixedpt(portfolio_value)
        print(f"\tportfolio: {portfolio_value}\tportfolio_fixedpt:{portfolio_fixed}\tportfolio_raw:{hex(portfolio_fixed)}")
        
        # Begin output: 4 bytes for portfolio value (big-endian).
        output = bytearray()
        output.extend(portfolio_fixed.to_bytes(4, byteorder='big'))
        
        # Generate orders using the latched weight vector.
        new_holdings = [0.0] * NUM_STOCKS
        total_cost = 0.0
        
        for i in range(NUM_STOCKS):
            # Desired allocation in dollars = weight * portfolio_value.
            desired_alloc = self.latched_weights[i] * portfolio_value
            # print(f"\tdesired_alloc:{desired_alloc}")

            # Compute target shares (unsigned integer).
            target_shares = int(desired_alloc / stock_prices[i])
            new_holdings[i] = float(target_shares)
            total_cost += target_shares * stock_prices[i]
            
            # Determine order delta.
            delta = target_shares - int(self.holdings[i])
            if delta > 0:
                side = 'B'  # Buy
                quantity = delta
            elif delta < 0:
                side = 'S'  # Sell
                quantity = -delta
            else:
                side = 'N'  # No action
                quantity = 0
            # Pack the order message (48 bytes) for this stock.
            order_msg = self.pack_order(self.userRefNum, side, quantity, self.symbols[i], real_prices[i])
            self.userRefNum += 1
            
            # Append the order message to output.
            output.extend(order_msg)
        
        # Update internal state.
        self.holdings = new_holdings
        self.cash = portfolio_value - total_cost

        output = self.reverse_endian_bytes(output)
        # Return final output: 4 bytes portfolio + 4 orders x 48 bytes = 196 bytes.
        return bytes(output)
    
    def reverse_endian_bytes(self, data: bytearray) -> bytearray:
        if len(data) % 4 != 0:
            raise ValueError("Input length must be divisible by 4")

        result = bytearray()

        for i in range(0, len(data), 4):
            chunk = data[i:i+4]
            reversed_chunk = chunk[::-1]  # Reverse the byte order
            result.extend(reversed_chunk)

        return result

# Example usage:
if __name__ == "__main__":
    # Dummy input values:
    # Weights (from qr_decomp_lin_solver)
    weights = [0.25, 0.25, 0.25, 0.25]
    # Current stock prices (from ta_parser)
    stock_prices = [150.0, 200.0, 250.0, 100.0]
    
    # Create an instance of the order generator.
    order_gen = OrderGenerator()
    
    # Generate the output binary blob.
    output_blob = order_gen.order_gen(weights, stock_prices)
    
    # Verify the output length.
    print("Output blob length:", len(output_blob))  # Expected: 196 bytes
    
    # For demo, print the output blob in hexadecimal.
    # The first 4 bytes are the portfolio value in fixed-point,
    # followed by 4 orders each 48 bytes.
    print("Output blob (hex):")
    print(output_blob.hex())

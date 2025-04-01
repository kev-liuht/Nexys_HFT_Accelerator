#!/usr/bin/env python3
class TaParser:
    def __init__(self, num_stocks=4):
        self.num_stocks = num_stocks

    def update2(self, snapshot):
        market_prices = [0.0] * self.num_stocks

        for entry in snapshot:
            stock_index = entry['stock']
            ask_prices = entry['ask_prices']
            ask_qty = entry['ask_qty']
            bid_prices = entry['bid_prices']
            bid_qty = entry['bid_qty']

            # Compute weighted average market price
            total_weight = sum(ask_qty) + sum(bid_qty)
            weighted_price = sum([(p / 10000.0) * (q / 10000.0) for p, q in zip(ask_prices, ask_qty)]) + \
                             sum([(p / 10000.0) * (q / 10000.0) for p, q in zip(bid_prices, bid_qty)])

            market_prices[stock_index] = weighted_price / total_weight if total_weight > 0 else 0.0

        return market_prices
    
    def update(self, snapshot):
        market_prices = [0.0] * self.num_stocks

        for entry in snapshot:
            stock_index = entry['stock']
            ask_prices = entry['ask_prices']
            ask_qty = entry['ask_qty']
            bid_prices = entry['bid_prices']
            bid_qty = entry['bid_qty']

            # Compute weighted average market price
            total_weight = sum(ask_qty) + sum(bid_qty)
            weighted_price = sum([(p / 10000.0) * q for p, q in zip(ask_prices, ask_qty)]) + \
                             sum([(p / 10000.0) * q for p, q in zip(bid_prices, bid_qty)])

            market_prices[stock_index] = weighted_price / total_weight if total_weight > 0 else 0.0

        return market_prices
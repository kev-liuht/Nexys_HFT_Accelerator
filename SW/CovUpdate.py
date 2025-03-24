#!/usr/bin/env python3
class CovarianceUpdateStack:
    """Maintains historical price data, updates the covariance matrix."""
    def __init__(self, num_stocks=4):
        self.num_stocks = num_stocks
        self.last_prices = [None] * self.num_stocks                                         # Store last price
        self.num_updates = 0                                                                # Track the number of updates (N)
        self.last_returns = [0] * self.num_stocks                                           # E[X] at time step N
        self.last_second_moment = [[0] * self.num_stocks for _ in range(self.num_stocks)]   # E[XX^T] at time step N
        self.cov_matrix = [[0] * self.num_stocks for _ in range(self.num_stocks)]           # Covariance matrix

    def update(self, market_prices):
        """Updates the covariance matrix using new market prices."""
        returns = [0] * self.num_stocks

        if self.num_updates == 0:
            # First update: Store initial prices only
            for i in range(self.num_stocks):
                self.last_prices[i] = market_prices[i]
            self.num_updates += 1
            return self.cov_matrix, False   # Return the covariance matrix, and do not proceed to generate order

        else:
            # Compute returns for each stock
            for i in range(self.num_stocks):
                prev_price = self.last_prices[i]
                returns[i] = (market_prices[i] - prev_price) / prev_price # x_i = (P_current - P_prev) / P_prev

            # Use incremental formula
            for i in range(self.num_stocks):
                self.last_returns[i] = ((self.num_updates) * self.last_returns[i] + returns[i]) / (self.num_updates + 1)  # Formula 9

            for i in range(self.num_stocks):
                for j in range(self.num_stocks):
                    self.last_second_moment[i][j] = ((self.num_updates) * self.last_second_moment[i][j] + returns[i] * returns[j]) / (self.num_updates + 1)  # Formula 8
                    self.cov_matrix[i][j] = (self.last_second_moment[i][j] - self.last_returns[i] * self.last_returns[j])  # Formula 7

        # Update last prices
        for i in range(self.num_stocks):
            self.last_prices[i] = market_prices[i]

        self.num_updates += 1

        return self.cov_matrix, True    # Return the covariance matrix, and proceed to generate order

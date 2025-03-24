#!/usr/bin/env python3
class QRDecompLinSolver:
    """
    QRDecompLinSolver performs Givens-based QR decomposition on a 4x4 matrix,
    solves the linear system K * x = 1 using back substitution, normalizes the
    result so that the sum equals 1, clamps negative weights to zero, and
    redistributes the positive weights so that the final sum is 1.
    """

    def __init__(self, N=4):
        self.N = N

    def my_sqrt(self, value, iterations=20):
        """
        Newton's method sqrt.
        Computes the square root of a positive value using an iterative method.
        """
        if value < 0:
            raise ValueError("Cannot compute sqrt of a negative value.")
        if value == 0:
            return 0.0
        x = value
        for _ in range(iterations):
            x = 0.5 * (x + value / x)
        return x

    def givens_qr(self, A, b):
        """
        Givens based QR decomposition for a 4x4 matrix.
        Performs rotations to zero out below-diagonal elements of A.
        The same rotation is applied to vector b.
        """
        N = self.N
        for i in range(N):
            for j in range(i + 1, N):
                a_val = A[i][i]
                b_val = A[j][i]
                r = self.my_sqrt(a_val * a_val + b_val * b_val)
                if r == 0.0:
                    continue
                c = a_val / r
                s = b_val / r
                for k in range(i, N):
                    temp = c * A[i][k] + s * A[j][k]
                    A[j][k] = -s * A[i][k] + c * A[j][k]
                    A[i][k] = temp
                # Rotate vector b
                temp_b = c * b[i] + s * b[j]
                b[j] = -s * b[i] + c * b[j]
                b[i] = temp_b

    def back_substitution(self, A, b):
        """
        Back substitution to solve A*x = b for an upper-triangular 4x4 matrix A.
        """
        N = self.N
        x = [0.0] * N
        for i in range(N - 1, -1, -1):
            sum_ax = 0.0
            for j in range(i + 1, N):
                sum_ax += A[i][j] * x[j]
            x[i] = (b[i] - sum_ax) / A[i][i]
        return x

    def solve(self, K):
        """
        Reads a 4x4 matrix K (row-major order), sets b = [1, 1, 1, 1],
        performs QR decomposition and back substitution, normalizes the result so that
        the sum equals 1, clamps negative weights to zero, and redistributes the weights
        so that the final sum is 1.
        
        Parameters:
          K: 4x4 matrix (list of 4 lists, each containing 4 floats)
        
        Returns:
          A list of 4 floats representing the computed weight vector.
        """
        # Copy input matrix to avoid modifying the original.
        A = [row[:] for row in K]
        # Define b = [1, 1, 1, 1]
        b = [1.0, 1.0, 1.0, 1.0]

        # Perform QR decomposition using Givens rotations.
        self.givens_qr(A, b)

        # Perform back substitution to solve for x.
        x = self.back_substitution(A, b)

        # First normalization: make sum(x) == 1.
        sum_x = sum(x)
        if sum_x != 0.0:
            x = [val / sum_x for val in x]

        # Clamp negative weights to zero.
        x = [val if val >= 0.0 else 0.0 for val in x]

        # Redistribute among remaining positive weights so that the final sum equals 1.
        sum_pos = sum(x)
        if sum_pos != 0.0:
            x = [val / sum_pos for val in x]

        return x


# Example: Test the implementation with a sample 4x4 matrix K.
if __name__ == "__main__":
    K = [
        [-0.00428772,  0.00657654,  0.00419617, -0.00871277],
        [ 0.00880432, -0.01359558, -0.00871277,  0.01789856],
        [ 0.0043335,  -0.00671387, -0.00428772,  0.00880432],
        [-0.00671387,  0.01028442,  0.00657654, -0.01359558]
    ]

    # Create an instance of the QRDecompLinSolver class.
    qr_solver = QRDecompLinSolver()

    # Compute the min-variance weights.
    weights = qr_solver.solve(K)
    print("Computed Portfolio Weights:")
    for i, w in enumerate(weights):
        print(f"  Asset {i+1}: {w:.6f}")

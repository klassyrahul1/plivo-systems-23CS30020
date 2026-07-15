# Experiment Log

*   **Run 1-6 (Profile B):** Initial testing and baseline establishment at 100ms.
*   **Run 7 (Profile B):** `delay_ms=90`, Miss=0.60%, Overhead=2.00x, VALID.
    *   *Note:* Stride-2 FEC implementation successfully cleared the 1% miss cap.
*   **Run 8 (Profile B):** `delay_ms=85`, Miss=0.60%, Overhead=2.00x, VALID.
    *   *Note:* Continued optimization of jitter buffer; performance remained stable.
*   **Run 9 (Profile B):** `delay_ms=82`, Miss=0.73%, Overhead=2.00x, VALID.
    *   *Note:* Optimized FEC logic and timing margins pushed latency to a new floor.
*   **Run 10 (Profile B):** `delay_ms=80`, Miss=1.53%, Overhead=2.00x, INVALID.
    *   *Note:* Hit the physical latency floor; jitter exceeded the 2ms early-flush window, causing deadline misses.

**FINAL SCORE LOCKED:** 82ms

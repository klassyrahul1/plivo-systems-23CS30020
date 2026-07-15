# Experiment Log

*   **Run 1 (Profile A):** `delay_ms=40`, Miss=4.73%, Overhead=1.02x, INVALID. 
    *   *Note:* Naive baseline. Fails due to unrecovered drops and tight playout deadlines.
*   **Run 2 (Profile A):** `delay_ms=60`, Miss=0.33%, Overhead=1.55x, VALID. 
    *   *Note:* Implemented 2:1 XOR FEC and a 2ms early-flush jitter buffer. Loss recovery successful.
*   **Run 3 (Profile B):** `delay_ms=90`, Miss=1.40%, Overhead=1.55x, INVALID. 
    *   *Note:* Stress testing on harsher profile. Misses due to max delay spikes of 80ms.
*   **Run 4 (Profile B):** `delay_ms=120`, Miss=0.80%, Overhead=1.55x, VALID. 
    *   *Note:* Increased delay to clear jitter bounds. Unrecoverable burst drops account for the 0.80% floor.
*   **Run 5 (Profile B):** `delay_ms=110`, Miss=0.80%, Overhead=1.55x, VALID. 
    *   *Note:* Stepping down delay.
*   **Run 6 (Profile B):** `delay_ms=100`, Miss=0.87%, Overhead=1.55x, VALID. 
    *   *Note:* Margin shrinks to just 2 frames before cap.
*   **Run 7 (Profile B):** `delay_ms=95`, Miss=1.13%, Overhead=1.55x, INVALID. 
    *   *Note:* Breached the 1.00% cap by 2 frames. 100ms is the confirmed floor.

**FINAL SCORE LOCKED:** 100ms
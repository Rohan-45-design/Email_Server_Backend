#pragma once
inline long computeBackoff(int retryCount) {
    // seconds: 1m, 5m, 30m, 2h, 6h, 1d
    static long table[] = {60, 300, 1800, 7200, 21600, 86400};
    if (retryCount >= 6) return table[5];
    return table[retryCount];
}

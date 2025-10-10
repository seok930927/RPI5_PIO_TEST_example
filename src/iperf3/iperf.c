#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "iperf.h"

uint64_t time_us_64()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000);
}

uint32_t get_time_us()
{
    return (uint32_t)time_us_64();
}

void iperf_stats_init(Stats *stats, uint32_t pacing_timer_ms)
{
    stats->pacing_timer_us = pacing_timer_ms * 1000;
    stats->running = false;
    stats->t0 = 0;
    stats->t1 = 0;
    stats->t3 = 0;
    stats->nb0 = 0;
    stats->nb1 = 0;
    stats->np0 = 0;
    stats->np1 = 0;
}

void iperf_stats_start(Stats *stats)
{
    stats->running = true;
    stats->t0 = stats->t1 = get_time_us();
    stats->nb0 = stats->nb1 = 0;
    stats->np0 = stats->np1 = 0;
    printf("Interval           Transfer     Bitrate\n");
}

void iperf_stats_update(Stats *stats, bool final)
{
    if (!stats->running) return;

    uint32_t t2 = get_time_us();
    uint32_t dt = t2 - stats->t1;  // Elapsed time since last update

    if (final || dt > stats->pacing_timer_us) {
        double ta = (stats->t1 - stats->t0) / 1e6;  // Start time of the previous interval
        double tb = (t2 - stats->t0) / 1e6;         // End time of the current interval
        double transfer_mbits = (stats->nb1 * 8) / 1e6 / (dt / 1e6);  // Calculate Mbps

#ifdef IPERF_DEBUG
        printf("%5.2f-%-5.2f sec %8u Bytes  %5.2f Mbits/sec\n",
               ta, tb, stats->nb1, transfer_mbits);
#endif

        stats->t1 = t2;  // Update the timer
        stats->nb1 = 0;  // Reset byte count per interval
        stats->np1 = 0;  // Reset packet count per interval
    }
}

void iperf_stats_stop(Stats *stats)
{
    stats->running = false;

    stats->t3 = get_time_us();
    uint32_t total_time_us = stats->t3 - stats->t0;
    double total_time_s = total_time_us / 1e6;
    double transfer_mbits = (stats->nb0 * 8) / 1e6 / total_time_s;

    printf("------------------------------------------------------------\n");
    printf("Total: %5.2f sec %8u Bytes  %5.2f Mbits/sec\n",
           total_time_s, stats->nb0, transfer_mbits);
}

void iperf_stats_add_bytes(Stats *stats, uint32_t n) {
    if (!stats->running) return;

    stats->nb0 += n;  // Increase total byte count
    stats->nb1 += n;  // Increase byte count per interval
    stats->np0 += 1;  // Increase total packet count
    stats->np1 += 1;  // Increase packet count per interval

}
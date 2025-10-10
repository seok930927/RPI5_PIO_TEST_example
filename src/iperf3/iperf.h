#include <stdint.h>
#include <stdbool.h>

/* Iperf debug message printout enable */
#define IPERF_DEBUG

typedef struct {
    uint32_t pacing_timer_us;  // Timer period (in microseconds)
    bool running;              // Execution flag (indicates whether stats tracking is active)
    uint32_t t0;               // Test start time
    uint32_t t1;               // Last update time
    uint32_t t3;               // Test end time
    uint32_t nb0;              // Total number of bytes
    uint32_t nb1;              // Number of bytes per interval
    uint32_t np0;              // Total number of packets
    uint32_t np1;              // Number of packets per interval
} Stats;

void iperf_stats_init(Stats *stats, uint32_t pacing_timer_ms);
void iperf_stats_start(Stats *stats);
void iperf_stats_update(Stats *stats, bool final);
void iperf_stats_stop(Stats *stats);
void iperf_stats_add_bytes(Stats *stats, uint32_t n);
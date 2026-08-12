#ifndef COMM_CONFIG_H
#define COMM_CONFIG_H

/* ---- System tick ---- */
#define SYSTEM_TICK_US           100UL

/* ---- Physical time constants (microseconds) ---- */
#define UART_FRAME_GAP_US       4000UL
#define SPI_BYTE_GAP_US         1000UL
#define SPI_BYTE_TIMEOUT_US     5000UL

/* ---- Tick conversion ---- */
#define US_TO_TICKS(us_) \
    (((us_) + SYSTEM_TICK_US - 1UL) / SYSTEM_TICK_US)

/* ---- Derived tick values ---- */
#define LED_DURATION_TICKS      500U

#define UART_FRAME_CAPACITY      64U
#define UART_FRAME_GAP_TICKS     US_TO_TICKS(UART_FRAME_GAP_US)

/* ---- SPI-A fixed parameters ---- */
#define SPI_BRR                   127U
#define SPI_BYTE_GAP_TICKS       US_TO_TICKS(SPI_BYTE_GAP_US)
#define SPI_BYTE_TIMEOUT_TICKS   US_TO_TICKS(SPI_BYTE_TIMEOUT_US)

/* ---- Compile-time sanity checks ---- */
#if (UART_FRAME_GAP_TICKS == 0) || (SPI_BYTE_GAP_TICKS == 0) || (SPI_BYTE_TIMEOUT_TICKS == 0)
#error "Tick values must be > 0"
#endif
#if (UART_FRAME_GAP_TICKS != 40)
#error "UART_FRAME_GAP_TICKS must be 40"
#endif
#if (SPI_BYTE_GAP_TICKS != 10)
#error "SPI_BYTE_GAP_TICKS must be 10"
#endif
#if (SPI_BYTE_TIMEOUT_TICKS != 50)
#error "SPI_BYTE_TIMEOUT_TICKS must be 50"
#endif

#endif

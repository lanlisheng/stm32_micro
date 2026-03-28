# USART1 Echo Test

## Firmware behavior

- `USART1` is configured as `115200, 8-N-1`.
- RX uses `HAL_UART_Receive_IT()` single-byte interrupt reception.
- Every byte received from the PC is pushed into a software buffer.
- The main loop sends buffered bytes back through `USART1`.
- The firmware prints a CPU usage snapshot for `USART1` every `1` second.
- Debug logs are output through `SEGGER RTT`.
- After reset, the MCU sends:

```text
USART1 echo test ready.
Send any data from PC, MCU will echo it back.
```

- During runtime, the MCU also prints messages like:
  These messages are visible in `J-Link RTT Viewer` or the RTT window in your debugger.

```text
[USART1 CPU 1000ms] RX:16B/16IRQ 0.1%, TX:95B/3Call 0.8%, TOTAL:0.9%
```

## PC test steps

1. Flash the project to the MCU.
2. Open a serial tool on the PC.
3. Select the serial port connected to `USART1`.
4. Set baud rate to `115200`, data bits `8`, stop bits `1`, parity `None`.
5. Send any string, for example `hello stm32`.
6. Expected result: the MCU returns exactly `hello stm32`.
7. Keep sending data and observe the periodic CPU usage report from the MCU.

## RTT usage

1. Connect the debugger with J-Link RTT support.
2. Open `J-Link RTT Viewer` or the IDE RTT terminal.
3. Select the target device and connect.
4. Expected RTT output:

```text
[RTT] SEGGER RTT ready.
[APP] USART1 echo test ready.
[USART1 CPU 1000ms] RX:16B/16IRQ 0.1%, TX:16B/1Call 0.1%, TOTAL:0.2%
```

## Suggested test data

- `abc123`
- `Hello, USART1!`
- `STM32H7 echo test\r\n`

## Notes

- If the PC sends data continuously faster than the main loop can forward it, the firmware reports `RX buffer overflow`.
- Current software RX buffer size is `128` bytes and can be adjusted in `Core/Inc/usart.h`.
- `RX` percentage measures CPU time used inside the UART receive interrupt callback.
- `TX` percentage measures CPU time used by foreground UART transmit calls.

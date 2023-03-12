// Stupid bit of code that does the bare minimum to make os_printf work.

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

#include "config.h"
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "io.h"
#include "osapi.h"
#include "syslog.h"
#include "uart_hw.h"
#include <time.h>
// Temperature reading timestamps (used for thermostat if configured)
int serialTreading = -9999;
time_t serialTreadingTS;    // timestamp for the reading
char userJSON[USERJSONMAX]; // user defined JSON submitted via serial

typedef struct {
  uint32 RcvBuffSize;
  uint8 *pRcvMsgBuff;
  uint8 *pWritePos;
  uint8 *pReadPos;
  uint8 TrigLvl; // JLU: may need to pad
  RcvMsgBuffState BuffState;
} RcvMsgBuff;

typedef struct {
  UartBautRate baut_rate;
  UartBitsNum4Char data_bits;
  UartExistParity exist_parity;
  UartParityMode parity; // chip size in byte
  UartStopBitsNum stop_bits;
  UartFlowCtrl flow_ctrl;
  RcvMsgBuff rcv_buff;
  TrxMsgBuff trx_buff;
  RcvMsgState rcv_state;
  int received;
  int buff_uart_no; // indicate which uart use tx/rx buffer
} UartDevice;

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

int serialTreading;

static void ICACHE_FLASH_ATTR stdoutUartTxd(char c) {
  // Wait until there is room in the FIFO
  while (((READ_PERI_REG(UART_STATUS(0)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) >= 126)
    ;
  // Send the character
  WRITE_PERI_REG(UART_FIFO(0), c);
}

static void ICACHE_FLASH_ATTR stdoutPutchar(char c) {
  // convert \n -> \r\n
  if (c == '\n')
    stdoutUartTxd('\r');
  stdoutUartTxd(c);
}

LOCAL void uart0_rx_intr_handler(void *para) {
  /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
   * uart1 and uart0 respectively
   */
  RcvMsgBuff *pRxBuff = (RcvMsgBuff *)para;
  uint8 RcvChar;

  if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
    return;
  }

  WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);

  while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
    RcvChar = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

    *(pRxBuff->pWritePos) = RcvChar;

    // insert here for get one command line from uart
    if (RcvChar == '\n') {
      *(pRxBuff->pWritePos) = 0x00;
      pRxBuff->BuffState = WRITE_OVER;
      pRxBuff->pWritePos = pRxBuff->pRcvMsgBuff - 1;
      os_printf("Received: %s\n", (const char *)pRxBuff->pRcvMsgBuff);

      if (os_strncmp((const char *)pRxBuff->pRcvMsgBuff, "?", 1) == 0 ||
          os_strncmp((const char *)pRxBuff->pRcvMsgBuff, "help", 4) == 0) {
        os_printf("ESP8266 Relay Board Serial Interface\n");
        os_printf("Commands:\n");
        os_printf("serialremotetemp=nn  Set thermostat room temp to nn (tenths of a degree, int only)\n");
        os_printf("userjson={xxxx}  Submit user defined JSON (max chars %d) which will be output by "
                  "thermostat.cgi?state & MQTT\n",
                  USERJSONMAX);
        os_printf("relayX=Y   Set relay X to on(Y=1) or off(Y=0)\n");
      }

      if (os_strncmp((const char *)pRxBuff->pRcvMsgBuff, "serialremotetemp=", 17) == 0) {
        int len = strlen((const char *)pRxBuff->pRcvMsgBuff + 17);
        if (len > 4) {
          os_printf("Invalid temperature, must be < 4 digits\n");
        } else {
          serialTreading = atoi((const char *)pRxBuff->pRcvMsgBuff + 17);
          if (serialTreading > 900) {
            serialTreading = -9999;
            os_printf("Invalid temperature, must be < 900 (90 degrees)\n");
          } else {
            serialTreadingTS = sntp_get_current_timestamp();
            os_printf("Serial temperature is: %d \r\n", serialTreading);
          }
        }
      }

      if (os_strncmp((const char *)pRxBuff->pRcvMsgBuff, "userjson=", 9) == 0) {
        int len = strlen((const char *)pRxBuff->pRcvMsgBuff + 9);

        if (len > USERJSONMAX) {
          os_printf("JSON string too long (%d chars), max chars is %d.", len, USERJSONMAX);
        } else {

          if (*(const char *)(pRxBuff->pRcvMsgBuff + 9) != '{') {
            os_printf("Bad JSON string, missing starting {\n");
          } else if (*(const char *)(pRxBuff->pRcvMsgBuff + 9 + len - 2) != '}') {
            os_printf("Bad JSON string, missing ending }\n");
          } else {
            strcpy(userJSON, (const char *)pRxBuff->pRcvMsgBuff + 9);
            os_printf("User defined JSON string: %s \r\n", userJSON);
          }
        }
      }

      if (os_strncmp((const char *)pRxBuff->pRcvMsgBuff, "relay", 5) == 0) {
        int relayNum = (pRxBuff->pRcvMsgBuff[5] - '0');
        int relayState = (pRxBuff->pRcvMsgBuff[7] - '0');

        relayOnOff(relayState, relayNum);
        // os_printf("Relay %d is now: %d \r\n", relayNum, relayState);
      }
    }

    if (pRxBuff->pWritePos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE)) {
      // overflow ...we may need more error handle here.
      pRxBuff->pWritePos = pRxBuff->pRcvMsgBuff;
    } else {
      pRxBuff->pWritePos++;
    }

    if (pRxBuff->pWritePos == pRxBuff->pReadPos) { // overflow one byte, need push pReadPos one byte ahead
      if (pRxBuff->pReadPos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE)) {
        pRxBuff->pReadPos = pRxBuff->pRcvMsgBuff;
      } else {
        pRxBuff->pReadPos++;
      }
    }
  }
}

void ICACHE_FLASH_ATTR stdoutInit() {

  /* rcv_buff size if 0x100 */
  ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, &(UartDev.rcv_buff));

  // Enable TxD pin
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
  PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0);

  // Set baud rate and other serial parameters to 115200,n,8,1
  uart_div_modify(0, UART_CLK_FREQ / BIT_RATE_74880);
  WRITE_PERI_REG(UART_CONF0(0),
                 (STICK_PARITY_DIS) | (ONE_STOP_BIT << UART_STOP_BIT_NUM_S) | (EIGHT_BITS << UART_BIT_NUM_S));

  // Reset tx & rx fifo
  SET_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST | UART_TXFIFO_RST);

  // set rx fifo trigger
  WRITE_PERI_REG(UART_CONF1(0), ((0x01 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                                    ((0x01 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) | UART_RX_FLOW_EN);

  // Install our own putchar handler
  os_install_putc1((void *)stdoutPutchar);

  // clear all interrupt
  WRITE_PERI_REG(UART_INT_CLR(0), 0xffff);

  // enable rx_interrupt
  SET_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_FULL_INT_ENA);

  ETS_UART_INTR_ENABLE();
}

/*
* The Clear BSD License
* Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
* All rights reserved.
*
* 
* Redistribution and use in source and binary forms, with or without modification,
* are permitted (subject to the limitations in the disclaimer below) provided
*  that the following conditions are met:
*
* o Redistributions of source code must retain the above copyright notice, this list
*   of conditions and the following disclaimer.
*
* o Redistributions in binary form must reproduce the above copyright notice, this
*   list of conditions and the following disclaimer in the documentation and/or
*   other materials provided with the distribution.
*
* o Neither the name of the copyright holder nor the names of its
*   contributors may be used to endorse or promote products derived from this
*   software without specific prior written permission.
*
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "lwip/opt.h"

#if LWIP_SOCKET
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/tcpip.h"
#include "lwip/ip.h"
#include "lwip/sockets.h"
#include "netif/etharp.h"

#include "ethernetif.h"
#include "board.h"

#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "ptpd.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
   
uint32_t PTP_E2E_MODE = 1;

uint32_t PTP_MASTER_MODE = 1;

uint32_t ms_ep = 0;
   
/* Test Master or Slave */

#define PTP_TEST_APP_MASTER 1

#if PTP_TEST_APP_MASTER
#define PTP_APP_MASTER_TEST true
#else
#define PTP_APP_MASTER_TEST false
#endif


/* IP address configuration. */
#define configIP_ADDR0 192
#define configIP_ADDR1 168
#define configIP_ADDR2 0
#if PTP_TEST_APP_MASTER
#define configIP_ADDR3 102
#else
#define configIP_ADDR3 103
#endif

/* Netmask configuration. */
#define configNET_MASK0 255
#define configNET_MASK1 255
#define configNET_MASK2 255
#define configNET_MASK3 0

/* Gateway address configuration. */
#define configGW_ADDR0 192
#define configGW_ADDR1 168
#define configGW_ADDR2 0
#define configGW_ADDR3 100

/* MAC address configuration. */
#define configMAC_ADDR0 0x02
#define configMAC_ADDR1 0x12
#define configMAC_ADDR2 0x13
#define configMAC_ADDR3 0x10
#define configMAC_ADDR4 0x15
#if PTP_TEST_APP_MASTER
#define configMAC_ADDR5 0x11
#else
#define configMAC_ADDR5 0x12
#endif

#define configMAC_ADDR {configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2,\
                        configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5}

/* Address of PHY interface. */
#define EXAMPLE_PHY_ADDRESS BOARD_ENET0_PHY_ADDRESS

/* System clock name. */
#define EXAMPLE_CLOCK_NAME kCLOCK_CoreSysClk




#ifndef PTPD_DEBUG
#define PTPD_DEBUG LWIP_DBG_ON
#endif
#ifndef PTPD_STACKSIZE
#define PTPD_STACKSIZE 1024 //DEFAULT_THREAD_STACKSIZE
#endif
#ifndef PTPD_PRIORITY
#define PTPD_PRIORITY DEFAULT_THREAD_PRIO
#endif
#ifndef DEBUG_WS
#define DEBUG_WS 0
#endif

/*******************************************************************************
* Prototypes
******************************************************************************/

/*******************************************************************************
* Variables
******************************************************************************/
static struct netif fsl_netif0;

/*******************************************************************************
 * Code
 ******************************************************************************/
void BOARD_InitModuleClock(void)
{
    const clock_enet_pll_config_t config = {true, true, 1, 0};
    CLOCK_InitEnetPll(&config);
}

void delay(void)
{
    volatile uint32_t i = 0;
    for (i = 0; i < 30000000; ++i)
    {
        __asm("NOP"); /* delay */
    }
}



/*!
 * @brief Initializes lwIP and ptpd stack.
 */
static void stack_init(void* pvParameters)
{
	(void)pvParameters;

    ip4_addr_t fsl_netif0_ipaddr, fsl_netif0_netmask, fsl_netif0_gw;
    ethernetif_config_t fsl_enet_config0 = {
        .phyAddress = EXAMPLE_PHY_ADDRESS,
        .clockName = EXAMPLE_CLOCK_NAME,
        .macAddress = configMAC_ADDR,
    };

    tcpip_init(NULL, NULL);
    
    LWIP_PLATFORM_DIAG(("\r\n Heyyyyyy"));

    IP4_ADDR(&fsl_netif0_ipaddr, configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3);
    IP4_ADDR(&fsl_netif0_netmask, configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3);
    IP4_ADDR(&fsl_netif0_gw, configGW_ADDR0, configGW_ADDR1, configGW_ADDR2, configGW_ADDR3);

    netif_add(&fsl_netif0, &fsl_netif0_ipaddr, &fsl_netif0_netmask, &fsl_netif0_gw,
              &fsl_enet_config0, ethernetif0_init, tcpip_input);
    netif_set_default(&fsl_netif0);
    netif_set_up(&fsl_netif0);
//    while(1)
//		__asm("nop");
//      LWIP_PLATFORM_DIAG(("\r\n************************************************"));


    LWIP_PLATFORM_DIAG(("\r\n************************************************"));
    LWIP_PLATFORM_DIAG((" PTPD Protocal example"));
    LWIP_PLATFORM_DIAG(("************************************************"));
    LWIP_PLATFORM_DIAG((" IPv4 Address     : %u.%u.%u.%u", ((u8_t *)&fsl_netif0_ipaddr)[0],
                        ((u8_t *)&fsl_netif0_ipaddr)[1], ((u8_t *)&fsl_netif0_ipaddr)[2],
                        ((u8_t *)&fsl_netif0_ipaddr)[3]));
    LWIP_PLATFORM_DIAG((" IPv4 Subnet mask : %u.%u.%u.%u", ((u8_t *)&fsl_netif0_netmask)[0],
                        ((u8_t *)&fsl_netif0_netmask)[1], ((u8_t *)&fsl_netif0_netmask)[2],
                        ((u8_t *)&fsl_netif0_netmask)[3]));
    LWIP_PLATFORM_DIAG((" IPv4 Gateway     : %u.%u.%u.%u", ((u8_t *)&fsl_netif0_gw)[0], ((u8_t *)&fsl_netif0_gw)[1],
                        ((u8_t *)&fsl_netif0_gw)[2], ((u8_t *)&fsl_netif0_gw)[3]));
    LWIP_PLATFORM_DIAG(("************************************************"));
    
    
    if( (PTP_MASTER_MODE == 1) && (PTP_E2E_MODE == 1)){
      ms_ep = 0;}
    else if((PTP_MASTER_MODE == 1) && (PTP_E2E_MODE == 0)){
      ms_ep = 1;}
    else if((PTP_MASTER_MODE == 0) && (PTP_E2E_MODE == 1)){
      ms_ep = 2;}
    else{
      ms_ep = 3;}
      
    
    /* Initialize the PTP daemon. */
    //ptpd_init(PTP_APP_MASTER_TEST);
    ptpd_init(ms_ep);

	vTaskDelete(NULL);
}

/*!
 * @brief Main function.
 */
int main(void)
{
    gpio_pin_config_t gpio_config = {kGPIO_DigitalOutput, 0, kGPIO_NoIntmode};
    
    // Thomas
    gpio_pin_config_t gpio_p2p_e2e = {kGPIO_DigitalInput, 0, kGPIO_NoIntmode};

    BOARD_ConfigMPU();
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    BOARD_InitModuleClock();

    IOMUXC_EnableMode(IOMUXC_GPR, kIOMUXC_GPR_ENET1TxClkOutputDir, true);

    GPIO_PinInit(GPIO1, 9, &gpio_config);
    GPIO_PinInit(GPIO1, 10, &gpio_config);
    GPIO_PinInit(GPIO1, 19, &gpio_config);
    GPIO_WritePinOutput(GPIO1, 19, 0);
    
    // Thomas
    GPIO_PinInit(GPIO1, 3, &gpio_p2p_e2e);
    GPIO_PinInit(GPIO1, 2, &gpio_p2p_e2e);
    
    /* PTP EVENT output */
    GPIO_PinInit(GPIO1, 10, &gpio_config);
    /* pull up the ENET_INT before RESET. */
    GPIO_WritePinOutput(GPIO1, 10, 1);
    GPIO_WritePinOutput(GPIO1, 9, 0);
    delay();
    GPIO_WritePinOutput(GPIO1, 9, 1);
    
    // Thomas
    PTP_E2E_MODE = GPIO_PinRead(GPIO1, 3);
    PTP_MASTER_MODE = GPIO_PinRead(GPIO1, 2);

    /* create server thread in RTOS */
    if(sys_thread_new("stack_init", stack_init, NULL, PTPD_STACKSIZE, PTPD_PRIORITY) == NULL)
        LWIP_ASSERT("main(): Task creation failed.", 0);

    /* run RTOS */
    vTaskStartScheduler();

    /* should not reach this statement */
    return 0;
}

#endif // LWIP_SOCKET

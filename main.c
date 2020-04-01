/***************************************************************************//**
 * @file
 * @brief This application's purpose is to put IQ samples to standard serial
 * interface.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "rail.h"
#include "hal_common.h"
#include "rail_config.h"

#include "em_chip.h"
#include "em_cmu.h"

#include "hal-config.h"

#define RX_FIFO_SIZE 512
#define FIFO_THRESHOLD 400

// TODO: We can get it from the log or from reading out certain registers (Zoli, what do you prefer?)
#define SAMPLE_FREQ
#define SAMPLE_NUMBER 4 * 1024
#define SAMPLE_IN_BYTES 2

/* Power of two: ring buffer implementation required */
#define BUFFER_LENGTH SAMPLE_NUMBER * SAMPLE_IN_BYTES

void RAILCb_Generic(RAIL_Handle_t railHandle, RAIL_Events_t events);

RAIL_Handle_t railHandle;

static RAIL_Config_t railCfg = { .eventsCallback = &RAILCb_Generic, };

/* TODO: can we put it to the flash? */
static volatile uint8_t buffer[BUFFER_LENGTH];
static volatile uint32_t bufferWritePtr = 0;

static volatile bool bufferFull = false;

void initRadio() {

	halInit();
	railHandle = RAIL_Init(&railCfg, NULL);
	if (railHandle == NULL) {
		while (1)
			;
	}
	RAIL_ConfigCal(railHandle, RAIL_CAL_ALL);

	// Set us to a valid channel for this config and force an update in the main
	// loop to restart whatever action was going on
	RAIL_ConfigChannels(railHandle, channelConfigs[0], NULL);

	// Initialize the PA now that the HFXO is up and the timing is correct
	RAIL_TxPowerConfig_t txPowerConfig = {
#if HAL_PA_2P4_LOWPOWER
			.mode = RAIL_TX_POWER_MODE_2P4_LP,
#else
			.mode = RAIL_TX_POWER_MODE_2P4_HP,
#endif
			.voltage = BSP_PA_VOLTAGE, .rampTime = HAL_PA_RAMP, };
#if RAIL_FEAT_SUBGIG_RADIO
	if (channelConfigs[0]->configs[0].baseFrequency < 1000000000UL) {
		// Use the Sub-GHz PA if required
		txPowerConfig.mode = RAIL_TX_POWER_MODE_SUBGIG;
	}
#endif
	if (RAIL_ConfigTxPower(railHandle, &txPowerConfig) != RAIL_STATUS_NO_ERROR) {
		// Error: The PA could not be initialized due to an improper configuration.
		// Please ensure your configuration is valid for the selected part.
		while (1)
			;
	}
	RAIL_SetTxPower(railHandle, HAL_PA_POWER);

	RAIL_Events_t events = RAIL_EVENT_RX_FIFO_ALMOST_FULL
			| RAIL_EVENT_RX_FIFO_OVERFLOW;
	RAIL_ConfigEvents(railHandle, events, events);

	RAIL_SetRxFifoThreshold(railHandle, (uint16_t) FIFO_THRESHOLD);

	RAIL_DataConfig_t dataConfig = { .rxMethod = FIFO_MODE, .rxSource =
			RX_IQDATA_FILTLSB, .txMethod =
	FIFO_MODE, .txSource = TX_PACKET_DATA };

	RAIL_ConfigData(railHandle, &dataConfig);

}

static void printIQSamples() {

	for (uint32_t i = 0; i < BUFFER_LENGTH; i += 2) {
		/* TODO: Maybe we can do it much faster either with DMA or printing
		 * more than one sample at once. */
		printf("%d\n", (int16_t) ((buffer[i+1] << 8) + buffer[i]));
	}

}

int main(void) {

	CHIP_Init();
	initRadio();

	RAIL_StartRx(railHandle, 0, NULL);

	while (1) {
		if (bufferFull) {
			RAIL_Idle(railHandle, RAIL_IDLE, false);
			bufferFull = false;
			printIQSamples();
		}
	}

	return 0;

}

void RAILCb_Generic(RAIL_Handle_t railHandle, RAIL_Events_t events) {

	if (events & RAIL_EVENT_RX_FIFO_ALMOST_FULL) {
		/* Move data */
		int i;
		BSP_LedToggle(1);
		uint16_t readLength =
		BUFFER_LENGTH - bufferWritePtr > FIFO_THRESHOLD ?
				FIFO_THRESHOLD : BUFFER_LENGTH - bufferWritePtr;
		bufferWritePtr += RAIL_ReadRxFifo(railHandle, &buffer[bufferWritePtr],
				readLength);
//		RAIL_ResetFifo(railHandle, false, true);
		if (bufferWritePtr == BUFFER_LENGTH) {
			bufferFull = true;
		}

	}

	if (events & RAIL_EVENT_RX_FIFO_OVERFLOW) {
		/* Critical error */
		printf("Critical error, reset the device!\n");
		while (1)
			;
	}

}

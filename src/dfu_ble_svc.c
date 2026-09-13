/**
 * Copyright (c) 2017 - 2017, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "dfu_ble_svc.h"
#include <string.h>
#include "nrf_error.h"
#include "crc16.h"

#if defined ( __CC_ARM )
  static dfu_ble_peer_data_t m_peer_data __attribute__((section("NoInit"), zero_init));            /**< This variable should be placed in a non initialized RAM section in order to be valid upon soft reset from application into bootloader. */
  static uint16_t            m_peer_data_crc __attribute__((section("NoInit"), zero_init));        /**< CRC variable to ensure the integrity of the peer data provided. */

#elif defined ( __GNUC__ )
  __attribute__((section(".noinit"))) static dfu_ble_peer_data_t m_peer_data;                      /**< This variable should be placed in a non initialized RAM section in order to be valid upon soft reset from application into bootloader. */
  __attribute__((section(".noinit"))) static uint16_t            m_peer_data_crc;                  /**< CRC variable to ensure the integrity of the peer data provided. */

#elif defined ( __ICCARM__ )
  __no_init static dfu_ble_peer_data_t m_peer_data     @ 0x20003F80;                               /**< This variable should be placed in a non initialized RAM section in order to be valid upon soft reset from application into bootloader. */
  __no_init static uint16_t            m_peer_data_crc @ 0x20003F80 + sizeof(dfu_ble_peer_data_t); /**< CRC variable to ensure the integrity of the peer data provided. */

#endif


/**@brief Function for setting the peer data from application in bootloader before reset.
 *
 * @param[in] p_peer_data  Pointer to the peer data containing keys for the connection.
 *
 * @retval NRF_SUCCES      The data was set succesfully.
 * @retval NRF_ERROR_NULL  If a null pointer was passed as argument.
 */
static uint32_t dfu_ble_peer_data_set(dfu_ble_peer_data_t * p_peer_data)
{
    if (p_peer_data == NULL)
    {
        return NRF_ERROR_NULL;
    }

    uint32_t src = (uint32_t)p_peer_data;
    uint32_t dst = (uint32_t)&m_peer_data;
    // Calculating length in order to check if destination is residing inside source.
    // Source inside the the destination (calculation underflow) is safe a source is read before 
    // written to destination so that when destination grows into source, the source data is no 
    // longer needed.
    uint32_t len = dst - src;

    if (src == dst)
    {
        // Do nothing as source and destination are identical, just calculate crc below.
    }
    else if (len < sizeof(dfu_ble_peer_data_t))
    {
        uint32_t i = 0;

        dst += sizeof(dfu_ble_peer_data_t);
        src += sizeof(dfu_ble_peer_data_t);

        // Copy byte wise backwards when facing overlapping structures.
        while (i++ <= sizeof(dfu_ble_peer_data_t))
        {
            *((uint8_t *)dst--) = *((uint8_t *)src--);
        }
    }
    else
    {
        memcpy((void *)dst, (void *)src, sizeof(dfu_ble_peer_data_t));
    }

    m_peer_data_crc = crc16_compute((uint8_t *)&m_peer_data, sizeof(m_peer_data), NULL);

    return NRF_SUCCESS;
}


/**@brief   Function for handling second stage of SuperVisor Calls (SVC).
 *
 * @details The function will use svc_num to call the corresponding SVC function.
 *
 * @param[in] svc_num    SVC number for function to be executed
 * @param[in] p_svc_args Argument list for the SVC.
 *
 * @return This function returns the error value of the SVC return. For further details, please
 *         refer to the details of the SVC implementation itself.
 *         @ref NRF_ERROR_SVC_HANDLER_MISSING is returned if no SVC handler is implemented for the
 *         provided svc_num.
 */
void C_SVC_Handler(uint8_t svc_num, uint32_t * p_svc_args)
{
    switch (svc_num)
    {
        case DFU_BLE_SVC_PEER_DATA_SET:
            p_svc_args[0] = dfu_ble_peer_data_set((dfu_ble_peer_data_t *)p_svc_args[0]);
            break;

        default:
            p_svc_args[0] = NRF_ERROR_SVC_HANDLER_MISSING;
            break;
    }
}


/**@brief   Function for handling the first stage of SuperVisor Calls (SVC) in assembly.
 *
 * @details The function will use the link register (LR) to determine the stack (PSP or MSP) to be
 *          used and then decode the SVC number afterwards. After decoding the SVC number then
 *          @ref C_SVC_Handler is called for further processing of the SVC.
 */
/* SVC_Handler: src/sd_isr.S dispatches SVC < 0x10 here and the rest to the SoftDevice. */


uint32_t dfu_ble_peer_data_get(dfu_ble_peer_data_t * p_peer_data)
{
    uint16_t crc;

    if (p_peer_data == NULL)
    {
        return NRF_ERROR_NULL;
    }

    crc = crc16_compute((uint8_t *)&m_peer_data, sizeof(m_peer_data), NULL);
    if (crc != m_peer_data_crc)
    {
        return NRF_ERROR_INVALID_DATA;
    }

    *p_peer_data = m_peer_data;

    // corrupt CRC to invalidate shared information.
    m_peer_data_crc++;

    return NRF_SUCCESS;
}

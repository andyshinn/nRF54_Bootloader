/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * nRF54L RRAM flash abstraction layer.
 *
 * nRF54L uses RRAM (Resistive RAM) instead of traditional Flash.
 * Key differences from nRF52 NVMC/Flash:
 *   - RRAM is byte-writable (no need for page erase before write)
 *   - Uses RRAMC controller instead of NVMC
 *   - Page erase is still supported but writes 0xFF pattern rather
 *     than a hardware erase operation
 *   - Write operations are simpler: just write words directly
 *
 * For the bootloader, we keep the page-buffered write strategy for
 * compatibility with the DFU protocol, but simplify erase operations.
 */

#include <stdbool.h>
#include <string.h>
#include "nrf_sdm.h"
#include "flash_nrf5x.h"
#include "boards.h"
#include "dfu_types.h"

#define FLASH_CACHE_INVALID_ADDR  0xffffffff

static uint32_t _fl_addr = FLASH_CACHE_INVALID_ADDR;
static uint8_t _fl_buf[CODE_PAGE_SIZE] __attribute__((aligned(4)));

/**
 * Wait for RRAMC to be ready for a new operation.
 */
static void rramc_wait_ready(void)
{
    while (NRF_RRAMC->READY == 0) {
        // wait
    }
}

/**
 * Enable or disable RRAM writes.
 *
 * RRAM is memory-mapped and directly writable, but only while
 * RRAMC.CONFIG.WEN is set. With WEN clear a store to RRAM does not silently
 * do nothing -- it raises a precise BusFault, which escalates to HardFault.
 * That is what every DFU on this port hit: the app erase faulted on its first
 * word (BFAR = 0x00001000 = DFU_BANK_0_REGION_START) inside
 * dfu_prepare_func_app_erase(), so DFU_STATE_PREPARING never advanced to
 * DFU_STATE_RDY and the init packet was never acknowledged.
 *
 * WRITEBUFSIZE is left at 0 (unbuffered) so each store lands directly and no
 * COMMITWRITEBUF is needed. Keep WEN set only across the writes themselves.
 */
static void rramc_write_enable(bool enable)
{
    rramc_wait_ready();
    NRF_RRAMC->CONFIG = enable ? RRAMC_CONFIG_WEN_Msk : 0;
}

/**
 * Write words to RRAM via direct memory-mapped access.
 * RRAM is directly writable; we just need to ensure RRAMC is ready.
 */
static void rramc_words_write(uint32_t addr, uint32_t const *src, uint32_t num_words)
{
    rramc_write_enable(true);

    volatile uint32_t *dst_ptr = (volatile uint32_t *)addr;
    for (uint32_t i = 0; i < num_words; i++) {
        dst_ptr[i] = src[i];
    }

    rramc_wait_ready();
    rramc_write_enable(false);
}

/**
 * Erase a page by writing 0xFF pattern.
 * RRAM doesn't have a hardware erase; we simulate it by writing all 0xFF.
 */
static void rramc_page_erase(uint32_t addr)
{
    /* Write the 0xFF pattern straight out. The previous version built it in a
     * CODE_PAGE_SIZE stack buffer first -- 4 KB of stack in a bootloader, for
     * a constant. */
    rramc_write_enable(true);

    volatile uint32_t *dst_ptr = (volatile uint32_t *)addr;
    for (uint32_t i = 0; i < CODE_PAGE_SIZE / 4; i++) {
        dst_ptr[i] = 0xFFFFFFFFUL;
    }

    rramc_wait_ready();
    rramc_write_enable(false);
}

void flash_nrf5x_erase (uint32_t dst, uint32_t len)
{
    uint32_t page_addr = dst & ~(CODE_PAGE_SIZE - 1);
    uint32_t const page_count = NRFX_CEIL_DIV(len, CODE_PAGE_SIZE);
    for ( uint32_t i = 0; i < page_count; i++ )
    {
        uint32_t const addr = page_addr + i * CODE_PAGE_SIZE;
        PRINTF("Erase 0x%08lX\r\n", addr);
        rramc_page_erase(addr);
    }
}


void flash_nrf5x_flush (bool need_erase)
{
    if ( _fl_addr == FLASH_CACHE_INVALID_ADDR )
        return;

    // skip the write if contents matches
    if ( memcmp(_fl_buf, (void *) _fl_addr, CODE_PAGE_SIZE) != 0 )
    {
        // RRAM can be overwritten without erase, but if the caller
        // requests erase, we write 0xFF first for protocol compatibility.
        if ( need_erase )
        {
            PRINTF("Erase and ");
            rramc_page_erase(_fl_addr);
        }

        PRINTF("Write 0x%08lX\r\n", _fl_addr);
        rramc_words_write(_fl_addr, (uint32_t *) _fl_buf, CODE_PAGE_SIZE / 4);
    }

    _fl_addr = FLASH_CACHE_INVALID_ADDR;
}

void flash_nrf5x_write (uint32_t dst, void const *src, uint32_t len, bool need_erase)
{
    // While something to write...
    while (len > 0) {

        // Align to start of page
        uint32_t page_addr = dst & ~(CODE_PAGE_SIZE - 1);

        // If page changed, write modified contents
        if ( page_addr != _fl_addr )
        {
            flash_nrf5x_flush(need_erase);

            // Remember new page
            _fl_addr = page_addr;

            // And read its contents
            memcpy(_fl_buf, (void *) page_addr, CODE_PAGE_SIZE);
        }

        // Compute the write offset into the current page
        uint32_t offset_in_page = dst & (CODE_PAGE_SIZE - 1);

        // Compute how many bytes remaining to complete the page
        uint32_t bytes_to_end = CODE_PAGE_SIZE - offset_in_page;

        // Limit the count to the available bytes in the current page
        uint32_t max_bytes = bytes_to_end < len ? bytes_to_end : len;

        // Copy contents to our buffer
        memcpy(_fl_buf + offset_in_page, src, max_bytes);

        // Update variables
        len -= max_bytes;
        dst += max_bytes;
        src  = (void const *) ((uint8_t*)src + max_bytes);
    };
}

/*
 * NVMC / SoC flash API shims for nRF54L.
 * The vendored SDK code calls these; on nRF54L we map them to direct
 * RRAM writes since there is no NVMC peripheral.
 */

uint32_t sd_flash_page_erase(uint32_t page_number)
{
    rramc_page_erase(page_number * CODE_PAGE_SIZE);
    return 0; /* NRF_SUCCESS */
}

void nrfx_nvmc_word_write(uint32_t addr, uint32_t value)
{
    rramc_wait_ready();
    *(volatile uint32_t *)addr = value;
    rramc_wait_ready();
}

void nrfx_nvmc_page_erase(uint32_t addr)
{
    rramc_page_erase(addr);
}

void nrfx_nvmc_words_write(uint32_t addr, uint32_t const *src, uint32_t num_words)
{
    rramc_words_write(addr, src, num_words);
}

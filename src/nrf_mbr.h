/* MBR API shim: s145 on nRF54L ships no Master Boot Record. The SDK 11 DFU code still
 * names the MBR commands, so the types stay and every command reports NOT_SUPPORTED
 * (SoftDevice and bootloader self-update are unavailable, application DFU is not affected). */

#ifndef NRF_MBR_H__
#define NRF_MBR_H__

#include <stdint.h>
#include "nrf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBR_PAGE_SIZE_IN_WORDS  (1024)
#define MBR_SIZE                (0x1000)

enum NRF_MBR_COMMANDS
{
  SD_MBR_COMMAND_COPY_BL,
  SD_MBR_COMMAND_COPY_SD,
  SD_MBR_COMMAND_INIT_SD,
  SD_MBR_COMMAND_COMPARE,
  SD_MBR_COMMAND_VECTOR_TABLE_BASE_SET,
  SD_MBR_COMMAND_RESERVED,
  SD_MBR_COMMAND_IRQ_FORWARD_ADDRESS_SET,
};

typedef struct
{
  uint32_t *src;
  uint32_t *dst;
  uint32_t len;
} sd_mbr_command_copy_sd_t;

typedef struct
{
  uint32_t *ptr1;
  uint32_t *ptr2;
  uint32_t len;
} sd_mbr_command_compare_t;

typedef struct
{
  uint32_t *bl_src;
  uint32_t bl_len;
} sd_mbr_command_copy_bl_t;

typedef struct
{
  uint32_t address;
} sd_mbr_command_vector_table_base_set_t;

typedef struct
{
  uint32_t address;
} sd_mbr_command_irq_forward_address_set_t;

typedef struct
{
  uint32_t command;
  union
  {
    sd_mbr_command_copy_sd_t copy_sd;
    sd_mbr_command_compare_t compare;
    sd_mbr_command_copy_bl_t copy_bl;
    sd_mbr_command_vector_table_base_set_t base_set;
    sd_mbr_command_irq_forward_address_set_t irq_forward_address_set;
  } params;
} sd_mbr_command_t;

static inline uint32_t sd_mbr_command(sd_mbr_command_t *param)
{
  (void) param;
  return NRF_ERROR_NOT_SUPPORTED;
}

#ifdef __cplusplus
}
#endif

#endif // NRF_MBR_H__

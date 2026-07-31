/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Minimal SYSTIMER defs for CPU-frequency measurement in the stub.
 * See ESP-IDF components/soc/esp32p4/register/hw_ver3/soc/systimer_reg.h.
 */

#pragma once

#include <bit_utils.h>
#include "reg_base.h"

#define SYSTIMER_CONF_REG (DR_REG_SYSTIMER_BASE + 0x0)
#define SYSTIMER_TIMER_UNIT0_WORK_EN    (BIT(30))
#define SYSTIMER_CLK_EN    (BIT(31))

#define SYSTIMER_UNIT0_OP_REG (DR_REG_SYSTIMER_BASE + 0x4)
#define SYSTIMER_TIMER_UNIT0_VALUE_VALID    (BIT(29))
#define SYSTIMER_TIMER_UNIT0_UPDATE    (BIT(30))

#define SYSTIMER_UNIT0_VALUE_HI_REG (DR_REG_SYSTIMER_BASE + 0x40)
#define SYSTIMER_UNIT0_VALUE_LO_REG (DR_REG_SYSTIMER_BASE + 0x44)

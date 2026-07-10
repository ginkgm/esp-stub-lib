/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <stdint.h>

#include <soc_utils.h>

#include <target/clock.h>

#include <soc/efuse_reg.h>
#include <soc/lp_clkrst_reg.h>
#include <soc/lp_system_reg.h>
#include <soc/lp_wdt_reg.h>
#include <soc/pmu_reg.h>
#include <soc/reg_base.h>
#include <soc/soc.h>

/* Match ESP-IDF pmu_param.h defaults used in rtc_clk_init(). */
#define HP_CALI_ACTIVE_DCM_VSET_DEFAULT 27
#define HP_CALI_ACTIVE_DBIAS_DEFAULT    24
#define LP_CALI_ACTIVE_DBIAS_DEFAULT    29

#define CPU_FREQ_MHZ 240

#define HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG (DR_REG_HP_SYS_CLKRST_BASE + 0x4)
#define HP_SYS_CLKRST_ROOT_CLK_CTRL1_REG (DR_REG_HP_SYS_CLKRST_BASE + 0x8)
#define HP_SYS_CLKRST_ROOT_CLK_CTRL2_REG (DR_REG_HP_SYS_CLKRST_BASE + 0xc)
#define HP_SYS_CLKRST_SOC_CLK_DIV_UPDATE  (BIT(4))

#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM    0x000000FFU
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM_M  (HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM_V << HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM_S)
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM_V  0x000000FFU
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM_S  5

#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR    0x000000FFU
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR_M  (HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR_V << HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR_S)
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR_V  0x000000FFU
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR_S  13

#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR    0x000000FFU
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR_M  (HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR_V << HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR_S)
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR_V  0x000000FFU
#define HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR_S  21

#define HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM    0x000000FFU
#define HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM_M  (HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM_V << HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM_S)
#define HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM_V  0x000000FFU
#define HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM_S  0

#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM    0x000000FFU
#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_M  (HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_V << HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_S)
#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_V  0x000000FFU
#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_S  16

extern uint32_t esp_rom_get_cpu_freq(void);
extern void esp_rom_set_cpu_ticks_per_us(uint32_t ticks_per_us);
extern void esp_rom_delay_us(uint32_t us);

static uint32_t s_cpu_freq = 0;

static unsigned stub_target_get_chip_revision(void)
{
    uint32_t reg = REG_READ(EFUSE_RD_REPEAT_DATA1_REG);
    unsigned major_lo = (reg >> EFUSE_WAFER_VERSION_MAJOR_LO_S) & EFUSE_WAFER_VERSION_MAJOR_LO_V;
    unsigned major_hi = (reg >> EFUSE_WAFER_VERSION_MAJOR_HI_S) & 1U;
    unsigned major = (major_hi << 2) | major_lo;
    unsigned minor = (reg >> EFUSE_WAFER_VERSION_MINOR_S) & EFUSE_WAFER_VERSION_MINOR_V;

    return major * 100U + minor;
}

static void stub_target_bus_update(void)
{
    SET_PERI_REG_MASK(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_SOC_CLK_DIV_UPDATE);
    while (READ_PERI_REG(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG) & HP_SYS_CLKRST_SOC_CLK_DIV_UPDATE) {
    }
}

static void stub_target_switch_to_dcdc(void)
{
    /*
     * DCDC switch sequence from ESP-IDF rtc_clk_init(), using fixed default
     * regulator targets (no PVT auto-dbias).
     */
    unsigned chip_version = stub_target_get_chip_revision();

    SET_PERI_REG_MASK(PMU_HP_ACTIVE_HP_REGULATOR0_REG, PMU_HP_ACTIVE_HP_REGULATOR_XPD);
    REG_SET_FIELD(PMU_HP_ACTIVE_HP_REGULATOR0_REG, PMU_HP_ACTIVE_HP_REGULATOR_DBIAS,
                  HP_CALI_ACTIVE_DBIAS_DEFAULT);
    REG_SET_FIELD(PMU_LP_ACTIVE_LP_REGULATOR0_REG, PMU_LP_ACTIVE_LP_REGULATOR_DBIAS,
                  LP_CALI_ACTIVE_DBIAS_DEFAULT);

    if (chip_version > 301U) {
        SET_PERI_REG_MASK(PMU_DCM_CTRL_REG, PMU_DCDC_FB_RES_FORCE_PD);
    }

    CLEAR_PERI_REG_MASK(PMU_DCM_CTRL_REG, PMU_DCDC_DONE_FORCE);
    SET_PERI_REG_MASK(PMU_DCM_CTRL_REG, PMU_DCDC_ON_REQ);
    CLEAR_PERI_REG_MASK(PMU_POWER_DCDC_SWITCH_REG, PMU_FORCE_DCDC_SWITCH_PU);
    CLEAR_PERI_REG_MASK(PMU_POWER_DCDC_SWITCH_REG, PMU_FORCE_DCDC_SWITCH_PD);
    REG_SET_FIELD(PMU_HP_ACTIVE_BIAS_REG, PMU_HP_ACTIVE_DCM_VSET, HP_CALI_ACTIVE_DCM_VSET_DEFAULT);
    SET_PERI_REG_MASK(PMU_HP_ACTIVE_HP_REGULATOR0_REG, PMU_DIG_REGULATOR0_DBIAS_SEL);
    esp_rom_delay_us(1000);

    if (chip_version > 301U) {
        REG_SET_FIELD(LP_SYSTEM_REG_SYS_CTRL_REG, LP_SYSTEM_REG_LP_FIB_SEL, 0xEF);
        CLEAR_PERI_REG_MASK(PMU_DCM_CTRL_REG, PMU_DCDC_FB_RES_FORCE_PD);
        esp_rom_delay_us(10);
    }

    CLEAR_PERI_REG_MASK(PMU_HP_ACTIVE_HP_REGULATOR0_REG, PMU_HP_ACTIVE_HP_REGULATOR_XPD);
}

static void stub_target_apply_cpu_240mhz(void)
{
    /*
     * On v3.x silicon the CPLL root is 400 MHz in download mode. Derive a real
     * 240 MHz CPU clock with a 3/5 divider and keep MEM/APB within IDF limits.
     */
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL2_REG, HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM, 1);
    stub_target_bus_update();
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM, 1);
    stub_target_bus_update();
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM, 0);
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR, 2);
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR, 3);
    stub_target_bus_update();
    REG_SET_FIELD(LP_CLKRST_HP_CLK_CTRL_REG, LP_CLKRST_HP_ROOT_CLK_SRC_SEL, 1);
}

void stub_target_clock_init(void)
{
    stub_target_switch_to_dcdc();

    s_cpu_freq = CPU_FREQ_MHZ * MHZ;
    esp_rom_set_cpu_ticks_per_us(CPU_FREQ_MHZ);

    stub_target_apply_cpu_240mhz();
}

uint32_t stub_target_get_cpu_freq(void)
{
    if (s_cpu_freq == 0) {
        return esp_rom_get_cpu_freq();
    }
    return s_cpu_freq;
}

#define LP_WDT_WDT_KEY 0x50D83AA1
#define LP_WDT_SWD_KEY 0x50D83AA1

void stub_target_clock_disable_watchdogs(void)
{
    // Disable RWDT (RTC Watchdog)
    REG_SET_BIT(LP_WDT_INT_CLR_REG, LP_WDT_LP_WDT_INT_CLR);
    WRITE_PERI_REG(LP_WDT_WPROTECT_REG, LP_WDT_WDT_KEY);
    WRITE_PERI_REG(LP_WDT_CONFIG0_REG, 0x0);
    WRITE_PERI_REG(LP_WDT_WPROTECT_REG, 0x0);

    // Configure SWD (Super Watchdog) to autofeed
    REG_SET_BIT(LP_WDT_INT_CLR_REG, LP_WDT_SUPER_WDT_INT_CLR);
    WRITE_PERI_REG(LP_WDT_SWD_WPROTECT_REG, LP_WDT_SWD_KEY);
    SET_PERI_REG_MASK(LP_WDT_SWD_CONFIG_REG, LP_WDT_SWD_AUTO_FEED_EN);
    WRITE_PERI_REG(LP_WDT_SWD_WPROTECT_REG, 0x0);
}

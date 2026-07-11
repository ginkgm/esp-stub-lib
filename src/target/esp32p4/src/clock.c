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

#define CPU_FREQ_MHZ   360
#define XTAL_FREQ_MHZ  40

#define HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG (DR_REG_HP_SYS_CLKRST_BASE + 0x4)
#define HP_SYS_CLKRST_ROOT_CLK_CTRL1_REG (DR_REG_HP_SYS_CLKRST_BASE + 0x8)
#define HP_SYS_CLKRST_ROOT_CLK_CTRL2_REG (DR_REG_HP_SYS_CLKRST_BASE + 0xc)
#define HP_SYS_CLKRST_ANA_PLL_CTRL0_REG  (DR_REG_HP_SYS_CLKRST_BASE + 0xbc)
#define HP_SYS_CLKRST_SOC_CLK_DIV_UPDATE (BIT(4))
#define HP_SYS_CLKRST_REG_CPU_PLL_CAL_END  (BIT(2))
#define HP_SYS_CLKRST_REG_CPU_PLL_CAL_STOP (BIT(3))

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

#define HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM    0x000000FFU
#define HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM_M  (HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM_V << HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM_S)
#define HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM_V  0x000000FFU
#define HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM_S  24

#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM    0x000000FFU
#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_M  (HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_V << HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_S)
#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_V  0x000000FFU
#define HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM_S  16

/* Analog I2C master used to program CPLL (from IDF regi2c_impl / regi2c_cpll). */
#define DR_REG_LP_I2C_ANA_MST_BASE           DR_REG_I2C_ANA_MST_BASE
#define LPPERI_CLK_EN_REG                    (DR_REG_LPPERI_BASE + 0x0)
#define LPPERI_CK_EN_LP_I2CMST               (BIT(27))
#define LP_I2C_ANA_MST_I2C0_CTRL_REG         (DR_REG_LP_I2C_ANA_MST_BASE + 0x0)
#define LP_I2C_ANA_MST_ANA_CONF1_REG         (DR_REG_LP_I2C_ANA_MST_BASE + 0x1c)
#define LP_I2C_ANA_MST_ANA_CONF2_REG         (DR_REG_LP_I2C_ANA_MST_BASE + 0x20)
#define LP_I2C_ANA_MST_CLK160M_REG           (DR_REG_LP_I2C_ANA_MST_BASE + 0x34)
#define LP_I2C_ANA_MST_ANA_CONF1             0x00FFFFFFU
#define LP_I2C_ANA_MST_ANA_CONF1_M           (LP_I2C_ANA_MST_ANA_CONF1_V << LP_I2C_ANA_MST_ANA_CONF1_S)
#define LP_I2C_ANA_MST_ANA_CONF1_V           0x00FFFFFFU
#define LP_I2C_ANA_MST_ANA_CONF1_S           0
#define LP_I2C_ANA_MST_ANA_CONF2             0x00FFFFFFU
#define LP_I2C_ANA_MST_ANA_CONF2_M           (LP_I2C_ANA_MST_ANA_CONF2_V << LP_I2C_ANA_MST_ANA_CONF2_S)
#define LP_I2C_ANA_MST_ANA_CONF2_V           0x00FFFFFFU
#define LP_I2C_ANA_MST_ANA_CONF2_S           0
#define LP_I2C_ANA_MST_CLK_I2C_MST_SEL_160M  (BIT(0))
#define REGI2C_PLL_CPU_MST_SEL               (BIT(11))
#define REGI2C_RTC_BUSY                      (BIT(25))
#define REGI2C_RTC_WR_CNTL_S                 24
#define REGI2C_RTC_DATA_S                    16
#define REGI2C_RTC_ADDR_S                    8
#define REGI2C_RTC_SLAVE_ID_S                0

#define I2C_CPLL                0x67
#define I2C_CPLL_OC_REF_DIV     2
#define I2C_CPLL_OC_DIV_7_0     3
#define I2C_CPLL_OC_DCUR        6
#define I2C_CPLL_OC_ENB_FCAL_LSB 7
#define I2C_CPLL_OC_DCHGP_LSB   4
#define I2C_CPLL_OC_DHREF_SEL_LSB 4
#define I2C_CPLL_OC_DLREF_SEL_LSB 6

#define HP_ROOT_CLK_SRC_XTAL  0
#define HP_ROOT_CLK_SRC_CPLL  1

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

static void stub_target_cpu_set_src(unsigned src)
{
    REG_SET_FIELD(LP_CLKRST_HP_CLK_CTRL_REG, LP_CLKRST_HP_ROOT_CLK_SRC_SEL, src);
}

static void stub_target_cpu_set_divider(uint32_t integer, uint32_t numerator, uint32_t denominator)
{
    /* Hardware field stores (integer - 1), matching clk_ll_cpu_set_divider(). */
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUM, integer - 1U);
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_CLK_DIV_NUMERATOR, numerator);
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_CLK_DIV_DENOMINATOR, denominator);
}

static void stub_target_mem_set_divider(uint32_t divider)
{
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_MEM_CLK_DIV_NUM, divider - 1U);
}

static void stub_target_sys_set_divider(uint32_t divider)
{
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_SYS_CLK_DIV_NUM, divider - 1U);
}

static void stub_target_apb_set_divider(uint32_t divider)
{
    REG_SET_FIELD(HP_SYS_CLKRST_ROOT_CLK_CTRL2_REG, HP_SYS_CLKRST_REG_APB_CLK_DIV_NUM, divider - 1U);
}

static void stub_target_regi2c_enable_clock(void)
{
    SET_PERI_REG_MASK(LPPERI_CLK_EN_REG, LPPERI_CK_EN_LP_I2CMST);
    SET_PERI_REG_MASK(LP_I2C_ANA_MST_CLK160M_REG, LP_I2C_ANA_MST_CLK_I2C_MST_SEL_160M);
}

static void stub_target_regi2c_write(uint8_t block, uint8_t reg_add, uint8_t data)
{
    REG_SET_FIELD(LP_I2C_ANA_MST_ANA_CONF2_REG, LP_I2C_ANA_MST_ANA_CONF2, 0);
    REG_SET_FIELD(LP_I2C_ANA_MST_ANA_CONF1_REG, LP_I2C_ANA_MST_ANA_CONF1, 0);
    SET_PERI_REG_MASK(LP_I2C_ANA_MST_ANA_CONF2_REG, REGI2C_PLL_CPU_MST_SEL);

    while (REG_GET_BIT(LP_I2C_ANA_MST_I2C0_CTRL_REG, REGI2C_RTC_BUSY)) {
    }
    uint32_t temp = ((uint32_t)block << REGI2C_RTC_SLAVE_ID_S)
                    | ((uint32_t)reg_add << REGI2C_RTC_ADDR_S)
                    | (1U << REGI2C_RTC_WR_CNTL_S)
                    | ((uint32_t)data << REGI2C_RTC_DATA_S);
    REG_WRITE(LP_I2C_ANA_MST_I2C0_CTRL_REG, temp);
    while (REG_GET_BIT(LP_I2C_ANA_MST_I2C0_CTRL_REG, REGI2C_RTC_BUSY)) {
    }
}

static void stub_target_cpll_enable(void)
{
    SET_PERI_REG_MASK(PMU_IMM_HP_CK_POWER_REG, PMU_TIE_HIGH_XPD_CPLL | PMU_TIE_HIGH_XPD_CPLL_I2C);
    SET_PERI_REG_MASK(PMU_IMM_HP_CK_POWER_REG, PMU_TIE_HIGH_GLOBAL_CPLL_ICG);
}

static void stub_target_cpll_configure(unsigned chip_version)
{
    /*
     * Analog CPLL programming from clk_ll_cpll_set_config() /
     * rtc_clk_cpll_configure(). Always target 360 MHz from 40 MHz XTAL,
     * for both pre-ECO1 and later silicon (different OC_DIV encoding).
     */
    uint8_t div_ref = 0;
    uint8_t div7_0;
    uint8_t dchgp = 5;
    uint8_t dcur = 3;
    uint8_t oc_enb_fcal = 0;

    if (chip_version < 1U) {
        div7_0 = 5; /* ECO0: 360 MHz */
    } else {
        div7_0 = 9; /* ECO1+: 360 MHz */
    }

    uint8_t i2c_cpll_lref = (uint8_t)((oc_enb_fcal << I2C_CPLL_OC_ENB_FCAL_LSB)
                                      | (dchgp << I2C_CPLL_OC_DCHGP_LSB)
                                      | div_ref);
    uint8_t i2c_cpll_dcur = (uint8_t)((1U << I2C_CPLL_OC_DLREF_SEL_LSB)
                                      | (3U << I2C_CPLL_OC_DHREF_SEL_LSB)
                                      | dcur);

    stub_target_regi2c_enable_clock();
    CLEAR_PERI_REG_MASK(HP_SYS_CLKRST_ANA_PLL_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_PLL_CAL_STOP);
    stub_target_regi2c_write(I2C_CPLL, I2C_CPLL_OC_REF_DIV, i2c_cpll_lref);
    stub_target_regi2c_write(I2C_CPLL, I2C_CPLL_OC_DIV_7_0, div7_0);
    stub_target_regi2c_write(I2C_CPLL, I2C_CPLL_OC_DCUR, i2c_cpll_dcur);
    while ((READ_PERI_REG(HP_SYS_CLKRST_ANA_PLL_CTRL0_REG) & HP_SYS_CLKRST_REG_CPU_PLL_CAL_END) == 0) {
    }
    esp_rom_delay_us(10);
    SET_PERI_REG_MASK(HP_SYS_CLKRST_ANA_PLL_CTRL0_REG, HP_SYS_CLKRST_REG_CPU_PLL_CAL_STOP);
}

static void stub_target_cpu_freq_to_xtal(void)
{
    /*
     * rtc_clk_cpu_freq_to_xtal(xtal, 1, false): switch source first, then
     * CPU/MEM/SYS/APB dividers to 1 (40-40-40-40).
     */
    stub_target_cpu_set_src(HP_ROOT_CLK_SRC_XTAL);
    stub_target_cpu_set_divider(1, 0, 0);
    stub_target_mem_set_divider(1);
    stub_target_sys_set_divider(1);
    stub_target_apb_set_divider(1);
    stub_target_bus_update();
    esp_rom_set_cpu_ticks_per_us(XTAL_FREQ_MHZ);
}

static void stub_target_cpu_freq_to_cpll_360mhz(void)
{
    /*
     * IDF rtc_clk_cpu_freq_to_cpll_mhz(360):
     * CPLL 360 /1 -> CPU 360, MEM /2 -> 180, SYS /1 -> 180, APB /2 -> 90.
     * Upscale path: APB -> SYS -> MEM -> CPU, then source select last.
     */
    const uint32_t mem_divider = 2;
    const uint32_t sys_divider = 1;
    const uint32_t apb_divider = 2;

    stub_target_apb_set_divider(apb_divider);
    stub_target_bus_update();
    stub_target_sys_set_divider(sys_divider);
    stub_target_bus_update();
    stub_target_mem_set_divider(mem_divider);
    stub_target_bus_update();
    stub_target_cpu_set_divider(1, 0, 0);
    stub_target_bus_update();
    stub_target_cpu_set_src(HP_ROOT_CLK_SRC_CPLL);
    esp_rom_set_cpu_ticks_per_us(CPU_FREQ_MHZ);
}

static void stub_target_switch_to_dcdc(void)
{
    /*
     * DCDC switch sequence from ESP-IDF rtc_clk_init(), including ECO6+
     * FIB handoff and HP LDO XPD off (rtc_clk_init.c:81-88).
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

static void stub_target_apply_cpu_360mhz(void)
{
    /*
     * Match rtc_clk_cpu_freq_set_config() for CPLL@360 / CPU@360:
     * switch to XTAL, (re)configure CPLL to 360, then raise CPU tree.
     */
    unsigned chip_version = stub_target_get_chip_revision();

    stub_target_cpu_freq_to_xtal();
    stub_target_cpll_enable();
    stub_target_cpll_configure(chip_version);
    stub_target_cpu_freq_to_cpll_360mhz();
}

void stub_target_clock_init(void)
{
    stub_target_switch_to_dcdc();

    s_cpu_freq = CPU_FREQ_MHZ * MHZ;
    stub_target_apply_cpu_360mhz();
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

//
// Copyright 2011-2012 Ettus Research LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//


#include "adf4351_regs.hpp"
#include "db_sbx_common.hpp"


using namespace uhd;
using namespace uhd::usrp;
using namespace boost::assign;

/***********************************************************************
 * Structors
 **********************************************************************/
sbx_xcvr::sbx_version4::sbx_version4(sbx_xcvr *_self_sbx_xcvr) {
    //register the handle to our base SBX class
    self_base = _self_sbx_xcvr;
}


sbx_xcvr::sbx_version4::~sbx_version4(void){
    /* NOP */
}


/***********************************************************************
 * Tuning
 **********************************************************************/
double sbx_xcvr::sbx_version4::set_lo_freq(dboard_iface::unit_t unit, double target_freq) {
    UHD_LOGV(often) << boost::format(
        "SBX tune: target frequency %f Mhz"
    ) % (target_freq/1e6) << std::endl;

    //clip the input
    target_freq = sbx_freq_range.clip(target_freq);

    //map prescaler setting to mininmum integer divider (N) values (pg.18 prescaler)
    static const uhd::dict<int, int> prescaler_to_min_int_div = map_list_of
        (0,23) //adf4351_regs_t::PRESCALER_4_5
        (1,75) //adf4351_regs_t::PRESCALER_8_9
    ;

    //map rf divider select output dividers to enums
    static const uhd::dict<int, adf4351_regs_t::rf_divider_select_t> rfdivsel_to_enum = map_list_of
        (1,  adf4351_regs_t::RF_DIVIDER_SELECT_DIV1)
        (2,  adf4351_regs_t::RF_DIVIDER_SELECT_DIV2)
        (4,  adf4351_regs_t::RF_DIVIDER_SELECT_DIV4)
        (8,  adf4351_regs_t::RF_DIVIDER_SELECT_DIV8)
        (16, adf4351_regs_t::RF_DIVIDER_SELECT_DIV16)
        (32, adf4351_regs_t::RF_DIVIDER_SELECT_DIV32)
        (64, adf4351_regs_t::RF_DIVIDER_SELECT_DIV64)
    ;

    //use 8/9 prescaler for vco_freq > 3 GHz (pg.18 prescaler)
    adf4351_regs_t::prescaler_t prescaler = target_freq > 3.6e9 ? adf4351_regs_t::PRESCALER_8_9 : adf4351_regs_t::PRESCALER_4_5;

    adf435x_tuning_constraints tuning_constraints;
    tuning_constraints.force_frac0 = false;
    tuning_constraints.band_sel_freq_max = 100e3;
    tuning_constraints.ref_doubler_threshold = 12.5e6;
    tuning_constraints.int_range = uhd::range_t(prescaler_to_min_int_div[prescaler], 4095);  //INT is a 12-bit field
    tuning_constraints.pfd_freq_max = 25e6;
    tuning_constraints.rf_divider_range = uhd::range_t(1, 64);

    double actual_freq;
    adf435x_tuning_settings tuning_settings = _tune_adf435x_synth(
        target_freq, self_base->get_iface()->get_clock_rate(unit),
        tuning_constraints, actual_freq);

    //load the register values
    adf4351_regs_t regs;

    if ((unit == dboard_iface::UNIT_TX) and (actual_freq == sbx_tx_lo_2dbm.clip(actual_freq))) 
        regs.output_power = adf4351_regs_t::OUTPUT_POWER_2DBM;
    else
        regs.output_power = adf4351_regs_t::OUTPUT_POWER_5DBM;

    regs.frac_12_bit            = tuning_settings.frac_12_bit;
    regs.int_16_bit             = tuning_settings.int_16_bit;
    regs.mod_12_bit             = tuning_settings.mod_12_bit;
    regs.clock_divider_12_bit   = tuning_settings.clock_divider_12_bit;
    regs.feedback_select        = tuning_settings.feedback_after_divider ?
                                    adf4351_regs_t::FEEDBACK_SELECT_DIVIDED :
                                    adf4351_regs_t::FEEDBACK_SELECT_FUNDAMENTAL;
    regs.clock_div_mode         = adf4351_regs_t::CLOCK_DIV_MODE_RESYNC_ENABLE;
    regs.prescaler              = prescaler;
    regs.r_counter_10_bit       = tuning_settings.r_counter_10_bit;
    regs.reference_divide_by_2  = tuning_settings.r_divide_by_2_en ?
                                    adf4351_regs_t::REFERENCE_DIVIDE_BY_2_ENABLED :
                                    adf4351_regs_t::REFERENCE_DIVIDE_BY_2_DISABLED;
    regs.reference_doubler      = tuning_settings.r_doubler_en ?
                                    adf4351_regs_t::REFERENCE_DOUBLER_ENABLED :
                                    adf4351_regs_t::REFERENCE_DOUBLER_DISABLED;
    regs.band_select_clock_div  = tuning_settings.band_select_clock_div;
    UHD_ASSERT_THROW(rfdivsel_to_enum.has_key(tuning_settings.rf_divider));
    regs.rf_divider_select      = rfdivsel_to_enum[tuning_settings.rf_divider];

    //reset the N and R counter
    regs.counter_reset = adf4351_regs_t::COUNTER_RESET_ENABLED;
    self_base->get_iface()->write_spi(unit, spi_config_t::EDGE_RISE, regs.get_reg(2), 32);
    regs.counter_reset = adf4351_regs_t::COUNTER_RESET_DISABLED;

    //write the registers
    //correct power-up sequence to write registers (5, 4, 3, 2, 1, 0)
    int addr;

    for(addr=5; addr>=0; addr--){
        UHD_LOGV(often) << boost::format(
            "SBX SPI Reg (0x%02x): 0x%08x"
        ) % addr % regs.get_reg(addr) << std::endl;
        self_base->get_iface()->write_spi(
            unit, spi_config_t::EDGE_RISE,
            regs.get_reg(addr), 32
        );
    }

    //return the actual frequency
    UHD_LOGV(often) << boost::format(
        "SBX tune: actual frequency %f Mhz"
    ) % (actual_freq/1e6) << std::endl;
    return actual_freq;
}


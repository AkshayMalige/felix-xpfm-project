#/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*/

proc numberOfCPUs {} {
    # Windows puts it in an environment variable
    global tcl_platform env
    if {$tcl_platform(platform) eq "windows"} {
        return $env(NUMBER_OF_PROCESSORS)
    }

    # Check for sysctl (OSX, BSD)
    set sysctl [auto_execok "sysctl"]
    if {[llength $sysctl]} {
        if {![catch {exec {*}$sysctl -n "hw.ncpu"} cores]} {
            return $cores
        }
    }

    # Assume Linux, which has /proc/cpuinfo, but be careful
    if {![catch {open "/proc/cpuinfo"} f]} {
        set cores [regexp -all -line {^processor\s} [read $f]]
        close $f
        if {$cores > 0} {
            return $cores
        }
    }

    # No idea what the actual number of cores is; exhausted all our options
    # Fall back to returning 1; there must be at least that because we're running on it!
    return 1
}
if {[file exists local_user_repo]} {
               file delete -force local_user_repo
               }

#unset ::env(HTTP_PROXY)
file mkdir local_user_repo

# The environment variable XILINX_XHUB_USERAREA_ROOT_DIR is responsible for redirecting downloaded board files to local_user_repo
set ::env(XILINX_XHUB_USERAREA_ROOT_DIR) local_user_repo

xhub::get_xstores




xhub::refresh_catalog [xhub::get_xstores Vivado_example_project]
get_property LOCAL_ROOT_DIR [xhub::get_xstores Vivado_example_project]
set_param ced.repoPaths [get_property LOCAL_ROOT_DIR [xhub::get_xstores Vivado_example_project]]
xhub::install [xhub::get_xitems *ext_platform_part*]



# ------ Create Vivado Project ------
create_project project_1 ./project_1 -part xcvp1552-vsva3340-2MHP-e-S
create_bd_design "ext_platform_part" -mode batch
instantiate_example_design -template xilinx.com:design:ext_platform_part:1.0 -design ext_platform_part -options { Include_DDR.VALUE true}
# Other options are default
# - Three clocks
# - 32 interrupt

# ------ Remove LPDDR4 (not present on FLX-155 board) ------
delete_bd_objs [get_bd_intf_nets noc_lpddr4_CH0_LPDDR4_0]
delete_bd_objs [get_bd_intf_nets noc_lpddr4_CH1_LPDDR4_0]
delete_bd_objs [get_bd_intf_nets lpddr4_sma_clk2_1]
delete_bd_objs [get_bd_intf_nets cips_noc_M01_INI]
delete_bd_objs [get_bd_cells noc_lpddr4]
delete_bd_objs [get_bd_intf_ports CH0_LPDDR4_0_0]
delete_bd_objs [get_bd_intf_ports CH1_LPDDR4_0_0]
delete_bd_objs [get_bd_intf_ports sys_clk0_1]
set_property CONFIG.NUM_NMI {1} [get_bd_cells cips_noc]




set_property -dict [list CONFIG.PS_PMC_CONFIG { BOOT_MODE Custom  CLOCK_MODE Custom  DDR_MEMORY_MODE Custom  DESIGN_MODE 1  IO_CONFIG_MODE Custom  PMC_CRP_PL0_REF_CTRL_FREQMHZ 99.999992  PMC_QSPI_FBCLK {{ENABLE 1} {IO {PMC_MIO 6}}}  PMC_QSPI_PERIPHERAL_ENABLE 0  PMC_SD0 {{CD_ENABLE 0} {CD_IO {PMC_MIO 24}} {POW_ENABLE 0} {POW_IO {PMC_MIO 17}} {RESET_ENABLE 0} {RESET_IO {PMC_MIO 17}} {WP_ENABLE 0} {WP_IO {PMC_MIO 25}}}  PMC_SD0_PERIPHERAL {{CLK_100_SDR_OTAP_DLY 0x00} {CLK_200_SDR_OTAP_DLY 0x00} {CLK_50_DDR_ITAP_DLY 0x00} {CLK_50_DDR_OTAP_DLY 0x00} {CLK_50_SDR_ITAP_DLY 0x00} {CLK_50_SDR_OTAP_DLY 0x00} {ENABLE 0} {IO {PMC_MIO 13 .. 25}}}  PMC_SD0_SLOT_TYPE {SD 2.0}  PMC_SD1_PERIPHERAL {{CLK_100_SDR_OTAP_DLY 0x3} {CLK_200_SDR_OTAP_DLY 0x2} {CLK_50_DDR_ITAP_DLY 0x36} {CLK_50_DDR_OTAP_DLY 0x3} {CLK_50_SDR_ITAP_DLY 0x2C} {CLK_50_SDR_OTAP_DLY 0x4} {ENABLE 1} {IO {PMC_MIO 26 .. 36}}}  PMC_SD1_SLOT_TYPE {SD 3.0}  PMC_SMAP_PERIPHERAL {{ENABLE 0} {IO {32 Bit}}}  PMC_USE_PMC_NOC_AXI0 1  PMC_USE_PMC_NOC_AXI1 1  PS_BOARD_INTERFACE Custom  PS_ENET0_MDIO {{ENABLE 1} {IO {PS_MIO 24 .. 25}}}  PS_ENET0_PERIPHERAL {{ENABLE 1} {IO {PS_MIO 0 .. 11}}}  PS_GEN_IPI0_ENABLE 1  PS_GEN_IPI0_MASTER A72  PS_GEN_IPI1_ENABLE 1  PS_GEN_IPI1_MASTER A72  PS_GEN_IPI2_ENABLE 1  PS_GEN_IPI2_MASTER A72  PS_GEN_IPI3_ENABLE 1  PS_GEN_IPI3_MASTER A72  PS_GEN_IPI4_ENABLE 1  PS_GEN_IPI4_MASTER A72  PS_GEN_IPI5_ENABLE 1  PS_GEN_IPI5_MASTER A72  PS_GEN_IPI6_ENABLE 1  PS_GEN_IPI6_MASTER A72  PS_IRQ_USAGE {{CH0 1} {CH1 0} {CH10 0} {CH11 0} {CH12 0} {CH13 0} {CH14 0} {CH15 0} {CH2 0} {CH3 0} {CH4 0} {CH5 0} {CH6 0} {CH7 0} {CH8 0} {CH9 0}}  PS_NUM_FABRIC_RESETS 1  PS_PL_CONNECTIVITY_MODE Custom  PS_TTC0_PERIPHERAL_ENABLE 1  PS_UART0_PERIPHERAL {{ENABLE 1} {IO {PMC_MIO 42 .. 43}}}  PS_USE_FPD_AXI_NOC0 1  PS_USE_FPD_AXI_NOC1 1  PS_USE_FPD_CCI_NOC 1  PS_USE_M_AXI_FPD 1  PS_USE_NOC_LPD_AXI0 1  PS_USE_PMCPL_CLK0 1  PS_USE_PMCPL_CLK1 0  PS_USE_PMCPL_CLK2 0  PS_USE_PMCPL_CLK3 0  SLR1_PMC_CRP_HSM0_REF_CTRL_FREQMHZ 100  SMON_ALARMS Set_Alarms_On  SMON_ENABLE_TEMP_AVERAGING 0  SMON_TEMP_AVERAGING_SAMPLES 0 } CONFIG.PS_PMC_CONFIG_APPLIED {1} CONFIG.IO_CONFIG_MODE {Custom}] [get_bd_cells CIPS_0]

set_property -dict [list \
  CONFIG.MC0_CONFIG_NUM {config17} \
  CONFIG.MC_CHAN_REGION1 {DDR_LOW1} \
  CONFIG.MC_DATAWIDTH {72} \
  CONFIG.MC_EN_INTR_RESP {TRUE} \
  CONFIG.MC_INPUTCLK0_PERIOD {5000} \
  CONFIG.MC_MEMORY_DEVICETYPE {UDIMMs} \
  CONFIG.MC_MEMORY_SPEEDGRADE {DDR4-2666V(19-19-19)} \
  CONFIG.MC_MEMORY_TIMEPERIOD0 {800} \
  CONFIG.MC_RANK {2} \
  CONFIG.MC_ROWADDRESSWIDTH {17} \
] [get_bd_cells noc_ddr4]


set_property -dict [list CONFIG.FREQ_HZ {200000000}] [get_bd_intf_ports sys_clk0_0]


# ------ Add multiple platform clocks for Vitis region ------
# Configure clk_wizard_0 for 3 output clocks: 156 / 300 / 500 MHz
startgroup
set_property -dict [list \
  CONFIG.CLKOUT_DRIVES {BUFG,BUFG,BUFG,BUFG,BUFG,BUFG,BUFG} \
  CONFIG.CLKOUT_DYN_PS {None,None,None,None,None,None,None} \
  CONFIG.CLKOUT_GROUPING {Auto,Auto,Auto,Auto,Auto,Auto,Auto} \
  CONFIG.CLKOUT_MATCHED_ROUTING {false,false,false,false,false,false,false} \
  CONFIG.CLKOUT_PORT {clk_out1,clk_out2,clk_out3,clk_out4,clk_out5,clk_out6,clk_out7} \
  CONFIG.CLKOUT_REQUESTED_DUTY_CYCLE {50.000,50.000,50.000,50.000,50.000,50.000,50.000} \
  CONFIG.CLKOUT_REQUESTED_OUT_FREQUENCY {156,300,500,100.000,100.000,100.000,100.000} \
  CONFIG.CLKOUT_REQUESTED_PHASE {0.000,0.000,0.000,0.000,0.000,0.000,0.000} \
  CONFIG.CLKOUT_USED {true,true,true,false,false,false,false} \
] [get_bd_cells clk_wizard_0]
endgroup

# Clone proc_sys_reset_0 to create reset blocks for the new clocks
copy_bd_objs /  [get_bd_cells {proc_sys_reset_0}]
connect_bd_net [get_bd_pins clk_wizard_0/clk_out2] [get_bd_pins proc_sys_reset_1/slowest_sync_clk]
copy_bd_objs /  [get_bd_cells {proc_sys_reset_1}]
connect_bd_net [get_bd_pins clk_wizard_0/clk_out3] [get_bd_pins proc_sys_reset_2/slowest_sync_clk]

# Let Vivado auto-wire the reset connections
startgroup
apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_1/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_2/ext_reset_in]
endgroup

# Register all 3 clocks as platform clocks for Vitis
#   clk_out1 (id=0, ~156 MHz) - original
#   clk_out2 (id=2, ~300 MHz) - default kernel clock
#   clk_out3 (id=3, ~500 MHz)
set_property PFM.CLOCK {clk_out1 {id "0" is_default "false" proc_sys_reset "/proc_sys_reset_0" status "fixed"} clk_out2 {id "2" is_default "true" proc_sys_reset "/proc_sys_reset_1" status "fixed"} clk_out3 {id "3" is_default "false" proc_sys_reset "/proc_sys_reset_2" status "fixed"}} [get_bd_cells /clk_wizard_0]


add_files -fileset constrs_1 -norecurse ../../pinout.xdc
import_files -fileset constrs_1 ../../pinout.xdc

remove_files ./project_1/project_1.srcs/sources_1/imports/hdl/ext_platform_part_wrapper.v
file delete -force ./project_1/project_1.srcs/sources_1/imports/hdl/ext_platform_part_wrapper.v
update_compile_order -fileset sources_1
assign_bd_address
validate_bd_design
make_wrapper -files [get_files ./project_1/project_1.srcs/sources_1/bd/ext_platform_part/ext_platform_part.bd] -top
add_files -norecurse ./project_1/project_1.gen/sources_1/bd/ext_platform_part/hdl/ext_platform_part_wrapper.v

generate_target all [get_files ext_platform_part.bd]
update_compile_order -fileset sources_1
write_hw_platform -force ./custom_hardware_platform_hw.xsa
write_hw_platform -hw_emu -force ./custom_hardware_platform_hwemu.xsa



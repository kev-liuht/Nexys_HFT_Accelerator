# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  set C_S00_AXIS_TDATA_WIDTH [ipgui::add_param $IPINST -name "C_S00_AXIS_TDATA_WIDTH" -parent ${Page_0} -widget comboBox]
  set_property tooltip {AXI4Stream sink: Data Width} ${C_S00_AXIS_TDATA_WIDTH}


}

proc update_PARAM_VALUE.NUM_SHARES_WIDTH { PARAM_VALUE.NUM_SHARES_WIDTH } {
	# Procedure called to update NUM_SHARES_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.NUM_SHARES_WIDTH { PARAM_VALUE.NUM_SHARES_WIDTH } {
	# Procedure called to validate NUM_SHARES_WIDTH
	return true
}

proc update_PARAM_VALUE.ORDER_REF_NUM_WIDTH { PARAM_VALUE.ORDER_REF_NUM_WIDTH } {
	# Procedure called to update ORDER_REF_NUM_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.ORDER_REF_NUM_WIDTH { PARAM_VALUE.ORDER_REF_NUM_WIDTH } {
	# Procedure called to validate ORDER_REF_NUM_WIDTH
	return true
}

proc update_PARAM_VALUE.ORDER_TYPE_WIDTH { PARAM_VALUE.ORDER_TYPE_WIDTH } {
	# Procedure called to update ORDER_TYPE_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.ORDER_TYPE_WIDTH { PARAM_VALUE.ORDER_TYPE_WIDTH } {
	# Procedure called to validate ORDER_TYPE_WIDTH
	return true
}

proc update_PARAM_VALUE.PRICE_WIDTH { PARAM_VALUE.PRICE_WIDTH } {
	# Procedure called to update PRICE_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.PRICE_WIDTH { PARAM_VALUE.PRICE_WIDTH } {
	# Procedure called to validate PRICE_WIDTH
	return true
}

proc update_PARAM_VALUE.STOCK_ID_WIDTH { PARAM_VALUE.STOCK_ID_WIDTH } {
	# Procedure called to update STOCK_ID_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.STOCK_ID_WIDTH { PARAM_VALUE.STOCK_ID_WIDTH } {
	# Procedure called to validate STOCK_ID_WIDTH
	return true
}

proc update_PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH { PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH } {
	# Procedure called to update C_S00_AXIS_TDATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH { PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH } {
	# Procedure called to validate C_S00_AXIS_TDATA_WIDTH
	return true
}


proc update_MODELPARAM_VALUE.C_S00_AXIS_TDATA_WIDTH { MODELPARAM_VALUE.C_S00_AXIS_TDATA_WIDTH PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH}] ${MODELPARAM_VALUE.C_S00_AXIS_TDATA_WIDTH}
}

proc update_MODELPARAM_VALUE.STOCK_ID_WIDTH { MODELPARAM_VALUE.STOCK_ID_WIDTH PARAM_VALUE.STOCK_ID_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.STOCK_ID_WIDTH}] ${MODELPARAM_VALUE.STOCK_ID_WIDTH}
}

proc update_MODELPARAM_VALUE.ORDER_REF_NUM_WIDTH { MODELPARAM_VALUE.ORDER_REF_NUM_WIDTH PARAM_VALUE.ORDER_REF_NUM_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.ORDER_REF_NUM_WIDTH}] ${MODELPARAM_VALUE.ORDER_REF_NUM_WIDTH}
}

proc update_MODELPARAM_VALUE.NUM_SHARES_WIDTH { MODELPARAM_VALUE.NUM_SHARES_WIDTH PARAM_VALUE.NUM_SHARES_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.NUM_SHARES_WIDTH}] ${MODELPARAM_VALUE.NUM_SHARES_WIDTH}
}

proc update_MODELPARAM_VALUE.PRICE_WIDTH { MODELPARAM_VALUE.PRICE_WIDTH PARAM_VALUE.PRICE_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.PRICE_WIDTH}] ${MODELPARAM_VALUE.PRICE_WIDTH}
}

proc update_MODELPARAM_VALUE.ORDER_TYPE_WIDTH { MODELPARAM_VALUE.ORDER_TYPE_WIDTH PARAM_VALUE.ORDER_TYPE_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.ORDER_TYPE_WIDTH}] ${MODELPARAM_VALUE.ORDER_TYPE_WIDTH}
}


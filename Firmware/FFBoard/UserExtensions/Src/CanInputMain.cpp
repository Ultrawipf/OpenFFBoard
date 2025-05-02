/*
 * CanInputMain.cpp
 *
 *  Created on: Apr 28, 2025
 *      Author: Yannick
 */


#include "CanInputMain.h"
#include "usb_hid_ffb_desc.h"
#ifdef CANINPUTMAIN
// Unique identifier for listing
ClassIdentifier CANInputMain::info = {
		 .name = "CAN remote Digital/Analog" ,
		 .id=CLSID_MAIN_CANINPUT,
 };

const ClassIdentifier CANInputMain::getInfo(){
	return info;
}

extern CANPort& canport; // Must be defined in target constants
CANInputMain::CANInputMain() : CANInputMain(canport)
{

}
CANInputMain::CANInputMain(CANPort& can):
	SelectableInputs(ButtonSource::all_buttonsources,AnalogSource::all_analogsources),
	Thread("FFBMAIN", 256, 30),
	can(can)
{
	this->restoreFlash();
	this->registerCommands();
	analogBuffer.reserve(8);
	this->Start();
}

CANInputMain::~CANInputMain() {

}



void CANInputMain::usbInit(){
	this->usbdev = std::make_unique<USBdevice>(&usb_devdesc_ffboard_composite,usb_cdc_conf,&usb_ffboard_strings_default);
	usbdev->registerUsb();
}



/**
 * Read parameters from flash and restore settings
 */
void CANInputMain::restoreFlash(){

	Flash_Read(ADR_FFBWHEEL_BUTTONCONF, &this->btnsources);
	setBtnTypes(this->btnsources);

	Flash_Read(ADR_FFBWHEEL_ANALOGCONF, &this->ainsources);
	setAinTypes(this->ainsources);

	uint16_t conf1;
	if(Flash_Read(ADR_CANREMOTE_CONF1,&conf1)){
		buttons_id = conf1 & 0x7ff; // 11b
		uint8_t rateidx = (conf1 >> 11) & 0x7;
		setReportRate(rateidx);
	}
	if(Flash_Read(ADR_CANREMOTE_CONF2,&conf1)){
		analog_id = conf1 & 0x7ff; // 11b
	}

}
/**
 * Save parameters to flash
 */
void CANInputMain::saveFlash(){

	Flash_Write(ADR_FFBWHEEL_BUTTONCONF,this->btnsources);
	Flash_Write(ADR_FFBWHEEL_ANALOGCONF,this->ainsources);

	uint16_t conf;
	conf = buttons_id & 0x7ff;
	conf |= (rate_idx & 0x7) << 11;
	Flash_Write(ADR_CANREMOTE_CONF1,conf);
	conf = analog_id & 0x7ff;
	Flash_Write(ADR_CANREMOTE_CONF2,conf);
}



void CANInputMain::Run(){

	while(true){
		Delay(1);
		updateControl();
	}
}


/**
 * Main update loop
 */
void CANInputMain::updateControl(){
	if(++report_rate_cnt >= report_rate){
		report_rate_cnt = 0;

		sendReport();
	}
}

void CANInputMain::sendReport(){
	uint64_t prevDigital = digitalBuffer;
	digitalBuffer = 0; // Clear buffer
	SelectableInputs::getButtonValues(digitalBuffer);
	if(prevDigital != digitalBuffer){ // Only send when data changed
		CAN_tx_msg msg;
		msg.header.id = buttons_id;
		msg.header.length = 8; // 64 buttons
		for(uint8_t i = 0;i<8;i++){
			msg.data[i] = ((digitalBuffer >> (i*8)) & 0xff);
		}
		can.sendMessage(msg);
	}

	std::vector<int32_t>* analogValues = SelectableInputs::getAnalogValues();
	if(analogValues->size() != analogBuffer.size()){
		analogBuffer.assign(analogValues->size(), 0); // Initialize vector if size changes
	}

	CAN_tx_msg msg;
	const uint8_t valuesPerMsg = 4;
	for(uint8_t i = 0;i<analogValues->size();i+=valuesPerMsg){
		bool changed = false;
		msg.header.id = analog_id + (i/valuesPerMsg);
		msg.header.length = 8;
		for(uint8_t v = 0;v<valuesPerMsg;v++){
			uint8_t idx = i+v;
			if(idx >=  analogValues->size()){
				break;
			}
			int32_t val = analogValues->at(idx);
			if(val != analogBuffer[idx]){
				changed = true;
			}
			msg.data[v*2] = val & 0xff;
			msg.data[(v*2) +1] = (val >> 8) & 0xff;
		}
		if(changed){
			can.sendMessage(msg);
			msg = CAN_tx_msg(); // Clear
		}
	}
	analogBuffer = *analogValues; // Copy values
}

/**
 * Changes the report rate based on the index for report_rates
 */
void CANInputMain::setReportRate(uint8_t rateidx){
	rateidx = clip<uint8_t,uint8_t>(rateidx, 0,report_rates.size());
	rate_idx = rateidx;
	report_rate = report_rates[rateidx];
}

/**
 * Generates the speed strings to display to the user
 */
std::string CANInputMain::report_rates_names() {
	std::string s = "";
	for(uint8_t i = 0 ; i < report_rates.size();i++){
		s += std::to_string(1000/(report_rates[i])) + "Hz:"+std::to_string(i);
		if(i < sizeof(report_rates)-1)
			s += ",";
	}
	return s;
}


void CANInputMain::registerCommands(){
	// CAN speed controlled by CAN port commands directly
	registerCommand("canidbtn", CANInput_commands::caniddigital, "Button output CAN ID",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("canidain", CANInput_commands::canidanalog, "Analog output start CAN ID",CMDFLAG_GET|CMDFLAG_SET);

	registerCommand("btntypes", CANInput_commands::btntypes, "Enabled button sources",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("addbtn", CANInput_commands::addbtn, "Enable button source",CMDFLAG_SET);
	registerCommand("lsbtn", CANInput_commands::lsbtn, "Get available button sources",CMDFLAG_GET|CMDFLAG_STR_ONLY);

	registerCommand("aintypes", CANInput_commands::aintypes, "Enabled analog sources",CMDFLAG_GET|CMDFLAG_SET);
	registerCommand("lsain", CANInput_commands::lsain, "Get available analog sources",CMDFLAG_GET|CMDFLAG_STR_ONLY);
	registerCommand("addain", CANInput_commands::addain, "Enable analog source",CMDFLAG_SET);

	registerCommand("rate", CANInput_commands::rate, "CAN interval rate",CMDFLAG_GET|CMDFLAG_SET|CMDFLAG_INFOSTRING);

	registerCommand("dvals", CANInput_commands::dvals, "Current digital outputs",CMDFLAG_GET);
	registerCommand("avals", CANInput_commands::avals, "Current analog outputs",CMDFLAG_GET);
}

CommandStatus CANInputMain::command(const ParsedCommand& cmd,std::vector<CommandReply>& replies){
	switch(static_cast<CANInput_commands>(cmd.cmdId)){
	case CANInput_commands::caniddigital:
		handleGetSet(cmd, replies, buttons_id);
		break;
	case CANInput_commands::canidanalog:
		handleGetSet(cmd, replies, analog_id);
		break;
	// Source management
	case CANInput_commands::btntypes:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(btnsources);
		}else if(cmd.type == CMDtype::set){
			setBtnTypes(cmd.val);
		}
		break;
	case CANInput_commands::lsbtn:
		btn_chooser.replyAvailableClasses(replies);
		break;
	case CANInput_commands::addbtn:
		if(cmd.type == CMDtype::set){
			this->addBtnType(cmd.val);
		}
		break;
	case CANInput_commands::aintypes:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(ainsources);
		}else if(cmd.type == CMDtype::set){
			setAinTypes(cmd.val);
		}
		break;
	case CANInput_commands::lsain:
		analog_chooser.replyAvailableClasses(replies);
		break;
	case CANInput_commands::addain:
		if(cmd.type == CMDtype::set){
			this->addAinType(cmd.val);
		}
		break;

	case CANInput_commands::rate:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(rate_idx);
		}else if(cmd.type == CMDtype::set){
			setReportRate(cmd.val);
		}else if(cmd.type == CMDtype::info){
			replies.emplace_back(report_rates_names());
		}
		break;

	case CANInput_commands::dvals:
		if(cmd.type == CMDtype::get){
			replies.emplace_back(digitalBuffer,0);
		}
		break;

	case CANInput_commands::avals:
		if(cmd.type == CMDtype::get){
			for(uint8_t i = 0;i<analogBuffer.size();i++){
				replies.emplace_back(analogBuffer[i],i);
			}
		}
		break;


	default:
		return CommandStatus::NOT_FOUND;
	}
	return CommandStatus::OK;
}


#endif

/*
 * FFBWheel_commands.cpp
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */


#include "FFBWheel.h"


bool FFBWheel::executeUserCommand(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	// ------------ General commands ----------------
	if(cmd->cmd == "save"){
		this->saveFlash();
		*reply+=">saved";
	}else if(cmd->cmd == "drvtype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.drvtype);
		}else if(cmd->type == CMDtype::set && cmd->val < (uint8_t)MotorDriverType::NONE){
			setDrvType(MotorDriverType(cmd->val));
		}else{
			*reply += "0:TMC4671";
		}
	}else if(cmd->cmd == "btntypes"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->btnsources);
		}else if(cmd->type == CMDtype::set){
			setBtnTypes(cmd->val);
		}else{
			*reply += "bin flag encoded list of button sources. (3 = source 1 & 2 active). See lsbtn for sources";
		}
	}else if(cmd->cmd == "lsbtn"){
		if(cmd->type == CMDtype::get){
			*reply += btn_chooser.printAvailableClasses();
		}
	}else if(cmd->cmd == "addbtn"){
		if(cmd->type == CMDtype::set){
			this->addBtnType(cmd->val);
		}
	}else if(cmd->cmd == "btnnum"){
		if(cmd->type == CMDtype::setat){
			ButtonSource* btn = this->getBtnSrc(cmd->adr);
			if(btn){
				ButtonSourceConfig c = btn->getConfig();
				c.numButtons = cmd->val;
				btn->setConfig(c);
			}

		}else if(cmd->type == CMDtype::getat){
			ButtonSource* btn = this->getBtnSrc(cmd->adr);
			if(btn){

				*reply+=std::to_string(btn->getBtnNum());
			}
		}else{
			*reply+="Err. Supply number of buttons for button source id as <c>?<id>=<num>";
		}
	}else if(cmd->cmd == "btnpol"){
		if(cmd->type == CMDtype::setat){
			ButtonSource* btn = this->getBtnSrc(cmd->adr);
			if(btn){
				ButtonSourceConfig c = btn->getConfig();
				c.invert = cmd->val == 0 ? false : true;
				btn->setConfig(c);
			}

		}else if(cmd->type == CMDtype::getat){
			ButtonSource* btn = this->getBtnSrc(cmd->adr);
			if(btn){
				ButtonSourceConfig c = btn->getConfig();
				*reply+=std::to_string(c.invert);
			}
		}else{
			*reply+="Err. invert: 1 else 0 for button source id as <c>?<id>=<pol>";
		}
	}else if(cmd->cmd == "btncut"){
		if(cmd->type == CMDtype::setat){
			ButtonSource* btn = this->getBtnSrc(cmd->adr);
			if(btn){
				ButtonSourceConfig c = btn->getConfig();
				c.cutRight = cmd->val == 0 ? false : true;
				btn->setConfig(c);
			}

		}else if(cmd->type == CMDtype::getat){
			ButtonSource* btn = this->getBtnSrc(cmd->adr);
			if(btn){
				ButtonSourceConfig c = btn->getConfig();
				*reply+=std::to_string(c.cutRight);
			}
		}else{
			*reply+="Err. cut right: 1 else 0 for button source id as <c>?<id>=<cut>";
		}
	}else if(cmd->cmd == "power"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(power);
		}else if(cmd->type == CMDtype::set){
			this->power = cmd->val;
		}
	}else if(cmd->cmd == "degrees"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->degreesOfRotation);
		}else if(cmd->type == CMDtype::set){
			this->degreesOfRotation = cmd->val;
		}
	}else if(cmd->cmd == "enctype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.enctype);
		}else if(cmd->type == CMDtype::set && cmd->val < (uint8_t)EncoderType::NONE){
			setEncType(EncoderType(cmd->val));
		}else{
			*reply += "0:ABN_LOCAL\nA1:BN_TMC\n2:HALL_TMC";
		}
	}else if(cmd->cmd == "axismask"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(aconf.analogmask);
		}else if(cmd->type == CMDtype::set){
			aconf.analogmask = cmd->val;
		}
	}else if(cmd->cmd == "ppr"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->enc->getPpr());
		}else if(cmd->type == CMDtype::set && this->enc != nullptr){
			this->enc->setPpr(cmd->val);
		}else{
			*reply += "Err. Setup enctype first";
		}
	}else if(cmd->cmd == "pos"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->enc->getPos());
		}else if(cmd->type == CMDtype::set && this->enc != nullptr){
			this->enc->setPos(cmd->val);
		}else{
			*reply += "Err. Setup enctype first";
		}

	}else if(cmd->cmd == "hidrate" && cmd->type == CMDtype::get){
		if(ffb->hid_out_period != 0){
			*reply+=std::to_string(1000/ffb->hid_out_period);
		}else{
			*reply+="0";
		}

	}else if(cmd->cmd == "help"){
		*reply += "FFBWheel commands:\n"
				"power,enctype,degrees,ppr,drvtype,btntype,lsbtn,btnnum,btntypes,btnpol,btncut,axismask\n"; // TODO
	}

	// ----------- TMC 4671 specific commands-------------
	if(this->conf.drvtype == MotorDriverType::TMC4671_type){
		TMC4671* drv = static_cast<TMC4671*>(this->drv);
		if(cmd->cmd == "mtype"){
			if(cmd->type == CMDtype::get){
				*reply+=std::to_string((uint8_t)drv->conf.motconf.motor_type);
			}else if(cmd->type == CMDtype::set && (uint8_t)cmd->type < (uint8_t)MotorType::ERR){
				drv->setMotorType((MotorType)cmd->val, drv->conf.motconf.pole_pairs);
			}

		}else if(cmd->cmd == "encalign"){
			if(cmd->type == CMDtype::get){
				drv->bangInitABN(3000);
			}else if(cmd->type ==CMDtype:: set){
				drv->bangInitABN(cmd->val);
			}
		}else if(cmd->cmd == "poles"){
			if(cmd->type == CMDtype::get){
				*reply+=std::to_string(drv->conf.motconf.pole_pairs);
			}else if(cmd->type == CMDtype::set){
				drv->setMotorType(drv->conf.motconf.motor_type,cmd->val);
			}

		}else if(cmd->cmd == "phiesrc"){
			if(cmd->type == CMDtype::get){
				*reply+=std::to_string((uint8_t)drv->conf.motconf.phiEsource);
			}else if(cmd->type == CMDtype::set){
				drv->setPhiEtype((PhiE)cmd->val);
			}

		}else if(cmd->cmd == "reg"){
			if(cmd->type == CMDtype::getat){
				*reply+=std::to_string(drv->readReg(cmd->val));
			}else if(cmd->type == CMDtype::setat){
				drv->writeReg(cmd->adr,cmd->val);
			}

		}else if(cmd->cmd == "help"){
			*reply += "TMC4671 commands:\n"
					"mtype,encalign,poles,phiesrc,reg\n";
		}else{
			flag=false;
		}
	}

	return flag;
}

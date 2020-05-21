/*
 * FFBWheel_commands.cpp
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */


#include "FFBWheel.h"


bool FFBWheel::command(ParsedCommand* cmd,std::string* reply){
	bool flag = true;
	// ------------ General commands ----------------
	if(cmd->cmd == "save"){
		this->saveFlash();
		*reply+=">saved";
	}else if(cmd->cmd == "drvtype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.drvtype);
		}else if(cmd->type == CMDtype::set && this->drv_chooser.isValidClassId(cmd->val)){
			setDrvType((cmd->val));
		}else{
			*reply += drv_chooser.printAvailableClasses();
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
	}else if(cmd->cmd == "zeroenc"){
		if(cmd->type == CMDtype::get){
			this->enc->setPos(0);
			*reply += "Zeroed";
		}
	}else if(cmd->cmd == "addbtn"){
		if(cmd->type == CMDtype::set){
			this->addBtnType(cmd->val);
		}

	}else if(cmd->cmd == "power"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(getPower());
		}else if(cmd->type == CMDtype::set){
			setPower(cmd->val);
		}
	}else if(cmd->cmd == "degrees"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->degreesOfRotation);
		}else if(cmd->type == CMDtype::set){
			this->degreesOfRotation = cmd->val;
		}
	}else if(cmd->cmd == "idlespring"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->ffb->getIdleSpringStrength());
		}else if(cmd->type == CMDtype::set){
			this->ffb->setIdleSpringStrength(cmd->val);
		}
	}else if(cmd->cmd == "friction"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->ffb->getFrictionStrength());
		}else if(cmd->type == CMDtype::set){
			this->ffb->setFrictionStrength(cmd->val);
		}
	}else if(cmd->cmd == "enctype"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string((uint8_t)this->conf.enctype);
		}else if(cmd->type == CMDtype::set && enc_chooser.isValidClassId(cmd->val)){
			setEncType((cmd->val));
		}else{
			*reply += enc_chooser.printAvailableClasses();
		}
	}else if(cmd->cmd == "axismask"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(aconf.analogmask);
		}else if(cmd->type == CMDtype::set){
			aconf.analogmask = cmd->val;
		}
	}else if(cmd->cmd == "invertx"){
		if(cmd->type == CMDtype::get){
			*reply+= aconf.invertX ? "1" : "0";
		}else if(cmd->type == CMDtype::set){
			aconf.invertX = cmd->val >= 1 ? true : false;
		}
	}else if(cmd->cmd == "ppr"){
		if(cmd->type == CMDtype::get){
			*reply+=std::to_string(this->enc->getCpr());
		}else if(cmd->type == CMDtype::set && this->enc != nullptr){
			this->enc->setCpr(cmd->val);
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
		*reply+=std::to_string(ffb->getRate());

	}else if(cmd->cmd == "ffbactive" && cmd->type == CMDtype::get){
		*reply+=std::to_string(ffb->getFfbActive() ? 1 : 0);

	}else if(cmd->cmd == "help"){
		flag = false;
		*reply += "FFBWheel commands:\n"
				"power,zeroenc,enctype,degrees,idlespring,friction,invertx,ppr,drvtype,btntype,lsbtn,btnnum,btntypes,btnpol,btncut,axismask,ffbactive\n"; // TODO
	}else{
		flag = false;
	}

	return flag;
}

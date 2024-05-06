/*
 * ClassChooser.h
 *
 *  Created on: 18.02.2020
 *      Author: Yannick
 */

#ifndef CLASSCHOOSER_H_
#define CLASSCHOOSER_H_

#include "vector"
#include "ChoosableClass.h"
#include <functional>
#include "Singleton.h"
#include <optional>
#include "CommandHandler.h"
#include "SystemCommands.h"

// If the class is a singleton do not create a new instance if one already exists. Return the existing pointer.


template<class B>
struct class_entry
{
	ClassIdentifier& info;
	uint16_t selectionId;
	std::function<B *()> create;
	std::function<bool ()> isCreatable = []() -> bool { return true; };
//	std::function<bool (T cls)> isCreatable = [](T cls){ ChoosableClass* c = dynamic_cast<ChoosableClass*>(cls); return (c != NULL) ? c::isCreatable() : true;}
};

//template<class T,class B>
//constexpr class_entry<B> add_class()
//{
//	if constexpr(std::is_base_of<Singleton<T>, T>::value){
//		return { T::info,T::info.id, []() -> B * { return Singleton<T>::getInstance(); },T::isCreatable };
//	}else{
////		return { T::info, []() -> B * { return new T; } };
//		return { T::info,T::info.id, []() -> B * { return new T; } , T::isCreatable};
//	}
//}

template<class T,class B>
constexpr class_entry<B> add_class(std::optional<uint16_t> selectionId=std::nullopt)
{
	if(!selectionId.has_value())
		selectionId = T::info.id;

	if constexpr(std::is_base_of<Singleton<T>, T>::value){
		return { T::info,selectionId.value(), []() -> B * { return Singleton<T>::getInstance(); },T::isCreatable };
	}else{
//		return { T::info, []() -> B * { return new T; } };
		return { T::info,selectionId.value(), []() -> B * { return new T; } , T::isCreatable };
	}
}


//template<class T,class B>
//class_entry<B> add_class_ref(B* ref)
//{
//	return { T::info,T::info.id, [ref]() -> B * { return ref; } ,T::isCreatable};
//}
//template<class B>
//class_entry<B> make_class_entry(ClassIdentifier info,B* ref)
//{
//	return { info,T::info.id, [ref]() -> B * { return  ref; },ref->isCreatable };
//}
// Override selection id for internal lists
template<class T,class B>
constexpr class_entry<B> add_class_ref(B* ref,std::optional<uint16_t> selectionId=std::nullopt)
{
	if(!selectionId.has_value())
		selectionId = T::info.id;

	return { T::info,selectionId, [ref]() -> B * { return ref; } ,T::isCreatable};
}
template<class B>
constexpr class_entry<B> make_class_entry(ClassIdentifier info,B* ref,std::optional<uint16_t> selectionId=std::nullopt)
{
	if(!selectionId.has_value())
		selectionId = B::info.id;
	return { info,selectionId, [ref]() -> B * { return  ref; },ref->isCreatable };
}

// TODO make this constexpr with fixed memory so the registry is not stored on heap
template<class T>
class ClassChooser {
public:
	ClassChooser(const std::vector<class_entry<T>>& classes) : class_registry(&classes){
		//this->class_registry = classes;
	}
	~ClassChooser(){

	}


	/**
	 * Creates a new instance of class
	 */
	T* Create(uint16_t id){
		T* cls = nullptr;
		for(const class_entry<T>& e : *class_registry){

			if(e.selectionId == id && e.isCreatable()){
				cls = e.create();
				cls->selectionId = id;
			}
		}
		return cls;
	}

	/**
	 * Check if class can be created
	 * Checks the isCreatable() function
	 */
	bool isCreatable(uint16_t id){
		for(const class_entry<T>& e : *class_registry){
			if(e.selectionId == id && e.isCreatable()){
				return true;
			}
		}
		return false;
	}


	/**
	 * Generates replies for the command system listing selectable classes
	 */
	void replyAvailableClasses(std::vector<CommandReply>& replies,int16_t ignoredCreatableId = 255){
		for(const class_entry<T>& cls : *class_registry){
			if(cls.info.visibility == ClassVisibility::hidden || (cls.info.visibility == ClassVisibility::debug && !SystemCommands::debugMode)){
				if(ignoredCreatableId != cls.selectionId)
					continue;
			}
//			std::string ret;
			CommandReply replyObj;
			replyObj.reply+= std::to_string(cls.selectionId);
			replyObj.reply+= ":";
			replyObj.reply+= (cls.isCreatable() || ignoredCreatableId == cls.selectionId) ? "1" : "0";
			replyObj.reply+= ":";
			replyObj.reply+= cls.info.name;

			if(cls.isCreatable()){
				replyObj.type = CommandReplyType::STRING_OR_DOUBLEINT;
				replyObj.adr = cls.selectionId;
				replyObj.val = cls.info.id;
			}else{
				replyObj.type = CommandReplyType::STRING;
			}
//			replyObj.reply = ret;
			replies.push_back(replyObj);
		}
	}


	/**
	 * Returns if this id is actually in the list of possible classes
	 */
	bool isValidClassId(uint16_t id){
		for(const class_entry<T>& cls : *class_registry){
			if(cls.selectionId == id){
				return true;
			}
		}
		return false;
	}
private:
	const std::vector<class_entry<T>>* class_registry;

};

#endif /* CLASSCHOOSER_H_ */

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

// If the class is a singleton do not create a new instance if one already exists. Return the existing pointer.


template<class B>
struct class_entry
{
	ClassIdentifier info;
	uint8_t selectionId;
	std::function<B *()> create;
	std::function<bool ()> isCreatable = []() -> bool { return true; };
//	std::function<bool (T cls)> isCreatable = [](T cls){ ChoosableClass* c = dynamic_cast<ChoosableClass*>(cls); return (c != NULL) ? c::isCreatable() : true;}
};

template<class T,class B>
constexpr class_entry<B> add_class()
{
	if constexpr(std::is_base_of<Singleton<T>, T>::value){
		return { T::info,T::info.id, []() -> B * { return Singleton<T>::getInstance(); },T::isCreatable };
	}else{
//		return { T::info, []() -> B * { return new T; } };
		return { T::info,T::info.id, []() -> B * { return new T; } , T::isCreatable};
	}
}

template<class T,class B>
constexpr class_entry<B> add_class(uint8_t selectionId)
{
	if constexpr(std::is_base_of<Singleton<T>, T>::value){
		return { T::info,selectionId, []() -> B * { return Singleton<T>::getInstance(); },T::isCreatable };
	}else{
//		return { T::info, []() -> B * { return new T; } };
		return { T::info,selectionId, []() -> B * { return new T; } , T::isCreatable};
	}
}


template<class T,class B>
class_entry<B> add_class_ref(B* ref)
{
	return { T::info,T::info.id, [ref]() -> B * { return ref; } ,T::isCreatable};
}
template<class B>
class_entry<B> make_class_entry(ClassIdentifier info,B* ref)
{
	return { info,T::info.id, [ref]() -> B * { return  ref; },ref->isCreatable };
}
// Override selection id for internal lists
template<class T,class B>
class_entry<B> add_class_ref(B* ref,uint8_t selectionId)
{
	return { T::info,selectionId, [ref]() -> B * { return ref; } ,T::isCreatable};
}
template<class B>
class_entry<B> make_class_entry(ClassIdentifier info,B* ref,uint8_t selectionId)
{
	return { info,selectionId, [ref]() -> B * { return  ref; },ref->isCreatable };
}

template<class T>
class ClassChooser {
public:
	ClassChooser(const std::vector<class_entry<T>>& classes) : class_registry(classes){
		//this->class_registry = classes;
	}
	~ClassChooser(){

	}


	const std::vector<class_entry<T>>& class_registry;

	/**
	 * Creates a new instance of class
	 */
	T* Create(uint16_t id){
		T* cls = nullptr;
		for(class_entry<T> e : class_registry){

			if(e.selectionId == id && e.isCreatable()){
				cls = e.create();
			}
		}
		return cls;
	}

	/**
	 * Check if class can be created
	 * Checks the isCreatable() function
	 */
	bool isCreatable(uint16_t id){
		for(class_entry<T> e : class_registry){
			if(e.selectionId == id && e.isCreatable()){
				return true;
			}
		}
		return false;
	}

	/**
	 * Returns a string of classes available to this classchooser
	 * ignoredCreatableId will list as creatable even if it is not. Useful to make a single class list as valid if it was already chosen
	 */
	std::string printAvailableClasses(int16_t ignoredCreatableId = 255){
		std::string ret;
		for(class_entry<T> cls : class_registry){
			if(cls.info.hidden){
				continue;
			}
			ret+= std::to_string(e.selectionId);
			ret+= ":";
			ret+= (cls.isCreatable() || ignoredCreatableId == e.selectionId) ? "1" : "0";
			ret+= ":";
			ret+= cls.info.name;
			ret+='\n';
		}
		return ret;
	}


	/**
	 * Returns if this id is actually in the list of possible classes
	 */
	bool isValidClassId(uint16_t id){
		for(class_entry<T> cls : class_registry){
			if(e.selectionId == id){
				return true;
			}
		}
		return false;
	}


};

#endif /* CLASSCHOOSER_H_ */

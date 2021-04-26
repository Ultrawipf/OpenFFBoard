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
	std::function<B *()> create;
	std::function<bool ()> isCreatable = []() -> bool { return true; };
//	std::function<bool (T cls)> isCreatable = [](T cls){ ChoosableClass* c = dynamic_cast<ChoosableClass*>(cls); return (c != NULL) ? c::isCreatable() : true;}
};

template<class T,class B>
constexpr class_entry<B> add_class()
{
	if constexpr(std::is_base_of<Singleton<T>, T>::value){
		return { T::info, []() -> B * { return Singleton<T>::getInstance(); },T::isCreatable };
	}else{
//		return { T::info, []() -> B * { return new T; } };
		return { T::info, []() -> B * { return new T; } , T::isCreatable};
	}

}

template<class T,class B>
class_entry<B> add_class_ref(B* ref)
{
//	return { T::info, [ref]() -> B * { return  ref; } };
	return { T::info, [ref]() -> B * { return ref; } ,T::isCreatable};
}
template<class B>
class_entry<B> make_class_entry(ClassIdentifier info,B* ref)
{
	return { info, [ref]() -> B * { return  ref; },ref->isCreatable };
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

	// Create class

	T* Create(uint16_t id){
		T* cls = nullptr;
		for(class_entry<T> e : class_registry){

			if(e.info.id == id && e.isCreatable()){
				cls = e.create();
			}
		}
		return cls;
	}

	// Check if class can be created
	bool isCreatable(uint16_t id){
		for(class_entry<T> e : class_registry){
			if(e.info.id == id && e.isCreatable()){
				return true;
			}
		}
		return false;
	}


	std::string printAvailableClasses(){
		std::string ret;
		for(class_entry<T> cls : class_registry){
			if(cls.info.hidden || !cls.isCreatable()){
				continue;
			}
			ret+= std::to_string(cls.info.id);
			ret+= ":";
			ret+= cls.info.name;
			ret+='\n';
		}
		return ret;
	}



	bool isValidClassId(uint16_t id){
		for(class_entry<T> cls : class_registry){
			if(cls.info.id == id){
				return true;
			}
		}
		return false;
	}


};

#endif /* CLASSCHOOSER_H_ */

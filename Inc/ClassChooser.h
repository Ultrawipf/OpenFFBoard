/*
 * ClassChooser.h
 *
 *  Created on: 18.02.2020
 *      Author: Yannick
 */

#ifndef CLASSCHOOSER_H_
#define CLASSCHOOSER_H_

#include "vector"
#include "cppmain.h"
#include "ChoosableClass.h"
#include <functional>

template<class T>
struct class_entry
{
	ClassIdentifier info;
	std::function<T *()> create;
};

template<class T,class B>
class_entry<B> add_class()
{
  return { T::info, []() -> B * { return new T; } };
}

template<class T,class B>
class_entry<B> add_class_ref(B* ref)
{
	return { T::info, [ref]() -> B * { return  ref; } };
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
			if(e.info.id == id){
				cls = e.create();
			}
		}
		return cls;
	}


	std::string printAvailableClasses(){
		std::string ret;
		for(class_entry<T> cls : class_registry){
			if(cls.info.hidden){
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

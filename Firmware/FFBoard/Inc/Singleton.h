/*
 * Singleton.h
 *
 *  Created on: 15.04.2021
 *      Author: Yannick
 */

#ifndef SRC_SINGLETON_H_
#define SRC_SINGLETON_H_
#include "main.h"
#include "memory"
/*
 * Helper class to enforce singleton pattern and prevent creation of multiple instances by the classchooser
 * This is intended to be used by the classchooser. It will not prevent you from making multiple instances by calling the constructor directly
 * Subclasses have to inherit this to prevent classchooser from creating multiple instances
 */
template<typename T>
class Singleton {

protected:
    static T* instance_ptr;
    Singleton() {}

    ~Singleton(){
    	Singleton<T>::instance_ptr = nullptr; // Reset pointer if destroyed
    	Singleton<T>::singletonRefCount = 0;
    }
    static int16_t singletonRefCount;

public:

    static T* getInstance(){
    	singletonRefCount++;
    	if(!Singleton<T>::instance_ptr){
    		Singleton<T>::instance_ptr = new T();
    	}
    	return Singleton<T>::instance_ptr;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator= (const Singleton) = delete;
    static int16_t getSingletonRefCount(){return Singleton<T>::singletonRefCount;}

};

#endif /* SRC_SINGLETON_H_ */

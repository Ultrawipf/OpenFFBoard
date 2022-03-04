/*
 * ChoosableClass.h
 *
 *  Created on: 18.02.2020
 *      Author: Yannick
 */

#ifndef CHOOSABLECLASS_H_
#define CHOOSABLECLASS_H_
#include "ClassIDs.h"
#include "string"

enum class ClassVisibility : uint8_t {visible,debug,hidden};

struct ClassIdentifier {
	const char* name = nullptr; // Display name of this class
	uint16_t id;	// The id equivalent to clsname. Classes with the same clsname must have the same id in this field. Classchooser can use this or a separate id for selection
	ClassVisibility visibility = ClassVisibility::visible;
};

template<class T>
class ClassChooser;

class ChoosableClass {
	template<class T>
	friend class ClassChooser;

public:
	//ChoosableClass(){};
	virtual ~ChoosableClass(){};

	static ClassIdentifier info;


	/**
	 * Returns true if a new instance can be created.
	 * Use this to do prechecks if ressources are available
	 * If it returns false this can signal the classchooser that at this time a new instance can not be created.
	 */
	static bool isCreatable() {return true;};
	virtual const ClassIdentifier getInfo() = 0;

	uint16_t getSelectionID(){return selectionId;}; //!< returns the used classchooser selection id of this instance

	/**
	 * Type of this class. Mainclass, motordriver...
	 * Should be implemented by the parent class so it is not in the info struct
	 */
	virtual const ClassType getClassType(){return ClassType::NONE;};

protected:
	uint16_t selectionId;	//!< Should only be written by ClassChooser during creation.
};

#endif /* CHOOSABLECLASS_H_ */

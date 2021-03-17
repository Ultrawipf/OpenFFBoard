/*
 * RessourceManager.h
 *
 *  Created on: 17.03.2021
 *      Author: Yannick
 */

#ifndef SRC_RESSOURCEMANAGER_H_
#define SRC_RESSOURCEMANAGER_H_

#include "semaphore.hpp"

/*
 * Manages mutexes for ports
 */
class RessourceManager {
public:
	RessourceManager();
	virtual ~RessourceManager();
};

#endif /* SRC_RESSOURCEMANAGER_H_ */

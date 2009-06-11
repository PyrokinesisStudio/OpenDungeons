#include "Creature.h"
#include "Defines.h"
#include "Globals.h"
#include "Functions.h"
#include "CreatureAction.h"
#include "Network.h"

Creature::Creature()
{
	position = Ogre::Vector3(0,0,0);
	scale = Ogre::Vector3(1,1,1);
	sightRadius = 10.0;
	digRate = 10.0;
	destinationX = 0;
	destinationY = 0;

	hp = 10;
	mana = 10;
	sightRadius = 10;
	digRate = 10;
	moveSpeed = 1.0;
	tilePassability = Tile::walkableTile;

	//currentTask = idle;
	animationState = NULL;

	if(positionTile() != NULL)
		positionTile()->addCreature(this);

	actionQueue.push_back(CreatureAction(CreatureAction::idle));
}

Creature::Creature(string nClassName, string nMeshName, Ogre::Vector3 nScale, int nHP, int nMana, double nSightRadius, double nDigRate)
{
	className = nClassName;
	meshName = nMeshName;
	scale = nScale;
	destinationX = 0;
	destinationY = 0;

	hp = nHP;
	mana = nMana;
	sightRadius = nSightRadius;
	digRate = nDigRate;
	moveSpeed = 1.0;
	tilePassability = Tile::walkableTile;

	//currentTask = idle;
	animationState = NULL;

	if(positionTile() != NULL)
		positionTile()->addCreature(this);
}

ostream& operator<<(ostream& os, Creature *c)
{
	os << c->className << "\t" << c->name << "\t";
	os << c->position.x << "\t" << c->position.y << "\t" << c->position.z << "\t";
	os << c->color << "\n";

	return os;
}

istream& operator>>(istream& is, Creature *c)
{
	static int uniqueNumber = 1;
	double xLocation = 0.0, yLocation = 0.0, zLocation = 0.0;
	string tempString;
	is >> c->className;

	is >> tempString;

	if(tempString.compare("autoname") == 0)
	{
		char tempArray[255];
		sprintf(tempArray, "%s_%04i", c->className.c_str(), uniqueNumber);
		tempString = string(tempArray);
		uniqueNumber++;
	}

	c->name = tempString;

	is >> xLocation >> yLocation >> zLocation;
	c->position = Ogre::Vector3(xLocation, yLocation, zLocation);
	is >> c->color;

	// Copy the class based items
	Creature *creatureClass = gameMap.getClassDescription(c->className);
	c->meshName = creatureClass->meshName;
	c->scale = creatureClass->scale;
	c->sightRadius = creatureClass->sightRadius;
	c->digRate = creatureClass->digRate;
	c->hp = creatureClass->hp;
	c->mana = creatureClass->mana;

	return is;
}

/*! \brief Allocate storage for, load, and inform OGRE about a mesh for this creature.
 *
 *  This function is called after a creature has been loaded from hard disk,
 *  received from a network connection, or created during the game play by the
 *  game engine itself.
 */
void Creature::createMesh()
{
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::createCreature;
	request->p = this;

	sem_wait(&renderQueueSemaphore);
	renderQueue.push_back(request);
	sem_post(&renderQueueSemaphore);

	//FIXME:  This function needs to wait until the render queue has processed the request before returning.  This should fix the bug where the client crashes loading levels with lots of creatures.  Other create mesh routines should have a similar wait statement.

}


/*! \brief Free the mesh and inform the OGRE system that the mesh has been destroyed.
 *
 *  This function is primarily a helper function for other methods.
 */
void Creature::destroyMesh()
{
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::destroyCreature;
	request->p = this;

	sem_wait(&renderQueueSemaphore);
	renderQueue.push_back(request);
	sem_post(&renderQueueSemaphore);
}

/*! \brief Changes the creatures position to a new position.
 *
 *  This is an overloaded function which just calls Creature::setPosition(double x, double y, double z).
 */
void Creature::setPosition(Ogre::Vector3 v)
{
	setPosition(v.x, v.y, v.z);
}

/*! \brief Changes the creatures position to a new position.
 *
 *  Moves the creature to a new location in 3d space.  This function is
 *  responsible for informing OGRE anything it needs to know, as well as
 *  maintaining the list of creatures in the individual tiles.
 */
void Creature::setPosition(double x, double y, double z)
{
	SceneNode *creatureSceneNode = mSceneMgr->getSceneNode(name + "_node");

	//creatureSceneNode->setPosition(x/BLENDER_UNITS_PER_OGRE_UNIT, y/BLENDER_UNITS_PER_OGRE_UNIT, z/BLENDER_UNITS_PER_OGRE_UNIT);
	creatureSceneNode->setPosition(x, y, z);

	// Move the creature relative to its parent scene node.  We record the
	// tile the creature is in before and after the move to properly
	// maintain the results returned by the positionTile() function.
	Tile *oldPositionTile = positionTile();
	position = Ogre::Vector3(x, y, z);
	Tile *newPositionTile = positionTile();

	if(oldPositionTile != newPositionTile)
	{
		if(oldPositionTile != NULL)
			oldPositionTile->removeCreature(this);

		if(positionTile() != NULL)
			positionTile()->addCreature(this);
	}
}

/*! \brief A simple accessor function to get the creature's current position in 3d space.
 *
 */
Ogre::Vector3 Creature::getPosition()
{
	return position;
}

/*! \brief The main AI routine which decides what the creature will do and carries out that action.
 *
 * The doTurn routine is the heart of the Creature AI subsystem.  The other,
 * higher level, functions such as GameMap::doTurn() ultimately just call this
 * function to make the creatures act.
 *
 * The function begins in a pre-cognition phase which prepares the creature's
 * brain state for decision making.  This involves generating lists of known
 * about creatures, either through sight, hearing, keeper knowledge, etc, as
 * well as some other bookkeeping stuff.
 *
 * Next the function enters the cognition phase where the creature's current
 * state is examined and a decision is made about what to do.  The state of the
 * creature is in the form of a queue, which is really used more like a stack.
 * At the beginning of the game the 'idle' action is pushed onto each
 * creature's actionQueue, this action is never removed from the tail end of
 * the queue and acts as a "last resort" for when the creature completely runs
 * out of things to do.  Other actions such as 'walkToTile' or 'attackCreature'
 * are then pushed onto the front of the queue and will determine the
 * creature's future behavior.  When actions are complete they are popped off
 * the front of the action queue, causing the creature to revert back into the
 * state it was in when the actions was placed onto the queue.  This allows
 * actions to be carried out recursively, i.e. if a creature is trying to dig a
 * tile and it is not nearby it can begin walking toward the tile as a new
 * action, and when it arrives at the tile it will revert to the 'digTile'
 * action.
 *
 * In the future there should also be a post-cognition phase to do any
 * additional checks after it tries to move, etc.
 */
void Creature::doTurn()
{
	vector<Tile*> markedTiles;
	Tile *nextStep;
	list<Tile*>walkPath;
	list<Tile*>basePath;
	vector< list<Tile*> > possiblePaths;
	vector< list<Tile*> > shortPaths;

	// If we are not standing somewhere on the map, do nothing.
	if(positionTile() == NULL)
		return;

	bool loopBack;
	// Look at the surrounding area
	updateVisibleTiles();

	// If the current task was 'idle' and it changed to something else, start doing the next
	// thing during this turn instead of waiting until the next turn.
	do
	{
		loopBack = false;

		// Carry out the current task
		int tempX, tempY, baseEndX, baseEndY;
		double diceRoll;
		Tile *neighborTile;
		vector<Tile*>neighbors, creatureNeighbors;
		bool wasANeighbor = false;

		diceRoll = randomDouble(0.0, 1.0);
		if(actionQueue.size() > 0)
		{
			switch(actionQueue.front().type)
			{
				case CreatureAction::idle:
					cout << "idle ";
					setAnimationState("Idle");
					//FIXME: make this into a while loop over a vector of <action, probability> pairs

					// Decide to check for diggable tiles with some probability
					if(diceRoll < 0.4 && digRate > 0.1)
					{
						//loopBack = true;
						actionQueue.push_front(CreatureAction(CreatureAction::digTile));
						loopBack = true;
					}

					// Decide to "wander" a short distance
					else if(diceRoll < 0.6)
					{
						//loopBack = true;
						actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
						int tempX = position.x + 2.0*gaussianRandomDouble();
						int tempY = position.y + 2.0*gaussianRandomDouble();

						list<Tile*> result = gameMap.path(positionTile()->x, positionTile()->y, tempX, tempY, tilePassability);
						if(result.size() >= 2)
						{
							setAnimationState("Walk");
							gameMap.cutCorners(result, tilePassability);
							list<Tile*>::iterator itr = result.begin();
							itr++;
							while(itr != result.end())
							{
								addDestination((*itr)->x, (*itr)->y);
								itr++;
							}
						}
					}
					else
					{
						// Remain idle
						//setAnimationState("Idle");
					}

					break;

				case CreatureAction::walkToTile:
					cout << "walkToTile ";
					if(walkQueue.size() == 0)
					{
						actionQueue.pop_front();
						loopBack = true;
					}
					break;

				case CreatureAction::digTile:
					cout << "dig ";

					// Find visible tiles, marked for digging
					for(unsigned int i = 0; i < visibleTiles.size(); i++)
					{
						// Check to see if the tile is marked for digging
						if(visibleTiles[i]->getMarkedForDigging())
						{
							markedTiles.push_back(visibleTiles[i]);
						}
					}

					// See if any of the tiles is one of our neighbors
					wasANeighbor = false;
					creatureNeighbors = gameMap.neighborTiles(position.x, position.y);
					for(unsigned int i = 0; i < creatureNeighbors.size() && !wasANeighbor; i++)
					{
						if(creatureNeighbors[i]->getMarkedForDigging())
						{
							setAnimationState("Dig");
							creatureNeighbors[i]->setFullness(creatureNeighbors[i]->getFullness() - digRate);

							// Force all the neighbors to recheck their meshes as we may have exposed
							// a new side that was not visible before.
							neighbors = gameMap.neighborTiles(creatureNeighbors[i]->x, creatureNeighbors[i]->y);
							for(unsigned int j = 0; j < neighbors.size(); j++)
							{
								neighbors[j]->setFullness(neighbors[j]->getFullness());
							}

							if(creatureNeighbors[i]->getFullness() < 0)
							{
								creatureNeighbors[i]->setFullness(0);
							}

							// If the tile has been dug out, move into that tile and idle
							if(creatureNeighbors[i]->getFullness() == 0)
							{
								addDestination(creatureNeighbors[i]->x, creatureNeighbors[i]->y);
								setAnimationState("Walk");

								// Remove the dig action and replace it with
								// walking to the newly dug out tile.
								actionQueue.pop_front();
								actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
							}

							wasANeighbor = true;
							break;
						}
					}

					if(wasANeighbor)
						break;

					// Find paths to all of the neighbor tiles for all of the marked visible tiles.
					possiblePaths.clear();
					for(unsigned int i = 0; i < markedTiles.size(); i++)
					{
						neighbors = gameMap.neighborTiles(markedTiles[i]->x, markedTiles[i]->y);
						for(unsigned int j = 0; j < neighbors.size(); j++)
						{
							neighborTile = neighbors[j];
							if(neighborTile != NULL && neighborTile->getFullness() == 0)
								possiblePaths.push_back(gameMap.path(positionTile()->x, positionTile()->y, neighborTile->x, neighborTile->y, tilePassability));

						}
					}

					// Find the shortest path and start walking toward the tile to be dug out
					if(possiblePaths.size() > 0)
					{
						// Find the N shortest valid paths, see if there are any valid paths shorter than this first guess
						shortPaths.clear();
						for(unsigned int i = 0; i < possiblePaths.size(); i++)
						{
							// If the current path is long enough to be valid
							unsigned int currentLength = possiblePaths[i].size();
							if(currentLength >= 2)
							{
								shortPaths.push_back(possiblePaths[i]);

								// If we already have enough short paths
								if(shortPaths.size() > 5)
								{
									unsigned int longestLength, longestIndex;

									// Kick out the longest
									longestLength = shortPaths[0].size();
									longestIndex = 0;
									for(unsigned int j = 1; j < shortPaths.size(); j++)
									{
										if(shortPaths[j].size() > longestLength)
										{
											longestLength = shortPaths.size();
											longestIndex = j;
										}
									}

									shortPaths.erase(shortPaths.begin() + longestIndex);
								}
							}
						}

						// Randomly pick a short path to take
						unsigned int numShortPaths = shortPaths.size();
						if(numShortPaths > 0)
						{
							unsigned int shortestIndex;
							shortestIndex = (int)randomDouble(0, (double)numShortPaths-0.001);
							walkPath = shortPaths[shortestIndex];

							// If the path is a legitimate path, walk down it to the tile to be dug out
							if(walkPath.size() >= 2)
							{
								setAnimationState("Walk");
								gameMap.cutCorners(walkPath, tilePassability);
								list<Tile*>::iterator itr = walkPath.begin();
								itr++;
								while(itr != walkPath.end())
								{
									addDestination((*itr)->x, (*itr)->y);
									itr++;
								}

								actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
								break;
							}
						}
					}

					// If none of our neighbors are marked for digging we got here too late.
					// Finish digging
					if(actionQueue.front().type == CreatureAction::digTile)
					{
						actionQueue.pop_front();
						loopBack = true;
					}
					break;

				default:
					cout << "default ";
					break;
			}
		}
		else
		{
			cout << "\n\nERROR:  Creature has empty action queue in doTurn(), this should not happen.\n\n";
			exit(1);
		}

	} while(loopBack);
}

/*! \brief Creates a list of Tile pointers in visibleTiles
 *
 * The tiles are currently determined to be visible or not, according only to
 * the distance they are away from the creature.  Because of this they can
 * currently see through walls, etc.
*/
void Creature::updateVisibleTiles()
{
	int xMin, yMin, xMax, yMax;

	//cout << "sightrad: " << sightRadius << " ";
	visibleTiles.clear();
	xMin = (int)position.x - sightRadius;
	xMax = (int)position.x + sightRadius;
	yMin = (int)position.y - sightRadius;
	yMax = (int)position.y + sightRadius;

	// Add the circular portion of the visible region
	for(int i = xMin; i < xMax; i++)
	{
		for(int j = yMin; j < yMax; j++)
		{
			int distSQ = powl(position.x - i, 2.0) + powl(position.y - j, 2.0);
			if(distSQ < sightRadius * sightRadius)
			{
				Tile *currentTile = gameMap.getTile(i, j);
				if(currentTile != NULL)
					//TODO:  This does not implement terrain yet, i.e. the creature can see through walls
					// if(currentTile->isVisibleFrom(positionTile()))
					visibleTiles.push_back(currentTile);
			}
		}
	}

	//TODO:  Add the sector shaped region of the visible region
}


/*! \brief Stub method, not implemented yet.
 *
 * 
*/
void createVisualDebugEntities()
{
	//TODO:  fill in this stub method.
}

/*! \brief Stub method, not implemented yet.
 *
 * 
*/
void destroyVisualDebugEntities()
{
	//TODO:  fill in this stub method.
}

/*! \brief Returns a pointer to the tile the creature is currently standing in.
 *
 * 
*/
Tile* Creature::positionTile()
{
	return gameMap.getTile((int)(position.x), (int)(position.y));
}

/*! \brief Completely destroy this creature, including its OGRE entities, scene nodes, etc.
 *
 * 
*/
void Creature::deleteYourself()
{
	if(positionTile() != NULL)
		positionTile()->removeCreature(this);

	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::destroyCreature;
	request->p = this;

	RenderRequest *request2 = new RenderRequest;
	request2->type = RenderRequest::deleteCreature;
	request2->p = this;

	sem_wait(&renderQueueSemaphore);
	renderQueue.push_back(request);
	renderQueue.push_back(request2);
	sem_post(&renderQueueSemaphore);
}

/*! \brief Sets a new animation state from the creature's library of animations.
 *
 * 
*/
void Creature::setAnimationState(string s)
{
	string tempString;
	stringstream tempSS;
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::setCreatureAnimationState;
	request->p = this;
	request->str = s;

	if(serverSocket != NULL)
	{
		// Place a message in the queue to inform the clients about the new animation state
		ServerNotification *serverNotification = new ServerNotification;
		serverNotification->type = ServerNotification::creatureSetAnimationState;
		serverNotification->str = s;
		serverNotification->cre = this;
		serverNotificationQueue.push_back(serverNotification);
		sem_post(&serverNotificationQueueSemaphore);
	}

	sem_wait(&renderQueueSemaphore);
	renderQueue.push_back(request);
	sem_post(&renderQueueSemaphore);
}

/*! \brief Returns the creature's currently active animation state.
 *
 * 
*/
AnimationState* Creature::getAnimationState()
{
	return animationState;
}

/*! \brief Adds a position in 3d space to the creature's walk queue and, if necessary, starts it walking.
 *
 * This function also places a message in the serverNotificationQueue so that
 * relevant clients are informed about the change.
 * 
*/
void Creature::addDestination(int x, int y)
{
	cout << "w(" << x << ", " << y << ") ";
	Ogre::Vector3 destination(x, y, 0);

	// if there are currently no destinations in the walk queue
	if(walkQueue.size() == 0)
	{
		// Add the destination and set the remaining distance counter
		walkQueue.push_back(destination);
		shortDistance = position.distance(walkQueue.front());

		// Rotate the creature to face the direction of the destination
		walkDirection = walkQueue.front() - position;
		walkDirection.normalise();

		SceneNode *node = mSceneMgr->getSceneNode(name + "_node");
		Ogre::Vector3 src = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;
		Quaternion quat = src.getRotationTo(walkDirection);
		node->rotate(quat);
	}
	else
	{
		// Add the destination
		walkQueue.push_back(destination);
	}

	// Place a message in the queue to inform the clients about the new destination
	ServerNotification *serverNotification = new ServerNotification;
	serverNotification->type = ServerNotification::creatureAddDestination;
	serverNotification->str = name;
	serverNotification->vec = destination;
	serverNotificationQueue.push_back(serverNotification);
	sem_post(&serverNotificationQueueSemaphore);
}


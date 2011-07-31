/*!
 * \file   ASWrapper.h
 * \date   08 July 2011
 * \author StefanP.MUC
 * \brief  Initializes AngelScript and provides access to its functions
 */

#ifndef ASWRAPPER_H_
#define ASWRAPPER_H_

#include <Ogre.h>

class asIScriptEngine;
class asIScriptModule;
class asIScriptContext;
class asSMessageInfo;

class ASWrapper :
    public Ogre::Singleton<ASWrapper>
{
    public:
        ASWrapper();
        ~ASWrapper();

        void loadScript(std::string fileName);
        void executeFunction(std::string function);

        // \brief helper function for registering the AS factories
        template<class T>
        static T* createInstance(){return new T();}

        // \brief dummy addref/release function
        static void dummyRef(){}

    private:
        asIScriptEngine* engine;
        asIScriptModule* module;
        asIScriptContext* context;

        void messageCallback(const asSMessageInfo* msg, void* param);
        void registerEverything();
};


#endif /* ASWRAPPER_H_ */
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   DiffEquations.cpp
 * Author: rafael.luiz.cancian
 * 
 * Created on
 */

#include "DiffEquations.h"
#include "../../kernel/simulator/Model.h"
//#include "../../kernel/simulator/Simulator.h"
//#include "../../kernel/simulator/PluginManager.h"

#include <fstream>
#include <dlfcn.h>
#include <string>


// Externalize function GetPluginInformation to be accessible throught dynamic linked library
#ifdef PLUGINCONNECT_DYNAMIC
extern "C" StaticGetPluginInformation GetPluginInformation() {
	return &DiffEquations::GetPluginInformation;
}
#endif


//
// public: //! constructors
//

DiffEquations::DiffEquations(Model* model, std::string name) : ModelComponent(model, Util::TypeOf<DiffEquations>(), name) {
}


//
// public: //! new public user methods for this component
//

List<std::string> *DiffEquations::getEquations() const {
	return _equations;
}

double DiffEquations::getfinalTime() const {
	return _finalTime;
}

void DiffEquations::setFinalTime(double newFinalTime) {
	_finalTime = newFinalTime;
}

double DiffEquations::getmaxSteps() const {
	return _maxSteps;
}

void DiffEquations::setMaxSteps(double newMaxSteps) {
	_maxSteps = newMaxSteps;
}

double DiffEquations::getprecision() const {
	return _precision;
}

void DiffEquations::setPrecision(double newPrecision) {
	_precision = newPrecision;
}

List<double> *DiffEquations::getvaluesOnFinalTime() const {
	return _valuesOnFinalTime;
}

// ...


//
// public: //! virtual methods
//

std::string DiffEquations::show() {
	return ModelComponent::show() + "";
}


//
// public: //! static methods that must have implementations (Load and New just the same. GetInformation must provide specific infos for the new component
//

PluginInformation* DiffEquations::GetPluginInformation() {
	PluginInformation* info = new PluginInformation(Util::TypeOf<DiffEquations>(), &DiffEquations::LoadInstance, &DiffEquations::NewInstance);
	info->setCategory("Continuous");
	//info->setMinimumInputs(1);
	//info->setMinimumOutputs(1);
	//info->setMaximumInputs(1);
	//info->setMaximumOutputs(1);
	//info->setSource(false);
	//info->setSink(false);
	//info->setSendTransfer(false);
	//info->setReceiveTransfer(false);
	info->insertDynamicLibFileDependence("cppcompiler.so");
	//info->setDescriptionHelp("//@TODO");
	//info->setAuthor("...");
	//info->setDate("...");
	//info->setObservation("...");
	return info;
}

ModelComponent* DiffEquations::LoadInstance(Model* model, PersistenceRecord *fields) {
	DiffEquations* newComponent = new DiffEquations(model);
	try {
		newComponent->_loadInstance(fields);
	} catch (const std::exception& e) {

	}
	return newComponent;
}

ModelDataDefinition* DiffEquations::NewInstance(Model* model, std::string name) {
	return new DiffEquations(model, name);
}

//
// protected: //! virtual method that must be overriden
//

bool DiffEquations::_loadInstance(PersistenceRecord *fields) {
	bool res = ModelComponent::_loadInstance(fields);
	if (res) {
		// @TODO: not implemented yet
	}
	return res;
}

void DiffEquations::_saveInstance(PersistenceRecord *fields, bool saveDefaultValues) {
	ModelComponent::_saveInstance(fields, saveDefaultValues);
	// @TODO: not implemented yet
}

void DiffEquations::_onDispatchEvent(Entity* entity, unsigned int inputPortNumber) {
	traceSimulation(this, "Invoking cpp user code for dispatchEvent");
	try {
		if (dispatchEvent_SharedLibHandler != nullptr)
			dispatchEvent_SharedLibHandler(_parentModel->getParentSimulator(), _parentModel, entity); // call shared library
	} catch (const std::exception& e) {

	}
	this->_parentModel->sendEntityToComponent(entity, this->getConnections()->getFrontConnection());
}


//
// protected: //! virtual methods that could be overriden by derived classes, if needed
//


bool DiffEquations::_check(std::string* errorMessage) {
	bool resultAll = true;
	CppCompiler::CompilationResult result;
	std::string name = this->getName();
	std::string sourceCode = "\
//\n\
// File automatically generated by GenESyS DiffEquations plugin\n\
// Date: \n\
//\n\
#include <iostream>\n\
#include <string>\n\
//@TODO: Adjust depending on the running path\n\
#include \"../../../../kernel/simulator/Simulator.h\"\n\
#include \"../../../../kernel/simulator/Model.h\"\n\
#include \"../../../../kernel/simulator/Entity.h\"\n\
#include \"../../../../tools/SolverDefaultImpl1.h\"\n\
\n\
\n\
void _onDispatchEvent_" + name + "(Simulator* simulator, Model* model, Entity* entity) {\n\
	Solver_if *_solver = new SolverDefaultImpl1("+std::to_string(_precision)+", "+std::to_string(_maxSteps)+");\n\
	double x["+std::to_string(_equations->size())+"];\
	delete solver;\n\
}\n\
\n\
extern \"C\" void onDispatchEvent" + "(Simulator* simulator, Model* model, Entity* entity) {\n\
	try {\n\
		_onDispatchEvent_" + name + "(simulator, model, entity);\n\
	} catch (const std::exception& e) {\n\
		model->getTracer()->traceError(e, \"Error while executing onDispatchEvent cpp user code in " + name + ".\");\n\
	}\n\
}\n\
\n";

	// save the code for the compiler
	std::string _sourceFilename = "./" + name + ".cpp";
	std::string _outputFilename = "./" + name + ".so";
	try {
		trace("Saving source file \"" + _sourceFilename + "\"");
		std::ofstream outfile(_sourceFilename);
		outfile << sourceCode;
		outfile.close();
	} catch (const std::exception& e) {
		resultAll = false;
		*errorMessage += "Error saving source code to compile: ";// + e.what();
	}
	// if saved, compile
	if (resultAll) {
		trace("Compiling source file \"" + _sourceFilename + "\"");
		_cppCompiler->setSourceFilename(_sourceFilename);
		_cppCompiler->setOutputFilename(_outputFilename);
		std::string objectFiles = "";
		_cppCompiler->setObjectFiles(objectFiles);
		CppCompiler::CompilationResult result = _cppCompiler->compileToDynamicLibrary();
		if (!result.success) {
			resultAll = false;
			if (result.compilationErrOutput != "")
				*errorMessage += result.compilationErrOutput;
			else
				*errorMessage += result.compilationStdOutput;
		}
	}
	// if compiled, load dynamic library
	if (resultAll) {
		_cppCompiler->unloadLibrary();
		trace("Loading dynamic library \"" + _outputFilename + "\"");
		resultAll = _cppCompiler->loadLibrary(errorMessage);
		if (!resultAll) {
			*errorMessage += ". Error loading dynamic library.";
		}
	}
	// if dynamic library loaded, then vinculate pointers to loaded functions
	if (resultAll) {
		trace("Vinculating dynamic library functions");
		try {
			void* handle = _cppCompiler->getDynamicLibraryHandler();
			dispatchEvent_SharedLibHandler = (onDispatchEvent_t)dlsym(handle, "onDispatchEvent");
			//initBetweenReplications_SharedLibHandler = (initBetweenReplications_t)dlsym(handle, "initBetweenReplications");
		} catch (...) {
			resultAll = false;
			*errorMessage += "Error vinculating library functions";
		}
	} else {
		dispatchEvent_SharedLibHandler = nullptr;
		//initBetweenReplications_SharedLibHandler = nullptr;
	}
	return resultAll;
}


/*
ParserChangesInformation* DiffEquations::_getParserChangesInformation() {
	ParserChangesInformation* changes = new ParserChangesInformation();
	//@TODO not implemented yet
	changes->getassignments().append("");
	changes->getexpressionProductions().append("");
	changes->getexpressions().append("");
	changes->getfunctionProdutions().append("");
	changes->getassignments().append("");
	changes->getincludes().append("");
	changes->gettokens().append("");
	changes->gettypeObjs().append("");
	return changes;
}
*/

/*
void DiffEquations::_initBetweenReplications() {
	try {
		if (initBetweenReplications_SharedLibHandler != nullptr) {
			initBetweenReplications_SharedLibHandler(_parentModel);
		} else {
			traceError("Could not call function in shared library for "+getName());
		}
	} catch (...) {
		traceError("Could not call function in shared library for "+getName());
	}
}
*/


void DiffEquations::_createInternalAndAttachedData() {
	if (_cppCompiler == nullptr) {
		_cppCompiler = new CppCompiler(_parentModel, getName() + ".CppCompiler");
		_cppCompiler->setSourceFilename(getName() + ".cpp");
		_cppCompiler->setOutputFilename(getName() + ".so");
		_internalDataInsert("CppCompiler", _cppCompiler);
	}
}


/*
void DiffEquations::_addProperty(PropertyBase* property) {
}
*/
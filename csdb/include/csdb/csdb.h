#pragma once
#ifndef _CREDITS_CSDB_H_INCLUDED_
#define _CREDITS_CSDB_H_INCLUDED_

#include "csdb/storage.h"

namespace csdb {

//Initializing the internal storage object
bool init(const Storage::OpenOptions& options, Storage::OpenCallback callback = nullptr);
bool init(const char* path_to_bases, Storage::OpenCallback callback = nullptr);

//Check if internal storage object is initialized
bool isInitialized();

//Closing an internal storage object
void done();

//Last Error Code
Storage::Error lastError();

//Description of the last error
::std::string lastErrorMessage();

//The code of the last error with the database driver
Database::Error dbLastError();

//Description of the last error with the database driver
::std::string dbLastErrorMessage();

//Default storage
Storage defaultStorage();

} //namespace csdb

#endif // _CREDITS_CSDB_H_INCLUDED_

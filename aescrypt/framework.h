#pragma once

// Controls the default COM threading model for ATL modules
#define _ATL_APARTMENT_THREADED

// Prevents ATL from putting everything inside the ATL::namespace automatically
#define _ATL_NO_AUTOMATIC_NAMESPACE

// Controls how CString constructors behave
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

// Suppresses an ATL debug assertion
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW
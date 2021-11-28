#pragma once
#include <windows.h>
#include <iostream>

class FileDeleteToSystem {
	

public:

	bool PrepForExploit();
	int RunExploit();

	bool CheckConfigMsiDirIsRegistered();
	bool CheckConfigMsiIsAccessible();
	bool CheckMsiServiceIsAvailable();
	

};
#pragma once
class Inject
{
public:
	static BOOL InjectDLL(HANDLE hProcess, const std::wstring& dllPath);
};


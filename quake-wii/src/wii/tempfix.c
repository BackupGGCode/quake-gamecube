#include <stdlib.h>

#pragma message TempFix
void __crtmain()
{
	int code;

	__init();
	SYS_PreMain();
	code = main(0, NULL);
	__fini();

	exit(code);
}

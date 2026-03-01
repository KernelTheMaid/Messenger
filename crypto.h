#pragma once

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/ptrace.h>
#include <unistd.h>
#include <cstdlib>
#endif

void antidebug(){
    	#ifdef _WIN32
    	if (IsDebuggerPresent()) {
        	exit(1);
    	}
	#endif
	#ifdef defined(__linux__)
	if(ptrace(PTRACE_TRACEME,0,1,0)<0){
		exit(1);
	}
	#endif
	#ifdef defined(__FreeBSD__)
	if(ptrace(PT_TRACE_ME,0,0,0)<0){
		exit(1);
	}
	#endif
}

#ifndef TARGETSIM_CORE_API_H
#define TARGETSIM_CORE_API_H

#if defined(_WIN32) && !defined(TARGETSIMCORE_STATIC)
	#ifdef TARGETSIMCORE_EXPORTS
		#define TSCORE_API	__declspec(dllexport)
	#else
		#define TSCORE_API	__declspec(dllimport)
	#endif
#else
	#define TSCORE_API
#endif

#endif

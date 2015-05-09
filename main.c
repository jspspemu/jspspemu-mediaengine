#include <stdio.h>
#include "ffmpeg/ffmpeg.h"

#if defined(_MSC_VER)
    //  Microsoft 
        #define EXPORT __declspec(dllexport)
            #define IMPORT __declspec(dllimport)
            #elif defined(_GCC)
                //  GCC
                    #define EXPORT __attribute__((visibility("default")))
                        #define IMPORT
                        #else
                            //  do nothing and hope for the best?
                                #define EXPORT
                                    #define IMPORT
                                        #pragma warning Unknown dynamic link import/export semantics.
                                        #endif

EXPORT int main2() {
	printf("Hello World: %d\n", avformat_version());
	return 0;
}

#ifndef ASSERTION_H
#define ASSERTION_H

#ifdef DEBUG
	#include <iostream>
	#include <cstdlib>
	void dh_assert(const char * const filename, const unsigned int lineNumber); 
	void dh_warn(const char * const filename, const unsigned int lineNumber,
							const char *message);

	#ifndef ASSERT
	#define ASSERT(f) if(!(f)) {dh_assert(__FILE__,__LINE__);}
	#endif

	#ifndef WARN
	#define WARN(f,g) if(!(f)) { dh_warn(__FILE__,__LINE__,g);}
	#endif
	
	inline void dh_assert(const char * const filename, const unsigned int lineNumber) 
	{
		std::cerr << "ASSERTION ERROR!" << std::endl;
		std::cerr << "Filename: " << filename << std::endl;
		std::cerr << "Line number: " << lineNumber << std::endl;

		std::cerr << "Do you wish to continue?(y/n)";
		char y = 'a';
		while (y != 'n' && y != 'y')
			std::cin >> y;

		if (y != 'y')
			exit(1);
	}

	inline void dh_warn(const char * const filename, const unsigned int lineNumber,const char *message) 
	{
		std::cerr << "Warning to programmer." << std::endl;
		std::cerr << "Filename: " << filename << std::endl;
		std::cerr << "Line number: " << lineNumber << std::endl;
	}

	//OpenGL debugging macro
	#if DEBUG
		#define glError() { \
			GLenum err = glGetError(); \
			while (err != GL_NO_ERROR) { \
						fprintf(stderr, "glError: %s caught at %s:%u\n", (char *)gluErrorString(err), __FILE__, __LINE__); \
						err = glGetError(); \
					} \
			std::cerr << "glErr Clean " << __FILE__ << ":" << __LINE__ << std::endl; \
			}
	#else
		#define glError()
	#endif


#else
	#define ASSERT(f)
	#define WARN(f,g) 
#endif

#endif

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <string>
#include <vector>
#include <deque>

class ConfigFile 
{
	std::deque<std::string> recentFiles;
	public:
		void addRecentFile(const std::string &str);

		void getRecentFiles(std::vector<std::string> &filenames) const; 
		void removeRecentFile(const std::string &str);
		bool read();
		bool write();

		unsigned int getMaxHistory() const;
};

#endif

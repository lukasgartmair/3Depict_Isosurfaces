/*
 * configFile.h - Configuration file management header 
 * Copyright (C) 2011  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H


#include "filter.h"

#include <string>
#include <vector>
#include <deque>

//!Startup panel identifiers. 
enum {
	CONFIG_STARTUPPANEL_RAWDATA,
	CONFIG_STARTUPPANEL_CONTROL,
	CONFIG_STARTUPPANEL_PLOTLIST,
	CONFIG_STARTUPPANEL_END_ENUM
};

enum
{
	CONFIG_PANELMODE_NONE,
	CONFIG_PANELMODE_REMEMBER,
	CONFIG_PANELMODE_SPECIFY,
	CONFIG_PANELMODE_END_ENUM
};

enum
{
	CONFIG_ERR_NOFILE=1,
	CONFIG_ERR_BADFILE,
	CONFIG_ERR_NOPARSER
};


class ConfigFile 
{
	private:
		std::deque<std::string> recentFiles;
		vector<Filter *> filterDefaults;
		//!Panel 
		vector<bool> startupPanelView;

		//!Any errors that occur during file IO. Set by class members during read()/write()
		std::string errMessage;

		//!Method for showing/hiding panel at startup
		unsigned int panelMode;

		//!Initial application window size in pixels
		unsigned int initialSizeX,initialSizeY;
		//!Do we have a valid initial app size?
		bool haveIntialAppSize;

		//!Percentile speeds for mouse zoom and move 
		unsigned int mouseZoomRatePercent,mouseMoveRatePercent;

		//!Master allow the program to do stuff online check. This is AND-ed, so cannot override disabled items
		bool allowOnline;

		//!Should the program perform online version number checking?
		bool allowOnlineVerCheck;	
	public:
		ConfigFile(); 
		void addRecentFile(const std::string &str);

		void getRecentFiles(std::vector<std::string> &filenames) const; 
		void removeRecentFile(const std::string &str);
		unsigned int read();
		bool write();

		std::string getErrMessage() const { return errMessage;};

		unsigned int getMaxHistory() const;

		//Get a vector of the default filter pointers
		void getFilterDefaults(vector<Filter* > &defs);
		//Set the default filter pointers (note this will take ownership of the poitner)
		void setFilterDefaults(const vector<Filter* > &defs);

		//Get a clone of the default filter for a given type,
		//even if it is not in the array (use hardcoded)
		Filter *getDefaultFilter(unsigned int type) const;

		//!Return startup status of UI panels
		bool getPanelEnabled(unsigned int panelID) const;
		
		//!Return startup status of UI panels
		void setPanelEnabled(unsigned int panelID,bool enabled, bool permanent=false);

		//!Get the mouse movement rate (for all but zoom)
		unsigned int getMouseMoveRate() const { return mouseMoveRatePercent; }
		//!Get the mouse movement rate for zoom
		unsigned int getMouseZoomRate() const { return mouseZoomRatePercent; }

		//Set the mouse zoom rate(percent)
		void setMouseZoomRate(unsigned int rate) { mouseZoomRatePercent=rate;};
		//Set the mouse move rate (percent)
		void setMouseMoveRate(unsigned int rate) { mouseMoveRatePercent=rate;};

		//!Return the current panelmode
		unsigned int getStartupPanelMode() const;
		void setStartupPanelMode(unsigned int panelM);

		//!Returns true if we have a suggested initial window size; with x & y being the suggestion
		bool getInitialAppSize(unsigned int &x, unsigned int &y) const;
		//!Set the inital window suggested size
		void setInitialAppSize(unsigned int x, unsigned int y);

		bool getAllowOnlineVersionCheck() const;

		void setAllowOnline(bool v);
		void setAllowOnlineVersionCheck(bool v);
};

#endif

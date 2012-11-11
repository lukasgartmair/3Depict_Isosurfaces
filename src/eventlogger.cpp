/*
 * eventlogger.cpp  - Program event logger
 * Copyright (C) 2012 A Ceguerra
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

#ifdef DEBUG

#include "eventlogger.h"

#include <utility>
#include <sstream>
#include <wx/window.h>

#include "wxcommon.h"

using std::pair;
using std::make_pair;

EventLogger::EventLogger(int b, std::ostream *s)
: bitshift(b), eventcount(0), prevevt(-1), ringbuffermax(1 << bitshift), stream(s)
{
    // events to ignore in the logger
    ignoreEvents.insert(wxEVT_HIBERNATE);
    ignoreEvents.insert(wxEVT_IDLE);
    ignoreEvents.insert(wxEVT_PAINT);
    ignoreEvents.insert(wxEVT_ERASE_BACKGROUND);
    ignoreEvents.insert(wxEVT_NC_PAINT);
    ignoreEvents.insert(wxEVT_UPDATE_UI);
    ignoreEvents.insert(wxEVT_SIZE);

    //Causes window name retrieval to crash
    //---
    ignoreEvents.insert(10215); 
    ignoreEvents.insert(wxEVT_SET_CURSOR);
    //---


    //    ignoreEvents.insert(wxEVT_MOTION);//this removed all events
    
    ringbuffer = new std::string[ringbuffermax];
    
    mask = 1;
    for (size_t i = 0; i < bitshift - 1; i++) {
        mask <<= 1;
        mask++;
    }
    pair<wxEventType,std::string> allEvents [] = {		
        make_pair(wxEVT_COMMAND_BUTTON_CLICKED, "wxEVT_COMMAND_BUTTON_CLICKED")
        ,make_pair(wxEVT_COMMAND_CHECKBOX_CLICKED, "wxEVT_COMMAND_CHECKBOX_CLICKED")
        ,make_pair(wxEVT_COMMAND_CHOICE_SELECTED, "wxEVT_COMMAND_CHOICE_SELECTED")
        ,make_pair(wxEVT_COMMAND_LISTBOX_SELECTED, "wxEVT_COMMAND_LISTBOX_SELECTED")
        ,make_pair(wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, "wxEVT_COMMAND_LISTBOX_DOUBLECLICKED")
        ,make_pair(wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, "wxEVT_COMMAND_CHECKLISTBOX_TOGGLED")
        ,make_pair(wxEVT_COMMAND_MENU_SELECTED, "wxEVT_COMMAND_MENU_SELECTED")
        ,make_pair(wxEVT_COMMAND_SLIDER_UPDATED, "wxEVT_COMMAND_SLIDER_UPDATED")
        ,make_pair(wxEVT_COMMAND_RADIOBOX_SELECTED, "wxEVT_COMMAND_RADIOBOX_SELECTED")
        ,make_pair(wxEVT_COMMAND_RADIOBUTTON_SELECTED, "wxEVT_COMMAND_RADIOBUTTON_SELECTED")
        ,make_pair(wxEVT_COMMAND_SCROLLBAR_UPDATED, "wxEVT_COMMAND_SCROLLBAR_UPDATED")
        ,make_pair(wxEVT_COMMAND_VLBOX_SELECTED, "wxEVT_COMMAND_VLBOX_SELECTED")
        ,make_pair(wxEVT_COMMAND_COMBOBOX_SELECTED, "wxEVT_COMMAND_COMBOBOX_SELECTED")
        ,make_pair(wxEVT_COMMAND_TOOL_RCLICKED, "wxEVT_COMMAND_TOOL_RCLICKED")
        //,make_pair(wxEVT_COMMAND_TOOL_DROPDOWN_CLICKED, "wxEVT_COMMAND_TOOL_DROPDOWN_CLICKED")
        ,make_pair(wxEVT_COMMAND_TOOL_ENTER, "wxEVT_COMMAND_TOOL_ENTER")
        //,make_pair(wxEVT_COMMAND_COMBOBOX_DROPDOWN, "wxEVT_COMMAND_COMBOBOX_DROPDOWN")
        //,make_pair(wxEVT_COMMAND_COMBOBOX_CLOSEUP, "wxEVT_COMMAND_COMBOBOX_CLOSEUP")
        //,make_pair(wxEVT_THREAD, "wxEVT_THREAD")
        ,make_pair(wxEVT_LEFT_DOWN, "wxEVT_LEFT_DOWN")
        ,make_pair(wxEVT_LEFT_UP, "wxEVT_LEFT_UP")
        ,make_pair(wxEVT_MIDDLE_DOWN, "wxEVT_MIDDLE_DOWN")
        ,make_pair(wxEVT_MIDDLE_UP, "wxEVT_MIDDLE_UP")
        ,make_pair(wxEVT_RIGHT_DOWN, "wxEVT_RIGHT_DOWN")
        ,make_pair(wxEVT_RIGHT_UP, "wxEVT_RIGHT_UP")
        ,make_pair(wxEVT_MOTION, "wxEVT_MOTION")
        ,make_pair(wxEVT_ENTER_WINDOW, "wxEVT_ENTER_WINDOW")
        ,make_pair(wxEVT_LEAVE_WINDOW, "wxEVT_LEAVE_WINDOW")
        ,make_pair(wxEVT_LEFT_DCLICK, "wxEVT_LEFT_DCLICK")
        ,make_pair(wxEVT_MIDDLE_DCLICK, "wxEVT_MIDDLE_DCLICK")
        ,make_pair(wxEVT_RIGHT_DCLICK, "wxEVT_RIGHT_DCLICK")
        ,make_pair(wxEVT_SET_FOCUS, "wxEVT_SET_FOCUS")
        ,make_pair(wxEVT_KILL_FOCUS, "wxEVT_KILL_FOCUS")
        ,make_pair(wxEVT_CHILD_FOCUS, "wxEVT_CHILD_FOCUS")
        ,make_pair(wxEVT_MOUSEWHEEL, "wxEVT_MOUSEWHEEL")
        //,make_pair(wxEVT_AUX1_DOWN, "wxEVT_AUX1_DOWN")
        //,make_pair(wxEVT_AUX1_UP, "wxEVT_AUX1_UP")
        //,make_pair(wxEVT_AUX1_DCLICK, "wxEVT_AUX1_DCLICK")
        //,make_pair(wxEVT_AUX2_DOWN, "wxEVT_AUX2_DOWN")
        //,make_pair(wxEVT_AUX2_UP, "wxEVT_AUX2_UP")
        //,make_pair(wxEVT_AUX2_DCLICK, "wxEVT_AUX2_DCLICK")
        ,make_pair(wxEVT_CHAR, "wxEVT_CHAR")
        ,make_pair(wxEVT_CHAR_HOOK, "wxEVT_CHAR_HOOK")
        ,make_pair(wxEVT_NAVIGATION_KEY, "wxEVT_NAVIGATION_KEY")
        ,make_pair(wxEVT_KEY_DOWN, "wxEVT_KEY_DOWN")
        ,make_pair(wxEVT_KEY_UP, "wxEVT_KEY_UP")
        //,make_pair(wxEVT_HOTKEY, "wxEVT_HOTKEY")
        ,make_pair(wxEVT_SET_CURSOR, "wxEVT_SET_CURSOR")
        ,make_pair(wxEVT_SCROLL_TOP, "wxEVT_SCROLL_TOP")
        ,make_pair(wxEVT_SCROLL_BOTTOM, "wxEVT_SCROLL_BOTTOM")
        ,make_pair(wxEVT_SCROLL_LINEUP, "wxEVT_SCROLL_LINEUP")
        ,make_pair(wxEVT_SCROLL_LINEDOWN, "wxEVT_SCROLL_LINEDOWN")
        ,make_pair(wxEVT_SCROLL_PAGEUP, "wxEVT_SCROLL_PAGEUP")
        ,make_pair(wxEVT_SCROLL_PAGEDOWN, "wxEVT_SCROLL_PAGEDOWN")
        ,make_pair(wxEVT_SCROLL_THUMBTRACK, "wxEVT_SCROLL_THUMBTRACK")
        ,make_pair(wxEVT_SCROLL_THUMBRELEASE, "wxEVT_SCROLL_THUMBRELEASE")
        ,make_pair(wxEVT_SCROLL_CHANGED, "wxEVT_SCROLL_CHANGED")
        //,make_pair(wxEVT_SPIN_UP, "wxEVT_SPIN_UP")
        //,make_pair(wxEVT_SPIN_DOWN, "wxEVT_SPIN_DOWN")
        //,make_pair(wxEVT_SPIN, "wxEVT_SPIN")
        ,make_pair(wxEVT_SCROLLWIN_TOP, "wxEVT_SCROLLWIN_TOP")
        ,make_pair(wxEVT_SCROLLWIN_BOTTOM, "wxEVT_SCROLLWIN_BOTTOM")
        ,make_pair(wxEVT_SCROLLWIN_LINEUP, "wxEVT_SCROLLWIN_LINEUP")
        ,make_pair(wxEVT_SCROLLWIN_LINEDOWN, "wxEVT_SCROLLWIN_LINEDOWN")
        ,make_pair(wxEVT_SCROLLWIN_PAGEUP, "wxEVT_SCROLLWIN_PAGEUP")
        ,make_pair(wxEVT_SCROLLWIN_PAGEDOWN, "wxEVT_SCROLLWIN_PAGEDOWN")
        ,make_pair(wxEVT_SCROLLWIN_THUMBTRACK, "wxEVT_SCROLLWIN_THUMBTRACK")
        ,make_pair(wxEVT_SCROLLWIN_THUMBRELEASE, "wxEVT_SCROLLWIN_THUMBRELEASE")
        ,make_pair(wxEVT_SIZE, "wxEVT_SIZE")
        ,make_pair(wxEVT_MOVE, "wxEVT_MOVE")
        ,make_pair(wxEVT_CLOSE_WINDOW, "wxEVT_CLOSE_WINDOW")
        ,make_pair(wxEVT_END_SESSION, "wxEVT_END_SESSION")
        ,make_pair(wxEVT_QUERY_END_SESSION, "wxEVT_QUERY_END_SESSION")
        ,make_pair(wxEVT_ACTIVATE_APP, "wxEVT_ACTIVATE_APP")
        ,make_pair(wxEVT_ACTIVATE, "wxEVT_ACTIVATE")
        ,make_pair(wxEVT_CREATE, "wxEVT_CREATE")
        ,make_pair(wxEVT_DESTROY, "wxEVT_DESTROY")
        ,make_pair(wxEVT_SHOW, "wxEVT_SHOW")
        ,make_pair(wxEVT_ICONIZE, "wxEVT_ICONIZE")
        ,make_pair(wxEVT_MAXIMIZE, "wxEVT_MAXIMIZE")
        ,make_pair(wxEVT_MOUSE_CAPTURE_CHANGED, "wxEVT_MOUSE_CAPTURE_CHANGED")
        ,make_pair(wxEVT_MOUSE_CAPTURE_LOST, "wxEVT_MOUSE_CAPTURE_LOST")
        ,make_pair(wxEVT_PAINT, "wxEVT_PAINT")
        ,make_pair(wxEVT_ERASE_BACKGROUND, "wxEVT_ERASE_BACKGROUND")
        ,make_pair(wxEVT_NC_PAINT, "wxEVT_NC_PAINT")
        ,make_pair(wxEVT_MENU_OPEN, "wxEVT_MENU_OPEN")
        ,make_pair(wxEVT_MENU_CLOSE, "wxEVT_MENU_CLOSE")
        ,make_pair(wxEVT_MENU_HIGHLIGHT, "wxEVT_MENU_HIGHLIGHT")
        ,make_pair(wxEVT_CONTEXT_MENU, "wxEVT_CONTEXT_MENU")
        ,make_pair(wxEVT_SYS_COLOUR_CHANGED, "wxEVT_SYS_COLOUR_CHANGED")
        ,make_pair(wxEVT_DISPLAY_CHANGED, "wxEVT_DISPLAY_CHANGED")
        ,make_pair(wxEVT_QUERY_NEW_PALETTE, "wxEVT_QUERY_NEW_PALETTE")
        ,make_pair(wxEVT_PALETTE_CHANGED, "wxEVT_PALETTE_CHANGED")
        ,make_pair(wxEVT_JOY_BUTTON_DOWN, "wxEVT_JOY_BUTTON_DOWN")
        ,make_pair(wxEVT_JOY_BUTTON_UP, "wxEVT_JOY_BUTTON_UP")
        ,make_pair(wxEVT_JOY_MOVE, "wxEVT_JOY_MOVE")
        ,make_pair(wxEVT_JOY_ZMOVE, "wxEVT_JOY_ZMOVE")
        ,make_pair(wxEVT_DROP_FILES, "wxEVT_DROP_FILES")
        ,make_pair(wxEVT_INIT_DIALOG, "wxEVT_INIT_DIALOG")
        ,make_pair(wxEVT_IDLE, "wxEVT_IDLE")
        ,make_pair(wxEVT_UPDATE_UI, "wxEVT_UPDATE_UI")
        ,make_pair(wxEVT_SIZING, "wxEVT_SIZING")
        ,make_pair(wxEVT_MOVING, "wxEVT_MOVING")
        //,make_pair(wxEVT_MOVE_START, "wxEVT_MOVE_START")
        //,make_pair(wxEVT_MOVE_END, "wxEVT_MOVE_END")
        ,make_pair(wxEVT_HIBERNATE, "wxEVT_HIBERNATE")
        ,make_pair(wxEVT_COMMAND_TEXT_COPY, "wxEVT_COMMAND_TEXT_COPY")
        ,make_pair(wxEVT_COMMAND_TEXT_CUT, "wxEVT_COMMAND_TEXT_CUT")
        ,make_pair(wxEVT_COMMAND_TEXT_PASTE, "wxEVT_COMMAND_TEXT_PASTE")
        ,make_pair(wxEVT_COMMAND_LEFT_CLICK, "wxEVT_COMMAND_LEFT_CLICK")
        ,make_pair(wxEVT_COMMAND_LEFT_DCLICK, "wxEVT_COMMAND_LEFT_DCLICK")
        ,make_pair(wxEVT_COMMAND_RIGHT_CLICK, "wxEVT_COMMAND_RIGHT_CLICK")
        ,make_pair(wxEVT_COMMAND_RIGHT_DCLICK, "wxEVT_COMMAND_RIGHT_DCLICK")
        ,make_pair(wxEVT_COMMAND_SET_FOCUS, "wxEVT_COMMAND_SET_FOCUS")
        ,make_pair(wxEVT_COMMAND_KILL_FOCUS, "wxEVT_COMMAND_KILL_FOCUS")
        ,make_pair(wxEVT_COMMAND_ENTER, "wxEVT_COMMAND_ENTER")
        ,make_pair(wxEVT_HELP, "wxEVT_HELP")
        ,make_pair(wxEVT_DETAILED_HELP, "wxEVT_DETAILED_HELP")
        ,make_pair(wxEVT_COMMAND_TEXT_UPDATED, "wxEVT_COMMAND_TEXT_UPDATED")
        //,make_pair(wxEVT_COMMAND_TEXT_ENTER, "wxEVT_COMMAND_TEXT_ENTER")
        ,make_pair(wxEVT_COMMAND_TOOL_CLICKED, "wxEVT_COMMAND_TOOL_CLICKED")
        //,make_pair(wxEVT_WINDOW_MODAL_DIALOG_CLOSED, "wxEVT_WINDOW_MODAL_DIALOG_CLOSED")
        ,make_pair((wxEventType)wxID_ANY, "wxID_ANY")
    };		
    
    for (int i = 0; allEvents[i].first != wxID_ANY; i++) {
        eventmap[allEvents[i].first]= allEvents[i].second;
    }
    
}

EventLogger::~EventLogger()
{
    delete [] ringbuffer;
}

void EventLogger::insert(wxEvent& event)
{
	if (ignoreEvents.count(event.GetEventType())) 
		return;

	std::stringstream ss;
	ss << "evt"<< eventcount<< ":"<< " " << event.GetEventType() 
		<<" " <<  eventmap[event.GetEventType()] << " : ";

	//Obtain the window data 
	wxObject *wxObj = event.GetEventObject();
	if(wxObj && wxObj->IsKindOf(CLASSINFO(wxWindow)))
		ss<< stlStr(((wxWindow *)wxObj)->GetName());

	ringbuffer[eventcount] = ss.str();
	eventcount++;
	eventcount&=mask;
}

void EventLogger::dump()
{
    size_t i = eventcount;
    do {
        *stream << ringbuffer[i&mask] <<std::endl;
        i++;
    } while( (i&mask) != eventcount && ringbuffer[i&mask].length());
}

void EventLogger::setostream(std::ostream *s)
{
    stream = s;
}

EventLogger::EventLogger(EventLogger &e)
: stream(e.stream)
{}
#endif

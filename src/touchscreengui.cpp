/*
Copyright (C) 2013 xyz, Ilya Zhuravlev <whatever@xyz.is>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "touchscreengui.h"
#include "irrlichttypes.h"
#include "irr_v2d.h"
#include "log.h"
#include "keycode.h"
#include "settings.h"
#include "gettime.h"
#include "util/numeric.h"

#include <iostream>
#include <algorithm>

#include <ISceneCollisionManager.h>

using namespace irr::core;

extern Settings *g_settings;

const char** keynames = (const char*[]) {
	"FORWARD",
	"BACKWARD",
	"LEFT",
	"RIGHT",
	"JUMP",
	"INVENTORY",
	"CHAT"
};

const char** eventnames = (const char*[]) {
		"EET_GUI_EVENT",
		"EET_MOUSE_INPUT_EVENT",
		"EET_KEY_INPUT_EVENT",
		"EET_MULTI_TOUCH_EVENT",
		"EET_ACCELEROMETER_EVENT",
		"EET_GYROSCOPE_EVENT",
		"EET_DEVICE_MOTION_EVENT",
		"EET_JOYSTICK_INPUT_EVENT",
		"EET_LOG_TEXT_EVENT"
};

const char** mouseeventnames = (const char*[]) {
		"EMIE_LMOUSE_PRESSED_DOWN",
		"EMIE_RMOUSE_PRESSED_DOWN",
		"EMIE_MMOUSE_PRESSED_DOWN",
		"EMIE_LMOUSE_LEFT_UP",
		"EMIE_RMOUSE_LEFT_UP",
		"EMIE_MMOUSE_LEFT_UP",
		"EMIE_MOUSE_MOVED",
		"EMIE_MOUSE_WHEEL",
		"EMIE_LMOUSE_DOUBLE_CLICK",
		"EMIE_RMOUSE_DOUBLE_CLICK",
		"EMIE_MMOUSE_DOUBLE_CLICK",
		"EMIE_LMOUSE_TRIPLE_CLICK",
		"EMIE_RMOUSE_TRIPLE_CLICK",
		"EMIE_MMOUSE_TRIPLE_CLICK"
};

const char** multitoucheventnames = (const char*[]) {
		"EMTIE_PRESSED_DOWN",
		"EMTIE_LEFT_UP",
		"EMTIE_MOVED",
};

const char** touchgui_button_imagenames = (const char*[]) {
	"up_arrow.png",
	"down_arrow.png",
	"left_arrow.png",
	"right_arrow.png",
	"jump_btn.png",
	"inventory_btn.png",
	"chat_btn.png"
};

#define CALCDELTA(a,b)                                                         \
	unsigned int delta = -1;                                                   \
	if (a > b) {                                                               \
		delta = a - b;                                                         \
	}                                                                          \
	else {                                                                     \
		delta = b - a;                                                         \
	}

static irr::EKEY_CODE id2keycode(touch_gui_button_id id) {
	std::string key = "";
	switch (id) {
		case forward_id:
			key = "forward";
			break;
		case left_id:
			key = "left";
			break;
		case right_id:
			key = "right";
			break;
		case backward_id:
			key = "backward";
			break;
		case jump_id:
			key = "jump";
			break;
		case inventory_id:
			key = "inventory";
			break;
		case chat_id:
			key = "chat";
			break;
	}

	assert(key != "");

	return keyname_to_keycode(g_settings->get("keymap_" + key).c_str());
}

TouchScreenGUI::TouchScreenGUI(IrrlichtDevice *device, IEventReceiver* receiver):
	m_device(device),
	m_guienv(device->getGUIEnvironment()),
	m_camera_yaw(0.0),
	m_camera_pitch(0.0),
	m_hud_start_y(100000),
	m_visible(false),
	m_move_id(-1),
	m_receiver(receiver)
{
	for (unsigned int i=0; i < after_last_element_id; i++) {
		m_buttons[i].guibutton     = 0;
		m_buttons[i].repeatcounter = -1;
	}

	m_screensize = m_device->getVideoDriver()->getScreenSize();
}

void TouchScreenGUI::LoadButtonTexture(button_info* btn, const char* path) {
	unsigned int tid;
	video::ITexture *texture = m_texturesource->getTexture(path,&tid);
	if (texture) {
		btn->guibutton->setUseAlphaChannel(true);
		btn->guibutton->setImage(texture);
		btn->guibutton->setPressedImage(texture);
		btn->guibutton->setScaleImage(true);
		btn->guibutton->setDrawBorder(false);
		btn->guibutton->setText(L"");
		}
}

void TouchScreenGUI::init(ISimpleTextureSource* tsrc) {
	assert(tsrc != 0);
	u32 control_pad_size = (2 * m_screensize.Y) / 3;
	u32 button_size = control_pad_size / 3;
	m_visible = true;
	m_texturesource = tsrc;

	m_control_pad_rect = rect<s32>(0, m_screensize.Y - 3 * button_size, 3 * button_size, m_screensize.Y);
	/*
	draw control pad
	0 1 2
	3 4 5
	for now only 0, 1, 2, and 4 are used
	*/
	int number = 0;
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 3; ++x, ++number) {
			rect<s32> button_rect(
					x * button_size, m_screensize.Y - button_size * (2 - y),
					(x + 1) * button_size, m_screensize.Y - button_size * (1 - y)
			);
			touch_gui_button_id id = after_last_element_id;
			std::wstring caption;
			switch (number) {
			case 0:
				id = left_id;
				caption = L"<";
				break;
			case 1:
				id = forward_id;
				caption = L"^";
				break;
			case 2:
				id = right_id;
				caption = L">";
				break;
			case 4:
				id = backward_id;
				caption = L"v";
				break;
			}
			if (id != after_last_element_id) {
				button_info* btn = &m_buttons[id];
				btn->guibutton     = m_guienv->addButton(button_rect, 0, id, caption.c_str());
				btn->repeatcounter = -1;
				btn->ids.clear();
				btn->keycode       = id2keycode(id);
				btn->immediate_release = false;

				LoadButtonTexture(btn,touchgui_button_imagenames[id]);
				}
		}

	/* init inventory button */
	button_info* btn = &m_buttons[inventory_id];
	btn->guibutton = m_guienv->addButton(
			rect<s32>(0, m_screensize.Y - 50, 50, m_screensize.Y),
			0,
			inventory_id,
			L"inv");
	btn->repeatcounter = -1;
	btn->ids.clear();
	btn->keycode       = id2keycode(inventory_id);
	btn->immediate_release = true;
	LoadButtonTexture(btn,touchgui_button_imagenames[inventory_id]);


	/* init jump button */
	btn = &m_buttons[jump_id];
	btn->guibutton = m_guienv->addButton(
			rect<s32>(m_screensize.X-(1.75*button_size), m_screensize.Y - (0.5*button_size),
					m_screensize.X-(0.25*button_size), m_screensize.Y),
			0,
			jump_id,
			L"x");
	btn->repeatcounter = -1;
	btn->ids.clear();
	btn->keycode       = id2keycode(jump_id);
	btn->immediate_release = false;
	LoadButtonTexture(btn,touchgui_button_imagenames[jump_id]);

	/* init chat button */
	btn = &m_buttons[chat_id];
	btn->guibutton = m_guienv->addButton(
			rect<s32>(0, 0, 50, 50),
			0,
			chat_id,
			L"Chat");
	btn->repeatcounter = -1;
	btn->ids.clear();
	btn->keycode       = id2keycode(chat_id);
	btn->immediate_release = true;
	LoadButtonTexture(btn,touchgui_button_imagenames[chat_id]);
}

touch_gui_button_id TouchScreenGUI::getButtonID(s32 x, s32 y) {

	IGUIElement* rootguielement = m_guienv->getRootGUIElement();

	if (rootguielement != NULL) {
		gui::IGUIElement *element = rootguielement->getElementFromPoint(
								core::position2d<s32>(x,y));

		if (element) {
			for (unsigned int i=0; i < after_last_element_id; i++) {
				if (element == m_buttons[i].guibutton) {
					return (touch_gui_button_id) i;
				}
			}
		}
	}
	return after_last_element_id;
}

touch_gui_button_id TouchScreenGUI::getButtonID(int eventID) {

	for (unsigned int i=0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];

		std::vector<int>::iterator id = std::find(btn->ids.begin(),btn->ids.end(), eventID);

		if (id != btn->ids.end())
			return (touch_gui_button_id) i;
	}

	return after_last_element_id;
}

bool TouchScreenGUI::isHUDButton
(
		const SEvent &event,
		int eventID
) {
	// check if hud item is pressed
	for (int j = 0; j < m_hud_rects.size(); ++j) {
		if (m_hud_rects[j].isPointInside(
				v2s32(event.MultiTouchInput.X[eventID],
						event.MultiTouchInput.Y[eventID])
			)) {
			if ( j < 6) {
				SEvent* translated = new SEvent();
				memset(translated,0,sizeof(SEvent));
				translated->EventType = irr::EET_KEY_INPUT_EVENT;
				translated->KeyInput.Key         = (irr::EKEY_CODE) (KEY_KEY_1 + j);
				translated->KeyInput.Control     = false;
				translated->KeyInput.Shift       = false;
				translated->KeyInput.PressedDown = true;
				m_receiver->OnEvent(*translated);
				m_hud_ids[eventID] = translated->KeyInput.Key;
				errorstream << "TouchScreenGUI::isReleaseHUDButton pushed hud"
						<<" buttonid=" << eventID
						<<" hudid_count=" << m_hud_ids.size()
						<<" keycode=" << translated->KeyInput.Key
						<< std::endl;
				delete translated;
				return true;
			}
		}
	}
	return false;
}

bool TouchScreenGUI::isReleaseHUDButton(int eventID) {

	std::map<int,irr::EKEY_CODE>::iterator iter = m_hud_ids.find(eventID);

	if (iter != m_hud_ids.end()) {
		SEvent* translated = new SEvent();
		memset(translated,0,sizeof(SEvent));
		translated->EventType            = irr::EET_KEY_INPUT_EVENT;
		translated->KeyInput.Key         = iter->second;
		translated->KeyInput.PressedDown = false;
		translated->KeyInput.Control     = false;
		translated->KeyInput.Shift       = false;
		m_receiver->OnEvent(*translated);
		errorstream << "TouchScreenGUI::isReleaseHUDButton released hud"
				<<" buttonid=" << eventID
				<<" hudid_count=" << m_hud_ids.size()
				<<" keycode=" << iter->second
				<< std::endl;
		m_hud_ids.erase(iter);
		delete translated;
		return true;
	}
	errorstream << "TouchScreenGUI::isReleaseHUDButton " << eventID << " isn't a known hud button" << std::endl;
	return false;
}

void TouchScreenGUI::ButtonEvent(touch_gui_button_id button, int eventID, bool action) {
	button_info* btn = &m_buttons[button];
	SEvent* translated = new SEvent();
	memset(translated,0,sizeof(SEvent));
	translated->EventType            = irr::EET_KEY_INPUT_EVENT;
	translated->KeyInput.Key         = btn->keycode;
	translated->KeyInput.Control     = false;
	translated->KeyInput.Shift       = false;
	translated->KeyInput.Char        = 0;

	/* add this event */
	if (action) {
		assert(std::find(btn->ids.begin(),btn->ids.end(), eventID) == btn->ids.end());

		btn->ids.push_back(eventID);

		if (btn->ids.size() > 1) return;

		btn->repeatcounter = 0;
		translated->KeyInput.PressedDown = true;
		translated->KeyInput.Key = btn->keycode;
		m_receiver->OnEvent(*translated);
	}
	/* remove event */
	if ((!action) || (btn->immediate_release)) {

		std::vector<int>::iterator pos = std::find(btn->ids.begin(),btn->ids.end(), eventID);

		/* has to be in touch list */
		assert(pos != btn->ids.end());

		btn->ids.erase(pos);

		if (btn->ids.size() > 0) return;

		translated->KeyInput.PressedDown = false;

		btn->repeatcounter = -1;
		m_receiver->OnEvent(*translated);
	}
	delete translated;
}

void TouchScreenGUI::translateEvent(const SEvent &event) {
	if (!m_visible) {
		errorstream << "TouchScreenGUI::translateEvent got event but not visible?!" << std::endl;
		return;
	}

	if (event.EventType != EET_MULTI_TOUCH_EVENT) {
		return;
	}

	std::vector<id_status> current_ids;
	for (unsigned int i=0; i < NUMBER_OF_MULTI_TOUCHES; i++) {
		if (event.MultiTouchInput.Touched[i]) {
			id_status toadd;
			toadd.id = event.MultiTouchInput.ID[i];
			toadd.X = event.MultiTouchInput.X[i];
			toadd.Y = event.MultiTouchInput.Y[i];
			current_ids.push_back(toadd);
		}
	}

	infostream << "TouchScreenGUI::translateEvent "
			<< "event=" << multitoucheventnames[event.MultiTouchInput.Event] << " "
			<< "changedPointer=" << event.MultiTouchInput.ChangedPointerID << " "
			<< "m_moved_id=" << m_move_id << " "
			<< "last_touch_count=" << m_known_ids.size() << " "
			<< "current_touch_count=" << current_ids.size() << " "
			<< "fwd_cnt=" << m_buttons[forward_id].ids.size() << " "
			<< "back_cnt=" << m_buttons[backward_id].ids.size() << " "
			<< "left_cnt=" << m_buttons[left_id].ids.size() << " "
			<< "right_cnt=" << m_buttons[right_id].ids.size() << " "
			<< "jump_cnt=" << m_buttons[jump_id].ids.size() << " "
			<< std::endl;

	if (event.MultiTouchInput.Event == EMTIE_PRESSED_DOWN) {
		int eventID = event.MultiTouchInput.ChangedPointerID;
		int eventIndex = -1;

		/* read index of changed id */
		for (unsigned int i=0; i < event.MultiTouchInput.touchedCount(); i ++) {
			if (event.MultiTouchInput.ID[i] == eventID) {
				eventIndex = i;
				break;
			}
		}

		assert (eventIndex != -1);

		touch_gui_button_id button =
				getButtonID(
						event.MultiTouchInput.X[eventIndex],
						event.MultiTouchInput.Y[eventIndex]
						);

		/* handle button events */
		if (button != after_last_element_id) {
			ButtonEvent(button,eventID,true);
		}
		else if (isHUDButton(event,eventID))
		{
			/* already handled in isHUDButton() */
		}
		/* handle non button events */
		else {
			/* if we don't already have a moving point make this the moving one */
			if (m_move_id == -1) {
				m_move_id = event.MultiTouchInput.ID[eventIndex];
				m_move_has_really_moved = false;
				m_move_downtime = getTimeMs();
				m_move_downlocation = v2s32(event.MultiTouchInput.X[eventIndex],
											event.MultiTouchInput.Y[eventIndex]);
				m_move_sent_as_mouse_event = false;
			}
		}
		m_known_ids = current_ids;
	}
	else if (event.MultiTouchInput.Event == EMTIE_LEFT_UP) {
		int eventID = findVanishedID(event);

		/* maybe a new touch occured while we haven't handled vanishing of last old one */
		if ((eventID == -1) && (m_known_ids.size() == 1)) {
			errorstream << "TouchScreenGUI::translateEvent new event appeared prior handling of vanished one" << std::endl;
			eventID = m_known_ids[0].id;
			current_ids.clear();
		}

		assert((eventID != -1) || ("TouchScreenGUI::translateEvent didn't find a vanished ID" == 0));

		touch_gui_button_id button = getButtonID(eventID);

		/* handle button events */
		if (button != after_last_element_id) {
			ButtonEvent(button,eventID,false);
		}
		/* handle hud button events */
		else if (isReleaseHUDButton(eventID)) {
			/* nothing to do here */
		}
		/* handle the point used for moving view */
		else if (eventID == m_move_id) {
			m_move_id = -1;

			/* if this pointer issued a mouse event issue symmetric release here */
			if (m_move_sent_as_mouse_event) {
				SEvent* translated = new SEvent;
				memset(translated,0,sizeof(SEvent));
				translated->EventType = EET_MOUSE_INPUT_EVENT;
				translated->MouseInput.X = m_move_downlocation.X;
				translated->MouseInput.Y = m_move_downlocation.Y;
				translated->MouseInput.Shift = false;
				translated->MouseInput.Control = false;
				translated->MouseInput.ButtonStates = 0;
				translated->MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
				infostream << "TouchScreenGUI::step left click release" << std::endl;
				m_receiver->OnEvent(*translated);
				delete translated;
			}
			else {
				/* do double tap detection */
				doubleTapDetection();
			}
		}
		else {
			infostream << "TouchScreenGUI::translateEvent released unknown button: " << eventID << std::endl;
		}
		m_known_ids = current_ids;
	}
	else {
		assert(event.MultiTouchInput.Event == EMTIE_MOVED);
		int move_idx = -1;
		for ( unsigned int i=0; i < event.MultiTouchInput.touchedCount(); i++) {
			if (event.MultiTouchInput.ID[i] == m_move_id) {
				move_idx = i;
				break;
			}
		}

		if (m_move_id != -1) {
			errorstream << "TouchScreenGUI::translateEvent moved move_id " << std::endl;
			if (((move_idx != -1) && isChangedIDX(event,move_idx)) &&
				(!m_move_sent_as_mouse_event)) {
				m_move_has_really_moved = true;
				s32 X = event.MultiTouchInput.X[move_idx];
				s32 Y = event.MultiTouchInput.Y[move_idx];

				// update camera_yaw and camera_pitch
				s32 dx = X - event.MultiTouchInput.PrevX[move_idx];
				s32 dy = Y - event.MultiTouchInput.PrevY[move_idx];

				/* adapt to similar behaviour as pc screen */
				double d = g_settings->getFloat("mouse_sensitivity") *4;

				double old_yaw = m_camera_yaw;
				double old_pitch = m_camera_pitch;

				m_camera_yaw -= dx * d;
				m_camera_pitch = MYMIN(MYMAX( m_camera_pitch + (dy * d),-180),180);

				while (m_camera_yaw < 0)
					m_camera_yaw += 360;

				while (m_camera_yaw > 360)
					m_camera_yaw -= 360;

				infostream << "TouchScreenGUI::translateEvent changing camera:"
							<< " dx=" << dx
							<< " dy=" << dy
							<< " old_yaw=" << old_yaw
							<< " old_pitch=" << old_pitch
							<< " new_yaw=" << m_camera_yaw
							<< " new_pitch=" << m_camera_pitch << std::endl;

				// update shootline
				m_shootline = m_device
						->getSceneManager()
						->getSceneCollisionManager()
						->getRayFromScreenCoordinates(v2s32(X, Y));
			}
		}
		else {
			handleChangedButton(event);
		}
	}
}

void TouchScreenGUI::handleChangedButton(const SEvent &event) {

	for (unsigned int i = 0; i < after_last_element_id; i++) {

		if (m_buttons[i].ids.empty()) {
			continue;
		}
		for(std::vector<int>::iterator iter = m_buttons[i].ids.begin();
				iter != m_buttons[i].ids.end(); iter++) {

			for (unsigned int j = 0; j < NUMBER_OF_MULTI_TOUCHES; j++) {
				if ( (event.MultiTouchInput.Touched[j]) &&
						(*iter == event.MultiTouchInput.ID[j]) ) {

					int current_button_id = getButtonID(
										event.MultiTouchInput.X[j],
										event.MultiTouchInput.Y[j]);

					if (current_button_id == i) {
						continue;
					}

					/* remove old button */
					ButtonEvent((touch_gui_button_id) i,*iter,false);

					if (current_button_id == after_last_element_id) {
						return;
					}
					ButtonEvent((touch_gui_button_id) current_button_id,*iter,true);
					return;
				}
			}
		}
	}
}

bool TouchScreenGUI::doubleTapDetection() {
	m_key_events[0].down_time = m_key_events[1].down_time;
	m_key_events[0].x = m_key_events[1].x;
	m_key_events[0].y = m_key_events[1].y;

	m_key_events[1].down_time = m_move_downtime;
	m_key_events[1].x = m_move_downlocation.X;
	m_key_events[1].y = m_move_downlocation.Y;

	CALCDELTA(m_key_events[0].down_time,getTimeMs())
	errorstream << "TouchScreenGUI::doubleTapDetection delta=" << delta << std::endl;
	if (delta < 400) {
		double distance = sqrt(
						(m_key_events[0].x - m_key_events[1].x) * (m_key_events[0].x - m_key_events[1].x) +
						(m_key_events[0].y - m_key_events[1].y) * (m_key_events[0].y - m_key_events[1].y));
		errorstream << "TouchScreenGUI::doubleTapDetection distance=" << distance << std::endl;
		if (distance < 20) {
			SEvent* translated = new SEvent();
			memset(translated,0,sizeof(SEvent));
			translated->EventType = EET_MOUSE_INPUT_EVENT;
			translated->MouseInput.X = m_key_events[0].x;
			translated->MouseInput.Y = m_key_events[0].y;
			translated->MouseInput.Shift = false;
			translated->MouseInput.Control = false;
			translated->MouseInput.ButtonStates = EMBSM_RIGHT;

			// update shootline
			m_shootline = m_device
					->getSceneManager()
					->getSceneCollisionManager()
					->getRayFromScreenCoordinates(v2s32(m_key_events[0].x, m_key_events[0].y));

			translated->MouseInput.Event = EMIE_RMOUSE_PRESSED_DOWN;
			infostream << "TouchScreenGUI::translateEvent right click press" << std::endl;
			m_receiver->OnEvent(*translated);

			translated->MouseInput.ButtonStates = 0;
			translated->MouseInput.Event = EMIE_RMOUSE_LEFT_UP;
			infostream << "TouchScreenGUI::translateEvent right click release" << std::endl;
			m_receiver->OnEvent(*translated);
			delete translated;
			return true;
		}
	}

	return false;
}

int TouchScreenGUI::findChangedIdx(const SEvent &event) {
	for (unsigned int i=0; i < event.MultiTouchInput.touchedCount(); i ++) {

		bool changed = isChangedIDX(event,i);

		if (changed) {
			return i;
		}
	}
	return -1;
}

bool TouchScreenGUI::isChangedIDX(const SEvent &event, int idx) {

	if (
		(event.MultiTouchInput.PrevX[idx] != event.MultiTouchInput.X[idx]) ||
		(event.MultiTouchInput.PrevX[idx] != event.MultiTouchInput.X[idx])
	) {
		return true;
	}
	return false;
}

int TouchScreenGUI::findVanishedID(const SEvent &event) {

	for (std::vector<id_status>::iterator iter = m_known_ids.begin();
			iter != m_known_ids.end(); iter++) {

		bool found = true;
		for (unsigned int i = 0; i < NUMBER_OF_MULTI_TOUCHES; i++) {

			if (event.MultiTouchInput.ID[i] == iter->id) {
				found = true;
				if (!event.MultiTouchInput.Touched[i]) {
					return iter->id;
				}
				break;
			}
		}
		if (!found) return iter->id;
	}
	verbosestream << "TouchScreenGUI::findVanishedID couldn't find a vanished ID" << std::endl;
	return -1;
}


//	/* neccessary to be able to modify the broken prevX/prevY values */
//	SEvent event;
//	memcpy(&event,&_event,sizeof(SEvent));
//
//	if (event.EventType == EET_MULTI_TOUCH_EVENT) {
//		int changed = getChangedTouch(&(event.MultiTouchInput));
//		s32 x = 0;
//		s32 y = 0;
//		/* first store state */
//		if (changed != -1) {
//			x = event.MultiTouchInput.X[changed];
//			y = event.MultiTouchInput.Y[changed];
//
//			if (event.MultiTouchInput.Event == EMTIE_PRESSED_DOWN) {
//				assert(m_known_touches[changed].is_down == false);
//				m_known_touches[changed].is_down = true;
//				m_known_touches[changed].down_location = v2s32(x,y);
//				m_known_touches[changed].down_time = getTimeMs();
//				m_known_touches[changed].not_sent = true;
//				m_known_touches[changed].moved = false;
//				m_known_touches[changed].last_pos = v2s32(x,y);
//				m_known_touches[changed].is_guibutton = false;
//			}
//			else if (event.MultiTouchInput.Event == EMTIE_LEFT_UP) {
//				assert(m_known_touches[changed].is_down == true);
//				m_known_touches[changed].is_down = false;
//				/* reset moving index */
//				if (changed == m_touch_moving_idx) {
//					m_touch_moving_idx = -1;
//				}
//			}
//		}
//
//		verbosestream << "TouchScreenGUI::translateEvent changed="
//				<< changed << " moving=" << m_touch_moving_idx
//				<< " event=" << multitoucheventnames[event.MultiTouchInput.Event] << std::endl;
//
//		/* case changed is valid and it isn't the pressed down button */
//		if ((changed != -1) && (changed != m_touch_moving_idx)) {
//
//			if (handleGUIButtons(x,y,receiver,event.MultiTouchInput.Event,changed)) {
//				return;
//			}
//
////			if (handleHUD(x,y,receiver,event.MultiTouchInput.Event,changed)) {
////				m_known_touches[changed].is_guibutton = true;
////				return;
////			}
//
////			/* detect single clicks */
////			if (event.MultiTouchInput.Event == EMTIE_LEFT_UP) {
////
////				/* this is a move touch, don't sent press events */
////				if ((m_known_touches[changed].moved) &&
////						(m_known_touches[changed].not_sent)) {
////					return;
////				}
////
////				SEvent translated;
////				translated.EventType = EET_MOUSE_INPUT_EVENT;
////				translated.MouseInput.X = m_known_touches[changed].down_location.X;
////				translated.MouseInput.Y = m_known_touches[changed].down_location.Y;
////				translated.MouseInput.Shift = false;
////				translated.MouseInput.Control = false;
////				translated.MouseInput.ButtonStates = EMBSM_LEFT;
////
////				// update shootline
////				m_shootline = m_device
////						->getSceneManager()
////						->getSceneCollisionManager()
////						->getRayFromScreenCoordinates(v2s32(x, y));
////
////				if (m_known_touches[changed].not_sent) {
////					translated.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
////					infostream << "TouchScreenGUI::translateEvent left click press" << std::endl;
////					receiver->OnEvent(translated);
////				}
////
////				translated.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
////				translated.MouseInput.ButtonStates = 0;
////				infostream << "TouchScreenGUI::translateEvent left click release" << std::endl;
////				receiver->OnEvent(translated);
////				return;
////			}
//		}
//
//		if ((changed != -1) &&
//			(event.MultiTouchInput.Event == EMTIE_MOVED) &&
//			(!m_known_touches[changed].is_guibutton)) {
//			m_known_touches[changed].moved = true;
//
//			if ( m_touch_moving_idx == -1 ) {
//				m_touch_moving_idx = changed;
//			}
//
//			if ( m_touch_moving_idx == changed ) {
//				x = event.MultiTouchInput.X[changed];
//				y = event.MultiTouchInput.Y[changed];
//
//				verbosestream << "TouchScreenGUI::translateEvent new_pos=("
//						<< x << "," << y << ")"
//						<< " old_pos=(" << event.MultiTouchInput.PrevX[changed]
//						<< "," << event.MultiTouchInput.PrevY[changed] << ")"
//						<< std::endl;
//

//				return;
//			}
//		}
//	}
//
//}

TouchScreenGUI::~TouchScreenGUI() {}

//void TouchScreenGUI::OnEvent(const SEvent &event) {
//	if (event.EventType == EET_KEY_INPUT_EVENT) {
//		errorstream << "KeyInputEvent" << std::endl;
//	}
//
//	if (!m_visible) {
//		errorstream << "TouchscreenGUI: got event but not visible?!" << std::endl;
//		return;
//	}
//
//	if (event.EventType == EET_MULTI_TOUCH_EVENT) {
//		keyIsDown.unset(getKeySetting("keymap_forward"));
//		keyIsDown.unset(getKeySetting("keymap_backward"));
//		keyIsDown.unset(getKeySetting("keymap_left"));
//		keyIsDown.unset(getKeySetting("keymap_right"));
//		keyIsDown.unset(getKeySetting("keymap_jump"));
//
//		int changed = getChangedTouch((u8*) event.MultiTouchInput.Touched);
//		bool is_doubleclick = false;
//		bool is_rightclick = false;
//
//
//		/* reset up event for handled button */
//		if ((changed == m_button_down_idx) &&
//				(event.MultiTouchInput.Event == EMTIE_LEFT_UP))
//		{
//			m_button_down_idx = -1;
//			m_down_since = 0;
//			return;
//		}
//
//
//		/* doubleclick detection */
//		if (event.MultiTouchInput.Event == EMTIE_PRESSED_DOWN)
//		{
//			m_key_events[0].down_time = m_key_events[1].down_time;
//			m_key_events[0].x         = m_key_events[1].x;
//			m_key_events[0].y         = m_key_events[1].y;
//
//			m_key_events[1].x = event.MultiTouchInput.X[changed];
//			m_key_events[1].y = event.MultiTouchInput.Y[changed];
//			m_key_events[1].down_time = getTimeMs();
//
//			unsigned int delta = -1;
//
//			if (m_key_events[1].down_time > m_key_events[0].down_time) {
//				delta = m_key_events[1].down_time - m_key_events[0].down_time;
//			}
//			else {
//				delta = m_key_events[0].down_time - m_key_events[1].down_time;
//			}
//
//			if ((delta > 0 ) && (delta < 50)) {
//				double distance = sqrt(
//						(m_key_events[0].x - m_key_events[1].x) * (m_key_events[0].x - m_key_events[1].x) +
//						(m_key_events[0].y - m_key_events[1].y) * (m_key_events[0].y - m_key_events[1].y));
//				if (distance < 20) {
//					is_doubleclick = true;
//				}
//			}
//		}
//
//		if (is_doubleclick) {
//
//			errorstream << "Doubleclick detected at: x=" << m_key_events[0].x
//							<< ", y=" << m_key_events[0].y << std::endl;
//			gui::IGUIElement *hovered =
//				m_guienv->getRootGUIElement()->getElementFromPoint(
//						core::position2d<s32>(m_key_events[0].x,m_key_events[0].y));
//
//			if (hovered) {
//				SEvent translated;
//				translated.EventType = EET_MOUSE_INPUT_EVENT;
//				translated.MouseInput.Event = EMIE_LMOUSE_DOUBLE_CLICK;
//				translated.MouseInput.X = m_key_events[0].x;
//				translated.MouseInput.Y = m_key_events[0].y;
//				translated.MouseInput.ButtonStates = EMBSM_LEFT;
//
//				hovered->OnEvent(translated);
//				return;
//			}
//		}
//
//		/* rightclick detection */
//		/* a rightclick is issued by two finger gesture, put one finger to the
//		 * thing you want to click, then do a short tap next to first finger
//		 */
//		if ((m_button_down_idx != -1) && (changed != -1)){
//
//			if (event.MultiTouchInput.Event == EMTIE_PRESSED_DOWN) {
//				m_tab_time = getTimeMs();
//			}
//
//			if (event.MultiTouchInput.Event == EMTIE_LEFT_UP) {
//				unsigned int delta = -1;
//				u32 now = getTimeMs();
//
//				if (now > m_tab_time) {
//					delta = now - m_tab_time;
//				}
//				else {
//					delta = m_tab_time - now;
//				}
//
//				if (delta < 250) {
//					double distance = sqrt(
//							(m_down_from.X - event.MultiTouchInput.X[changed]) * (m_down_from.X - event.MultiTouchInput.X[changed]) +
//							(m_down_from.Y - event.MultiTouchInput.Y[changed]) * (m_down_from.Y - event.MultiTouchInput.Y[changed]));
//
//					if (distance < 100) {
//						is_rightclick = true;
//						m_button_down_idx = -1;
//						m_down_since = 0;
//					}
//				}
//			}
//		}
//
//		if (is_rightclick) {
//			errorstream << "Rightclick detected at: x=" << m_key_events[0].x
//							<< ", y=" << m_key_events[0].y << std::endl;
//		}
//
//
//		for (int i = 0; i < event.MultiTouchInput.touchedCount(); ++i) {
//
//
//				// check if hud item is pressed
//				for (int j = 0; j < m_hud_rects.size(); ++j) {
//					if (m_hud_rects[j].isPointInside(v2s32(x, y))) {
//						m_player_item = j;
//						m_player_item_changed = true;
//						gui_button = true;
//						break;
//					}
//				}
//			}
//		}
//	}
//}



void TouchScreenGUI::step(float dtime) {

	/* simulate keyboard repeats */
	for (unsigned int i=0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];

		if (btn->ids.size() > 0) {
			btn->repeatcounter += dtime;

			if (btn->repeatcounter < 0.2) continue;

			btn->repeatcounter = 0;
			SEvent translated;
			memset(&translated,0,sizeof(SEvent));
			translated.EventType = irr::EET_KEY_INPUT_EVENT;
			translated.KeyInput.Key = btn->keycode;
			translated.KeyInput.PressedDown = false;
			m_receiver->OnEvent(translated);

			translated.KeyInput.PressedDown = true;
			m_receiver->OnEvent(translated);
		}
	}

	/* if a new placed pointer isn't moved for some time start digging */
	if ((m_move_id != -1) &&
			(!m_move_has_really_moved) &&
			(!m_move_sent_as_mouse_event)) {
		CALCDELTA(m_move_downtime,getTimeMs())

		if (delta > MIN_DIG_TIME_MS) {
			m_shootline = m_device
					->getSceneManager()
					->getSceneCollisionManager()
					->getRayFromScreenCoordinates(
							v2s32(m_move_downlocation.X,m_move_downlocation.Y));

			SEvent translated;
			memset(&translated,0,sizeof(SEvent));
			translated.EventType = EET_MOUSE_INPUT_EVENT;
			translated.MouseInput.X = m_move_downlocation.X;
			translated.MouseInput.Y = m_move_downlocation.Y;
			translated.MouseInput.Shift = false;
			translated.MouseInput.Control = false;
			translated.MouseInput.ButtonStates = EMBSM_LEFT;
			translated.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
			infostream << "TouchScreenGUI::step left click press" << std::endl;
			m_receiver->OnEvent(translated);
			m_move_sent_as_mouse_event = true;
		}
	}
}

void TouchScreenGUI::resetHud() {
	m_hud_rects.clear();
	m_hud_start_y = 100000;
}

void TouchScreenGUI::registerHudItem(int index, const rect<s32> &rect) {
	m_hud_start_y = std::min((int)m_hud_start_y, rect.UpperLeftCorner.Y);
	m_hud_rects.push_back(rect);
}

s32 TouchScreenGUI::getHotbarImageSize() {
	return m_screensize.Y / 10;
}

void TouchScreenGUI::Toggle(bool visible) {
	m_visible = visible;
	for (unsigned int i=0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];

		if (btn->guibutton != 0) {
			btn->guibutton->setVisible(visible);
		}
	}
}

void TouchScreenGUI::Hide() {
	Toggle(false);
}

void TouchScreenGUI::Show() {
	Toggle(true);
}

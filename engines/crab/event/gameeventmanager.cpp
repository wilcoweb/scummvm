/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
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
 *
 */

/*
 * This code is based on the CRAB engine
 *
 * Copyright (c) Arvind Raja Yadav
 *
 * Licensed under MIT
 *
 */

#include "crab/event/gameeventmanager.h"

namespace Crab {

using namespace pyrodactyl::people;
using namespace pyrodactyl::event;
using namespace pyrodactyl::level;
using namespace pyrodactyl::image;
using namespace pyrodactyl::ui;

void Manager::init() {
	_eventMap.clear();
	_activeSeq = UINT_MAX;
	_curSp = nullptr;
	_player = false;
	_curEvent = nullptr;
	_drawGame = true;
}

//------------------------------------------------------------------------
// Purpose: Load this
//------------------------------------------------------------------------
void Manager::load(rapidxml::xml_node<char> *node, ParagraphData &popup) {
	if (nodeValid(node)) {
		XMLDoc conf(node->first_attribute("list")->value());
		if (conf.ready()) {
			rapidxml::xml_node<char> *lnode = conf.doc()->first_node("event_list");
			for (rapidxml::xml_node<char> *loc = lnode->first_node("loc"); loc != nullptr; loc = loc->next_sibling("loc")) {
				Common::String locName;
				loadStr(locName, "name", loc);

				for (auto n = loc->first_node("file"); n != nullptr; n = n->next_sibling("file")) {
					uint id;
					Common::String path;
					loadNum(id, "name", n);
					loadStr(path, "path", n);
					_eventMap[locName].addSeq(id, path);
				}
			}
		}

		_activeSeq = UINT_MAX;

		conf.load(node->first_attribute("layout")->value());
		if (conf.ready()) {
			rapidxml::xml_node<char> *layout = conf.doc()->first_node("layout");
			if (nodeValid(layout)) {
				if (nodeValid("character", layout))
					_oh.load(layout->first_node("character"));

				if (nodeValid("popup", layout))
					popup.load(layout->first_node("popup"));

				if (nodeValid("intro", layout))
					_intro.load(layout->first_node("intro"));
			}
		}

		_reply.load(node->first_attribute("conversation")->value());

		_per.load(node->first_attribute("char")->value());
	}
}

//------------------------------------------------------------------------
// Purpose: Handle events
//------------------------------------------------------------------------
void Manager::handleEvents(Info &info, const Common::String &playerId, Common::Event &event, HUD &hud, Level &level, Common::Array<EventResult> &result) {
	// If an event is already being performed
	if (_eventMap.contains(info.curLocID()) && _eventMap[info.curLocID()].eventInProgress(_activeSeq)) {
		switch (_curEvent->_type) {
		case EVENT_DIALOG:
			if (_oh._showJournal) {
				info._journal.handleEvents(playerId, event);

				if (hud._back.handleEvents(event) == BUAC_LCLICK || hud._pausekey.handleEvents(event))
					_oh._showJournal = false;
			} else {
				// If journal button is select from within an event, go to the entry corresponding to that person's name
				if (_oh.handleCommonEvents(event)) {
					if (info.personValid(_curEvent->_title)) {
						Person &p = info.personGet(_curEvent->_title);
						if (p._altJournalName)
							info._journal.open(playerId, JE_PEOPLE, p._journalName);
						else
							info._journal.open(playerId, JE_PEOPLE, p._name);
					}
				}

				if (_oh.handleDlboxEvents(event)) {
					_eventMap[info.curLocID()].nextEvent(_activeSeq, info, playerId, result, _endSeq);
					_oh._showJournal = false;
				}
			}
			break;
		case EVENT_ANIM:
			// Skip animation if key pressed or mouse pressed
			if (event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_RBUTTONUP)
				_eventMap[info.curLocID()].nextEvent(_activeSeq, info, playerId, result, _endSeq);
			break;
		case EVENT_REPLY:
			if (_oh._showJournal) {
				info._journal.handleEvents(playerId, event);

				if (hud._back.handleEvents(event) == BUAC_LCLICK || hud._pausekey.handleEvents(event))
					_oh._showJournal = false;
			} else {
				// If journal button is select from within an event, go to the entry corresponding to that person's name
				if (_oh.handleCommonEvents(event))
					if (info.personValid(_curEvent->_title))
						info._journal.open(playerId, JE_PEOPLE, info.personGet(_curEvent->_title)._name);

				int choice = _reply.handleEvents(info, g_engine->_eventStore->_con[_curEvent->_special], _curEvent->_title, _oh, event);
				if (choice >= 0) {
					_eventMap[info.curLocID()].nextEvent(_activeSeq, info, playerId, result, _endSeq, choice);
					_oh._showJournal = false;
				}
			}
			break;
		case EVENT_TEXT:
			// If journal button is select from within an event, go to the entry corresponding to that person's name
			if (_oh.handleCommonEvents(event))
				if (info.personValid(_curEvent->_title))
					info._journal.open(playerId, JE_PEOPLE, info.personGet(_curEvent->_title)._name);

			if (_textin.handleEvents(event))
				_eventMap[info.curLocID()].nextEvent(_activeSeq, info, playerId, result, _endSeq);
			break;
		case EVENT_SPLASH:
			if (_intro._showTraits) {
				_per.handleEvents(info, _curEvent->_title, event);

				if (hud._back.handleEvents(event) == BUAC_LCLICK || hud._pausekey.handleEvents(event))
					_intro._showTraits = false;
			} else {
				if (_intro.handleEvents(event))
					_eventMap[info.curLocID()].nextEvent(_activeSeq, info, playerId, result, _endSeq);

				if (_intro._showTraits)
					_per.Cache(info, level.playerId(), level);
			}
			break;
		default:
			break;
		}

		endSequence(info.curLocID());
	}
}

//------------------------------------------------------------------------
// Purpose: Internal Events
//------------------------------------------------------------------------
void Manager::internalEvents(Info &info, Level &level, Common::Array<EventResult> &result) {
	if (_eventMap.contains(info.curLocID())) {
		if (_eventMap[info.curLocID()].eventInProgress(_activeSeq)) {
			switch (_curEvent->_type) {
			case EVENT_DIALOG:
				updateDialogBox(info, level);
				break;
			case EVENT_ANIM: {
				using namespace pyrodactyl::anim;

				DrawType drawVal = DRAW_SAME;
				if (g_engine->_eventStore->_anim[_curEvent->_special].internalEvents(drawVal))
					_eventMap[info.curLocID()].nextEvent(_activeSeq, info, level.playerId(), result, _endSeq);

				if (drawVal == DRAW_STOP)
					_drawGame = false;
				else if (drawVal == DRAW_START)
					_drawGame = true;
			} break;
			case EVENT_SILENT:
				_eventMap[info.curLocID()].nextEvent(_activeSeq, info, level.playerId(), result, _endSeq);
				break;
			case EVENT_REPLY:
				updateDialogBox(info, level);
				break;
			case EVENT_SPLASH:
				updateDialogBox(info, level);
				break;
			default:
				break;
			}

			endSequence(info.curLocID());
		} else {
			_eventMap[info.curLocID()].internalEvents(info);
			calcActiveSeq(info, level, level.camera());
		}
	}
}

void Manager::updateDialogBox(Info &info, pyrodactyl::level::Level &level) {
	_oh.internalEvents(_curEvent->_state, _curSp);
}
//------------------------------------------------------------------------
// Purpose: Draw
//------------------------------------------------------------------------
void Manager::draw(Info &info, HUD &hud, Level &level) {
	if (_eventMap.contains(info.curLocID()) && _eventMap[info.curLocID()].eventInProgress(_activeSeq)) {
		switch (_curEvent->_type) {
		case EVENT_ANIM:
			g_engine->_eventStore->_anim[_curEvent->_special].draw();
			break;
		case EVENT_DIALOG:
			g_engine->_imageManager->dimScreen();
			if (_oh._showJournal) {
				info._journal.draw(level.playerId());
				hud._back.draw();
			} else
				_oh.draw(info, _curEvent, _curEvent->_title, _player, _curSp);
			break;
		case EVENT_REPLY:
			g_engine->_imageManager->dimScreen();
			if (_oh._showJournal) {
				info._journal.draw(level.playerId());
				hud._back.draw();
			} else {
				_oh.draw(info, _curEvent, _curEvent->_title, _player, _curSp);
				_reply.draw();
			}
			break;
		case EVENT_TEXT:
			_oh.draw(info, _curEvent, _curEvent->_title, _player, _curSp);
			_textin.draw();
			break;
		case EVENT_SPLASH:
			g_engine->_imageManager->dimScreen();
			if (_intro._showTraits) {
				_per.draw(info, _curEvent->_title);
				hud._back.draw();
			} else
				_intro.draw(info, _curEvent->_dialog, _curSp, _curEvent->_state);

			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------
// Purpose: Calculate the current sequence in progress
//------------------------------------------------------------------------
void Manager::calcActiveSeq(Info &info, pyrodactyl::level::Level &level, const Rect &camera) {
	if (_eventMap[info.curLocID()].activeSeq(_activeSeq)) {
		// Set all the pointers to the new values
		_curEvent = _eventMap[info.curLocID()].curEvent(_activeSeq);
		_oh.reset(_curEvent->_title);
		_curSp = level.getSprite(_curEvent->_title);

		// The player character's dialog is drawn a bit differently compared to others
		_player = (_curEvent->_title == level.playerId());

		switch (_curEvent->_type) {
		case EVENT_ANIM:
			g_engine->_eventStore->_anim[_curEvent->_special].start();
			break;
		case EVENT_REPLY:
			_reply.cache(info, g_engine->_eventStore->_con[_curEvent->_special]);
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------
// Purpose: Get/set info
//------------------------------------------------------------------------
void Manager::endSequence(const Common::String &curloc) {
	if (_endSeq.empty())
		return;

	for (const auto &i : _endSeq)
		if (i._cur)
			_eventMap[curloc].endSeq(_activeSeq);
		else if (_eventMap.contains(i._loc))
			_eventMap[i._loc].endSeq(stringToNumber<uint>(i._val));

	_activeSeq = UINT_MAX;
	_endSeq.clear();
}

bool Manager::eventInProgress() {
	if (_activeSeq == UINT_MAX)
		return false;
	return true;
}

//------------------------------------------------------------------------
// Purpose: Save the state of the object
//------------------------------------------------------------------------
void Manager::saveState(rapidxml::xml_document<> &doc, rapidxml::xml_node<char> *root) {
	for (auto &i : _eventMap) {
		rapidxml::xml_node<char> *child = doc.allocate_node(rapidxml::node_element, "loc");
		child->append_attribute(doc.allocate_attribute("name", i._key.c_str()));
		i._value.saveState(doc, child);
		root->append_node(child);
	}
}

//------------------------------------------------------------------------
// Purpose: Load the state of the object
//------------------------------------------------------------------------
void Manager::loadState(rapidxml::xml_node<char> *node) {
	for (auto n = node->first_node("loc"); n != nullptr; n = n->next_sibling("loc")) {
		if (n->first_attribute("name") != nullptr) {
			Common::String name = n->first_attribute("name")->value();
			if (_eventMap.contains(name))
				_eventMap[name].loadState(n);
		}
	}
}

//------------------------------------------------------------------------
// Purpose: Function called when window size is changed to adjust UI
//------------------------------------------------------------------------
void Manager::setUI() {
	_oh.setUI();
	_reply.setUI();
	_textin.setUI();
	_per.setUI();
}

} // End of namespace Crab

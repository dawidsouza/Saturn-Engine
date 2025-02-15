/********************************************************************************************
*                                                                                           *
*                                                                                           *
*                                                                                           *
* MIT License                                                                               *
*                                                                                           *
* Copyright (c) 2020 - 2024 BEAST                                                           *
*                                                                                           *
* Permission is hereby granted, free of charge, to any person obtaining a copy              *
* of this software and associated documentation files (the "Software"), to deal             *
* in the Software without restriction, including without limitation the rights              *
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell                 *
* copies of the Software, and to permit persons to whom the Software is                     *
* furnished to do so, subject to the following conditions:                                  *
*                                                                                           *
* The above copyright notice and this permission notice shall be included in all            *
* copies or substantial portions of the Software.                                           *
*                                                                                           *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR                *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,                  *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE               *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER                    *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,             *
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE             *
* SOFTWARE.                                                                                 *
*********************************************************************************************
*/

#pragma once

#include "Saturn/Core/UUID.h"

#include <string>
#include <functional>
#include <Ruby/RubyEventType.h>

namespace Saturn {

	enum class ActionBindingType 
	{
		Key,
		Mouse
	};

	struct ActionBinding
	{
		std::string Name = "";
		ActionBindingType Type = ActionBindingType::Key;
		
		// That state should did this event fire in. For example, Pressed or Released.
		// This will not be set by the user and will be set by the Engine when this event is pressed or released.
		bool State = false;

		RubyKey Key = RubyKey::UnknownKey;
		RubyMouseButton MouseButton = RubyMouseButton::Unknown;
		
		std::function<void()> Function = nullptr;

		// Editor Only
		// TODO: I want to create a SAT_HAS_EDITOR macro so that this code is only there in Debug, Release
		// As our Dist config is our shipping for running the game without the editor attached.
		std::string ActionName = "";
		UUID ID;

		bool operator==( const ActionBinding& rOther ) 
		{
			return Name == rOther.Name && Type == rOther.Type && Key == rOther.Key && MouseButton == rOther.MouseButton && ID == rOther.ID;
		}
	};
}
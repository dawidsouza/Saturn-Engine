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

#include "Saturn/Core/Ref.h"
#include "Saturn/Core/Timestep.h"

#include <filesystem>

namespace Saturn {

	//////////////////////////////////////////////////////////////////////////
	// SClass Metadata
	// Editor Only
	// This information could of been inside of the SClass however I want to keep this away from whatever class uses this.
	// This is mainly used when choosing a parent class for a new class in the editor.
	struct SClassMetadata 
	{
		std::string Name;
		std::string ParentClassName;

		std::string Date;
		std::filesystem::path Path;

		bool Handled = false;
	};

	class SClass : public RefTarget
	{
	public:
		SClass() {}
		virtual ~SClass() = default;

		virtual void BeginPlay() {}
		virtual void OnUpdate( Saturn::Timestep ts ) {}
		virtual void OnPhysicsUpdate( Saturn::Timestep ts ) {}
	};

}
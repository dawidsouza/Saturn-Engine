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

#include "sppch.h"
#include "AudioSystem.h"

namespace Saturn {

	AudioSystem::AudioSystem()
	{
		//Init();
	}

	AudioSystem::~AudioSystem()
	{
		//ma_engine_uninit( &m_Engine );
	}

	void AudioSystem::CreateAudio( AudioType type, UUID ID, const std::filesystem::path& rPath )
	{
		ma_sound sound;
		MA_CHECK( ma_sound_init_from_file( &m_Engine, rPath.string().c_str(), 0, NULL, NULL, &sound ) );
	}

	bool AudioSystem::Play( ma_sound Sound )
	{
		MA_CHECK( ma_sound_start( &Sound ) );
		return true;
	}

	bool AudioSystem::Stop( ma_sound Sound )
	{
		MA_CHECK( ma_sound_stop( &Sound ) );
		return true;
	}

	void AudioSystem::Init()
	{
		// Create engine
		MA_CHECK( ma_engine_init( NULL, &m_Engine ) );
	}

	void AudioSystem::Terminate()
	{
	}
}
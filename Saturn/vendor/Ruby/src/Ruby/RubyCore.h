/********************************************************************************************
*                                                                                           *
*                                                                                           *
*                                                                                           *
* MIT License                                                                               *
*                                                                                           *
* Copyright (c) 2023 BEAST                                                           		*
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

#include <stdint.h>
#include <string>

#define RBY_BIT( x ) ( 1 << x )
#define RBY_DISABLE_COPY( x ) x(const x&) = delete

#if defined( RBY_DLL )
#if defined( RBY_BUILD )
#define RBY_API __declspec(dllexport)
#else
#define RBY_API __declspec(dllimport)
#endif
#else
#define RBY_API 
#endif

#if defined(_WIN32)
// I did not want to include windows.h here, but we will need to in order to use the HWND handle.
#include <Windows.h>
using WindowType_t = HWND;
#else
using WindowType_t = void*;
#endif

enum class RubyGraphicsAPI
{
	OpenGL,
	Vulkan,
	DirectX11,
	DirectX12,
	None
};

enum class RubyStyle
{
	Default = RBY_BIT( 0 ),
	Borderless = RBY_BIT( 1 ),
};

enum class RubyCursorType 
{
	Arrow,
	Hand,
	IBeam,
	ResizeEW,
	ResizeNS,
	ResizeNESW,
	ResizeNWSE,
	NotAllowed,
	Custom
};

enum class RubyCursorMode
{
	Normal,
	Hidden,
	Locked
};

enum class RubyWindowShowCmd
{
	Default,
	Fullscreen
};

struct RBY_API RubyWindowSpecification
{
	std::string_view Name;
	uint32_t Width = 0;
	uint32_t Height = 0;
	RubyGraphicsAPI GraphicsAPI = RubyGraphicsAPI::None;
	RubyStyle Style = RubyStyle::Default;
	bool ShowNow = true;
};

template<typename N>
struct RubyBasicVector2
{
	RubyBasicVector2() = default;
	RubyBasicVector2( N _x, N _y ) : x( _x ), y( _y ) {}

	N x = N();
	N y = N();

	RubyBasicVector2 operator+( const RubyBasicVector2& other ) const { return RubyBasicVector2( x + other.x, y + other.y ); }
	RubyBasicVector2 operator-( const RubyBasicVector2& other ) const { return RubyBasicVector2( x - other.x, y - other.y ); }
	RubyBasicVector2 operator/( const RubyBasicVector2& other ) const { return RubyBasicVector2( x / other.x, y / other.y ); }
	RubyBasicVector2 operator*( const RubyBasicVector2& other ) const { return RubyBasicVector2( x * other.x, y * other.y ); }

	RubyBasicVector2& operator+=( const RubyBasicVector2& other ) { x += other.x; y += other.y; return *this; }
	RubyBasicVector2& operator-=( const RubyBasicVector2& other ) { x -= other.x; y -= other.y; return *this; }
	RubyBasicVector2& operator*=( N scalar ) { x *= scalar; y *= scalar; return *this; }
	RubyBasicVector2& operator/=( N divisor ) { x /= divisor; y /= divisor; return *this; }
};

using RubyVec2 = RubyBasicVector2<float>;
using RubyIVec2 = RubyBasicVector2<int>;
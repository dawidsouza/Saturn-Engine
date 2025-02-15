/********************************************************************************************
*                                                                                           *
*                                                                                           *
*                                                                                           *
* MIT License                                                                               *
*                                                                                           *
* Copyright (c) 2015 - 2023 BEAST                                                           *
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
#include "PhysicsMaterialAsset.h"

#include "Saturn/Physics/PhysicsAuxiliary.h"
#include "Saturn/Physics/PhysicsFoundation.h"

namespace Saturn {

	PhysicsMaterialAsset::PhysicsMaterialAsset( float StaticFriction, float DynamicFriction, float Restitution, PhysicsMaterialFlags flags )
		: m_StaticFriction( StaticFriction ),  m_DynamicFriction( DynamicFriction ), m_Restitution( Restitution ), m_Flags( ( uint32_t ) flags )
	{
		m_Material = PhysicsFoundation::Get().GetPhysics().createMaterial( StaticFriction, DynamicFriction, Restitution );

		if( flags != PhysicsMaterialFlags::None )
			m_Material->setFlags( (physx::PxMaterialFlag::Enum)flags );
	}

	PhysicsMaterialAsset::~PhysicsMaterialAsset()
	{
		m_StaticFriction = 0.0f;
		m_DynamicFriction = 0.0f;
		m_Restitution = 0.0f;
		
		PHYSX_TERMINATE_ITEM( m_Material );
	}

	void PhysicsMaterialAsset::SetStaticFriction( float val )
	{
		m_StaticFriction = val;
		
		m_Material->setStaticFriction( val );
	}

	void PhysicsMaterialAsset::SetDynamicFriction( float val )
	{
		m_DynamicFriction = val;

		m_Material->setDynamicFriction( val );
	}

	void PhysicsMaterialAsset::SetRestitution( float val )
	{
		m_Restitution = val;

		m_Material->setRestitution( val );
	}

	void PhysicsMaterialAsset::SetFlag( PhysicsMaterialFlags flag, bool value )
	{
		if( value )
			m_Flags |= ( uint32_t ) flag;
		else
			m_Flags &= ~( uint32_t ) flag;

		m_Material->setFlag( ( physx::PxMaterialFlag::Enum )flag, value );
	}

}
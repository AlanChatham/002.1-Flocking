//
//  Bait.cpp
//  Flocking
//
//  Created by Robert Hodgin on 4/26/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "cinder/app/AppBasic.h"
#include "cinder/Rand.h"
#include "Lantern.h"

using namespace ci;

Lantern::Lantern( const Vec3f &pos )
{
	mPos		= pos;
	mRadius		= 0.0f;
	mRadiusDest	= Rand::randFloat( 4.5f, 7.5f );
	if( Rand::randFloat() < 0.1f ) mRadiusDest = Rand::randFloat( 13.0f, 25.0f );
	
	mFallSpeed	= Rand::randFloat( -0.5f, -0.15f );
	mVelocity   = Vec3f(Rand::randFloat(-0.5f, 0.5f), 0 , Rand::randFloat(-0.5f, 0.5f));
	mColor		= Color( CM_HSV, Rand::randFloat( 0.0f, 0.1f ), 0.9f, 1.0f );
	mIsDead		= false;
	mIsDying	= false;
	mFramesToLive = 1000;
	
	mTargetID = 0;
	mMaxSpeed = 1.0f;

	mVisiblePer	= 1.0f;
}

///<summary>

void Lantern::update( float deltaTime, Vec3f velocity, float yFloor ){
	mPos += deltaTime * velocity;
	if( ( mPos.y + mRadiusDest ) < yFloor ){
		mIsSinking = true;
		mIsDying = true;
	}
	
	
	if( mIsSinking ){
		//mVisiblePer = 1.0f - ( ( mPos.y + mRadiusDest ) - yFloor ) / ( mRadius + mRadius );
		mVisiblePer = 1.0f - ( ( mPos.y + mRadiusDest ) ) / ( mRadius + mRadius );
	}
	
	mFramesToLive--;
	if (mFramesToLive < 0){
		mIsDead = true;
	}

	if( mIsDying ){
		mRadius -= ( mRadius - 0.0f ) * 0.2f;
		if( mRadius < 0.1f )
			mIsDead = true;
	} else {
		mRadius -= ( mRadius - ( mRadiusDest + Rand::randFloat( 0.9f, 1.2f ) ) ) * 0.2f;
	}
}

void Lantern::update( float deltaTime, float yFloor ){
	// Update the position
	mPos += Vec3f( 0, mFallSpeed * deltaTime, 0 );
//	mPos += Vec3f( mVelocity.x + Rand::randFloat(-.1f, 0.1f), mFallSpeed * deltaTime + Rand::randFloat(-.1f, 0.1f), mVelocity.z + Rand::randFloat(-.1f, 0.1f) );
	// If it's below the floor, make it die
	if( ( mPos.y + mRadiusDest ) < yFloor ) {//||
//		( mPos.z - mRadiusDest ) > 300    ||
//		( mPos.z + mRadiusDest ) < -300    ||
//		( mPos.x - mRadiusDest ) > 300    ||
//		( mPos.x + mRadiusDest ) < -300 ) {
		mIsSinking = true;
		mIsDying = true;
	}
	
	
	if( mIsSinking ){
		mVisiblePer = 1.0f - ( ( mPos.y + mRadiusDest ) - yFloor ) / ( mRadius + mRadius );
	}
	
	
	if( mIsDying ){
		mRadius -= ( mRadius - 0.0f ) * 0.2f;
		if( mRadius < 0.1f )
			mIsDead = true;
	} else {
		mRadius -= ( mRadius - ( mRadiusDest + Rand::randFloat( 0.9f, 1.2f ) ) ) * 0.2f;
	}
}

void Lantern::draw(){
	gl::drawSphere( mPos, mRadius * 0.5f, 32 );
}
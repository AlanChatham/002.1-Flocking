#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Vbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/Camera.h"
#include "cinder/Rand.h"
#include "cinder/params/Params.h"
#include "Resources.h"
#include "SpringCam.h"
#include "HeadCam.h"
#include "Room.h"
#include "Controller.h"

#include "OscListener.h"
#include "OscMessage.h"

#include "Lantern.h"

using namespace ci;
using namespace ci::app;
using namespace std;

#define APP_WIDTH		2560
#define APP_HEIGHT		720
#define ROOM_WIDTH		600
#define ROOM_HEIGHT		400
#define ROOM_DEPTH		600
#define ROOM_FBO_RES	2
// This is the square root of the number of fish and predators
// Make this too big, and they start to escape the tank for some reason...
#define FBO_DIM			40 //50//167
#define P_FBO_DIM		5
#define MAX_LANTERNS	5

class FlockingApp : public AppBasic {
public:
	virtual void		prepareSettings( Settings *settings );
	virtual void		setup();
	void				adjustFboDim( int offset );
	void				initialize();
	void				setFboPositions( gl::Fbo fbo );
	void				setFboVelocities( gl::Fbo fbo );
	void				setPredatorFboPositions( gl::Fbo fbo );
	void				setPredatorFboVelocities( gl::Fbo fbo );
	void				initVbo();
	void				initPredatorVbo();
	virtual void		mouseDown( MouseEvent event );
	virtual void		mouseUp( MouseEvent event );
	virtual void		mouseMove( MouseEvent event );
	virtual void		mouseDrag( MouseEvent event );
	virtual void		mouseWheel( MouseEvent event );
	virtual void		keyDown( KeyEvent event );
	virtual void		update();
	void				drawIntoRoomFbo();
	void				drawInfoPanel();
	void				drawIntoVelocityFbo();
	void				drawIntoPositionFbo();
	void				drawIntoPredatorVelocityFbo();
	void				drawIntoPredatorPositionFbo();
	void				drawIntoLanternsFbo();
	void				drawGlows();
	void				drawNebulas();
	virtual void		draw();
	void				drawGuts(Area area);
	void				setCameras(Vec3f headPosition, bool fromKeyboard);
	void				checkOSCMessage(const osc::Message*);
	
	// CAMERA
	HeadCam			    mSpringCam;
	HeadCam				mHeadCam0;
	HeadCam				mHeadCam1;
	HeadCam				mActiveCam;
	
	// TEXTURES
	gl::Texture			mLanternGlowTex;
	gl::Texture			mGlowTex;
	gl::Texture			mNebulaTex;
	gl::Texture			mIconTex;
	
	// SHADERS
	gl::GlslProg		mVelocityShader;
	gl::GlslProg		mPositionShader;
	gl::GlslProg		mP_VelocityShader;
	gl::GlslProg		mP_PositionShader;
	gl::GlslProg		mLanternShader;
	gl::GlslProg		mLanternGlowShader;
	gl::GlslProg		mRoomShader;
	gl::GlslProg		mShader;
	gl::GlslProg		mP_Shader;
	gl::GlslProg		mGlowShader;
	gl::GlslProg		mNebulaShader;
	
	// CONTROLLER
	Controller			mController;

	// LANTERNS (point lights)
	gl::Fbo				mLanternsFbo;

	Vec3f lanternTargets[100];
	int lanternTimeouts[100];
	
	// ROOM
	Room				mRoom;
	gl::Fbo				mRoomFbo;
	
	// VBOS
	gl::VboMesh			mVboMesh;
	gl::VboMesh			mP_VboMesh;
	
	// POSITION/VELOCITY FBOS
	gl::Fbo::Format		mRgba16Format;
	int					mFboDim;
	ci::Vec2f			mFboSize;
	ci::Area			mFboBounds;
	gl::Fbo				mPositionFbos[2];
	gl::Fbo				mVelocityFbos[2];
	int					mP_FboDim;
	ci::Vec2f			mP_FboSize;
	ci::Area			mP_FboBounds;
	gl::Fbo				mP_PositionFbos[2];
	gl::Fbo				mP_VelocityFbos[2];
	int					mThisFbo, mPrevFbo;
	
	// MOUSE
	Vec2f				mMousePos, mMouseDownPos, mMouseOffset;
	bool				mMousePressed;
	
	bool				mSaveFrames;
	int					mNumSavedFrames;
	
	bool				mInitUpdateCalled;

	// OSC listener
	osc::Listener   oscListener;
	
};

void FlockingApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( APP_WIDTH, APP_HEIGHT );
	settings->setBorderless();
	settings->setWindowPos(0,0);
}

void FlockingApp::setup()
{
	// CAMERA	
	mSpringCam = HeadCam( 1210.0f, getWindowAspectRatio() );
	mSpringCam.mEye = Vec3f(1200,0,1200);
	mSpringCam.mEye.y = 0;
	mSpringCam.mCenter = Vec3f(0,0, ROOM_DEPTH / 2 );
	
	// Set up a listener for OSC messages
	oscListener.setup(7110);

	// Setup the camera for the main window
	mHeadCam0 = HeadCam( 1210.0f, getWindowAspectRatio() );
	mHeadCam0.mEye = Vec3f(-1200,0,1200);
	mHeadCam0.mEye.y = 0;
	mHeadCam0.mCenter = Vec3f(0,0, ROOM_DEPTH / 2 );

	mHeadCam1 = HeadCam( 1200.0f, getWindowAspectRatio() );
	mHeadCam1.mEye = Vec3f(-1210,0,0);
	mHeadCam1.mCenter = Vec3f(-ROOM_WIDTH / 2, 0, 0 );

	// POSITION/VELOCITY FBOS
	mRgba16Format = gl::Fbo::Format();
	mRgba16Format.setColorInternalFormat( GL_RGBA16F_ARB );
	mRgba16Format.setMinFilter( GL_NEAREST );
	mRgba16Format.setMagFilter( GL_NEAREST );
	mThisFbo			= 0;
	mPrevFbo			= 1;
	
	// LANTERNS
	mLanternsFbo		= gl::Fbo( MAX_LANTERNS, 2, mRgba16Format );
	for (int i = 0; i < 100; i++){
		lanternTargets[i] = Vec3f::zero();
		lanternTimeouts[i] = 0;
	}
	
	// TEXTURE FORMAT
	gl::Texture::Format mipFmt;
    mipFmt.enableMipmapping( true );
    mipFmt.setMinFilter( GL_LINEAR_MIPMAP_LINEAR );    
    mipFmt.setMagFilter( GL_LINEAR );
	
	// TEXTURES
	mLanternGlowTex		= gl::Texture( loadImage( loadResource( RES_LANTERNGLOW_PNG ) ) );
	mGlowTex			= gl::Texture( loadImage( loadResource( RES_GLOW_PNG ) ) );
	mNebulaTex			= gl::Texture( loadImage( loadResource( RES_NEBULA_PNG ) ) );
	mIconTex			= gl::Texture( loadImage( loadResource( ICON_ID ) ), mipFmt );
	
	// LOAD SHADERS
	try {
		mVelocityShader		= gl::GlslProg( loadResource( RES_PASSTHRU_VERT ),	loadResource( RES_VELOCITY_FRAG ) );
		mPositionShader		= gl::GlslProg( loadResource( RES_PASSTHRU_VERT ),	loadResource( RES_POSITION_FRAG ) );
		mP_VelocityShader	= gl::GlslProg( loadResource( RES_PASSTHRU_VERT ),	loadResource( RES_P_VELOCITY_FRAG ) );
		mP_PositionShader	= gl::GlslProg( loadResource( RES_PASSTHRU_VERT ),	loadResource( RES_P_POSITION_FRAG ) );
		mLanternShader		= gl::GlslProg( loadResource( RES_LANTERN_VERT ),	loadResource( RES_LANTERN_FRAG ) );
		mLanternGlowShader	= gl::GlslProg( loadResource( RES_GLOW_NEBULA_VERT ),	loadResource( RES_GLOW_FRAG ) );
		mRoomShader			= gl::GlslProg( loadResource( RES_ROOM_VERT ),		loadResource( RES_ROOM_FRAG ) );
		mShader				= gl::GlslProg( loadResource( RES_VBOPOS_VERT ),	loadResource( RES_VBOPOS_FRAG ) );
		mP_Shader			= gl::GlslProg( loadResource( RES_P_VBOPOS_VERT ),	loadResource( RES_P_VBOPOS_FRAG ) );
		mGlowShader			= gl::GlslProg( loadResource( RES_GLOW_NEBULA_VERT ),	loadResource( RES_GLOW_FRAG ) );
		mNebulaShader		= gl::GlslProg( loadResource( RES_GLOW_NEBULA_VERT ),	loadResource( RES_NEBULA_FRAG ) );
	} catch( gl::GlslProgCompileExc e ) {
		std::cout << e.what() << std::endl;
		quit();
	}
	
	// ROOM
	gl::Fbo::Format roomFormat;
	roomFormat.setColorInternalFormat( GL_RGB );
	mRoomFbo			= gl::Fbo( APP_WIDTH/ROOM_FBO_RES, APP_HEIGHT/ROOM_FBO_RES, roomFormat );
	bool isPowerOn		= false;
	bool isGravityOn	= true;
	mRoom				= Room( Vec3f( ROOM_WIDTH / 2 , ROOM_HEIGHT / 2, ROOM_DEPTH / 2 ), isPowerOn, isGravityOn );	
	mRoom.init();
	
	// CONTROLLER
	mController			= Controller( &mRoom, MAX_LANTERNS );
	
	// MOUSE
	mMousePos			= Vec2f::zero();
	mMouseDownPos		= Vec2f::zero();
	mMouseOffset		= Vec2f::zero();
	mMousePressed		= false;
	
	mSaveFrames			= false;
	mNumSavedFrames		= 0;
	
	mInitUpdateCalled	= false;
	
	initialize();
}

void FlockingApp::initialize()
{
	gl::disableAlphaBlending();
	gl::disableDepthWrite();
	gl::disableDepthRead();
	
	mFboDim				= FBO_DIM;
	mFboSize			= Vec2f( mFboDim, mFboDim );
	mFboBounds			= Area( 0, 0, mFboDim, mFboDim );
	mPositionFbos[0]	= gl::Fbo( mFboDim, mFboDim, mRgba16Format );
	mPositionFbos[1]	= gl::Fbo( mFboDim, mFboDim, mRgba16Format );
	mVelocityFbos[0]	= gl::Fbo( mFboDim, mFboDim, mRgba16Format );
	mVelocityFbos[1]	= gl::Fbo( mFboDim, mFboDim, mRgba16Format );
	
	mPositionFbos[0].getTexture();

	setFboPositions( mPositionFbos[0] );
	setFboPositions( mPositionFbos[1] );
	setFboVelocities( mVelocityFbos[0] );
	setFboVelocities( mVelocityFbos[1] );
	
	
	mP_FboDim			= P_FBO_DIM;
	mP_FboSize			= Vec2f( mP_FboDim, mP_FboDim );
	mP_FboBounds		= Area( 0, 0, mP_FboDim, mP_FboDim );
	mP_PositionFbos[0]	= gl::Fbo( mP_FboDim, mP_FboDim, mRgba16Format );
	mP_PositionFbos[1]	= gl::Fbo( mP_FboDim, mP_FboDim, mRgba16Format );
	mP_VelocityFbos[0]	= gl::Fbo( mP_FboDim, mP_FboDim, mRgba16Format );
	mP_VelocityFbos[1]	= gl::Fbo( mP_FboDim, mP_FboDim, mRgba16Format );
	
	setPredatorFboPositions( mP_PositionFbos[0] );
	setPredatorFboPositions( mP_PositionFbos[1] );
	setPredatorFboVelocities( mP_VelocityFbos[0] );
	setPredatorFboVelocities( mP_VelocityFbos[1] );
	
	initVbo();
	initPredatorVbo();
}

void FlockingApp::setFboPositions( gl::Fbo fbo )
{	
	// FISH POSITION
	Surface32f posSurface( fbo.getTexture() );
	Surface32f::Iter it = posSurface.getIter();
	while( it.line() ){
		float y = (float)it.y()/(float)it.getHeight() - 0.5f;
		while( it.pixel() ){
			float per		= (float)it.x()/(float)it.getWidth();
			float angle		= per * M_PI * 2.0f;
			float radius	= 100.0f;
			float cosA		= cos( angle );
			float sinA		= sin( angle );
			Vec3f p			= Vec3f( cosA, y, sinA ) * radius;
			
			it.r() = p.x;
			it.g() = p.y;
			it.b() = p.z;
			it.a() = Rand::randFloat( 0.7f, 1.0f );	// GENERAL EMOTIONAL STATE. 
		}
	}
	
	gl::Texture posTexture( posSurface );
	fbo.bindFramebuffer();
	gl::setMatricesWindow( mFboSize, false );
	gl::setViewport( mFboBounds );
	gl::draw( posTexture );
	fbo.unbindFramebuffer();
}

void FlockingApp::setFboVelocities( gl::Fbo fbo )
{
	// FISH VELOCITY
	Surface32f velSurface( fbo.getTexture() );
	Surface32f::Iter it = velSurface.getIter();
	while( it.line() ){
		while( it.pixel() ){
			float per		= (float)it.x()/(float)it.getWidth();
			float angle		= per * M_PI * 2.0f;
			float cosA		= cos( angle );
			float sinA		= sin( angle );
			Vec3f p			= Vec3f( cosA, 0.0f, sinA );
			it.r() = p.x;
			it.g() = p.y;
			it.b() = p.z;
			it.a() = 1.0f;
		}
	}
	
	gl::Texture velTexture( velSurface );
	fbo.bindFramebuffer();
	gl::setMatricesWindow( mFboSize, false );
	gl::setViewport( mFboBounds );
	gl::draw( velTexture );
	fbo.unbindFramebuffer();
}

void FlockingApp::setPredatorFboPositions( gl::Fbo fbo )
{	
	// PREDATOR POSITION
	Surface32f posSurface( fbo.getTexture() );
	Surface32f::Iter it = posSurface.getIter();
	while( it.line() ){
		while( it.pixel() ){
			Vec3f r = Rand::randVec3f() * 50.0f;
			it.r() = r.x;
			it.g() = r.y;
			it.b() = r.z;
			it.a() = Rand::randFloat( 0.7f, 1.0f );	// GENERAL EMOTIONAL STATE. 
		}
	}
	
	gl::Texture posTexture( posSurface );
	fbo.bindFramebuffer();
	gl::setMatricesWindow( mP_FboSize, false );
	gl::setViewport( mP_FboBounds );
	gl::draw( posTexture );
	fbo.unbindFramebuffer();
}

void FlockingApp::setPredatorFboVelocities( gl::Fbo fbo )
{
	// PREDATOR VELOCITY
	Surface32f velSurface( fbo.getTexture() );
	Surface32f::Iter it = velSurface.getIter();
	while( it.line() ){
		while( it.pixel() ){
			Vec3f r = Rand::randVec3f() * 3.0f;
			it.r() = r.x;
			it.g() = r.y;
			it.b() = r.z;
			it.a() = 1.0f;
		}
	}
	
	gl::Texture velTexture( velSurface );
	fbo.bindFramebuffer();
	gl::setMatricesWindow( mP_FboSize, false );
	gl::setViewport( mP_FboBounds );
	gl::draw( velTexture );
	fbo.unbindFramebuffer();
}

void FlockingApp::initVbo()
{
	gl::VboMesh::Layout layout;
	layout.setStaticPositions();
	layout.setStaticTexCoords2d();
	layout.setStaticNormals();
	
	int numVertices = mFboDim * mFboDim;
	// 5 points make up the pyramid
	// 8 triangles make up two pyramids
	// 3 points per triangle
	
	mVboMesh		= gl::VboMesh( numVertices * 8 * 3, 0, layout, GL_TRIANGLES );
	
	float s = 1.5f;
	Vec3f p0( 0.0f, 0.0f, 2.0f );
	Vec3f p1( -s, -s, 0.0f );
	Vec3f p2( -s,  s, 0.0f );
	Vec3f p3(  s,  s, 0.0f );
	Vec3f p4(  s, -s, 0.0f );
	Vec3f p5( 0.0f, 0.0f, -5.0f );
	
	Vec3f n;
	Vec3f n0 = Vec3f( 0.0f, 0.0f, 1.0f );
	Vec3f n1 = Vec3f(-1.0f,-1.0f, 0.0f ).normalized();
	Vec3f n2 = Vec3f(-1.0f, 1.0f, 0.0f ).normalized();
	Vec3f n3 = Vec3f( 1.0f, 1.0f, 0.0f ).normalized();
	Vec3f n4 = Vec3f( 1.0f,-1.0f, 0.0f ).normalized();
	Vec3f n5 = Vec3f( 0.0f, 0.0f,-1.0f );
	
	vector<Vec3f>		positions;
	vector<Vec3f>		normals;
	vector<Vec2f>		texCoords;
	
	for( int x = 0; x < mFboDim; ++x ) {
		for( int y = 0; y < mFboDim; ++y ) {
			float u = (float)x/(float)mFboDim;
			float v = (float)y/(float)mFboDim;
			Vec2f t = Vec2f( u, v );
			
			positions.push_back( p0 );
			positions.push_back( p1 );
			positions.push_back( p2 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p1 + p2 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p0 );
			positions.push_back( p2 );
			positions.push_back( p3 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p2 + p3 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p0 );
			positions.push_back( p3 );
			positions.push_back( p4 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p3 + p4 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p0 );
			positions.push_back( p4 );
			positions.push_back( p1 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p4 + p1 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			
			
			
			positions.push_back( p5 );
			positions.push_back( p1 );
			positions.push_back( p4 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p1 + p4 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p5 );
			positions.push_back( p2 );
			positions.push_back( p1 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p2 + p1 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p5 );
			positions.push_back( p3 );
			positions.push_back( p2 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p3 + p2 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p5 );
			positions.push_back( p4 );
			positions.push_back( p3 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p4 + p3 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			
		}
	}
	
	mVboMesh.bufferPositions( positions );
	mVboMesh.bufferTexCoords2d( 0, texCoords );
	mVboMesh.bufferNormals( normals );
	mVboMesh.unbindBuffers();
}

void FlockingApp::initPredatorVbo()
{
	gl::VboMesh::Layout layout;
	layout.setStaticPositions();
	layout.setStaticTexCoords2d();
	layout.setStaticNormals();
	
	int numVertices = mP_FboDim * mP_FboDim;
	// 5 points make up the pyramid
	// 8 triangles make up two pyramids
	// 3 points per triangle
	
	mP_VboMesh		= gl::VboMesh( numVertices * 8 * 3, 0, layout, GL_TRIANGLES );
	
	float s = 5.0f;
	Vec3f p0( 0.0f, 0.0f, 3.0f );
	Vec3f p1( -s*1.3f, 0.0f, 0.0f );
	Vec3f p2( 0.0f, s * 0.5f, 0.0f );
	Vec3f p3( s*1.3f, 0.0f, 0.0f );
	Vec3f p4( 0.0f, -s * 0.5f, 0.0f );
	Vec3f p5( 0.0f, 0.0f, -12.0f );
	

	
	Vec3f n;
	Vec3f n0 = Vec3f( 0.0f, 0.0f, 1.0f );
	Vec3f n1 = Vec3f(-1.0f, 0.0f, 0.0f ).normalized();
	Vec3f n2 = Vec3f( 0.0f, 1.0f, 0.0f ).normalized();
	Vec3f n3 = Vec3f( 1.0f, 0.0f, 0.0f ).normalized();
	Vec3f n4 = Vec3f( 0.0f,-1.0f, 0.0f ).normalized();
	Vec3f n5 = Vec3f( 0.0f, 0.0f,-1.0f );
	
	vector<Vec3f>		positions;
	vector<Vec3f>		normals;
	vector<Vec2f>		texCoords;
	
	for( int x = 0; x < mP_FboDim; ++x ) {
		for( int y = 0; y < mP_FboDim; ++y ) {
			float u = (float)x/(float)mP_FboDim;
			float v = (float)y/(float)mP_FboDim;
			Vec2f t = Vec2f( u, v );
			
			positions.push_back( p0 );
			positions.push_back( p1 );
			positions.push_back( p2 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p1 + p2 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p0 );
			positions.push_back( p2 );
			positions.push_back( p3 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p2 + p3 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p0 );
			positions.push_back( p3 );
			positions.push_back( p4 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p3 + p4 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p0 );
			positions.push_back( p4 );
			positions.push_back( p1 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p0 + p4 + p1 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			
			
			
			positions.push_back( p5 );
			positions.push_back( p1 );
			positions.push_back( p4 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p1 + p4 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p5 );
			positions.push_back( p2 );
			positions.push_back( p1 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p2 + p1 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p5 );
			positions.push_back( p3 );
			positions.push_back( p2 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p3 + p2 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			positions.push_back( p5 );
			positions.push_back( p4 );
			positions.push_back( p3 );
			texCoords.push_back( t );
			texCoords.push_back( t );
			texCoords.push_back( t );
			n = ( p5 + p4 + p3 ).normalized();
			normals.push_back( n );
			normals.push_back( n );
			normals.push_back( n );
			
			
		}
	}
	
	mP_VboMesh.bufferPositions( positions );
	mP_VboMesh.bufferTexCoords2d( 0, texCoords );
	mP_VboMesh.bufferNormals( normals );
	mP_VboMesh.unbindBuffers();
}


void FlockingApp::mouseDown( MouseEvent event )
{
	mMouseDownPos = event.getPos();
	mMousePressed = true;
	mMouseOffset = Vec2f::zero();
}

void FlockingApp::mouseUp( MouseEvent event )
{
	mMousePressed = false;
	mMouseOffset = Vec2f::zero();
}

void FlockingApp::mouseMove( MouseEvent event )
{
	mMousePos = event.getPos();
}

void FlockingApp::mouseDrag( MouseEvent event )
{
	mouseMove( event );
	mMouseOffset = ( mMousePos - mMouseDownPos );
}

void FlockingApp::mouseWheel( MouseEvent event )
{
	float dWheel = event.getWheelIncrement();
	mRoom.adjustTimeMulti( dWheel );
}

void FlockingApp::keyDown( KeyEvent event )
{
	if( event.getChar() == ' ' ){
		mRoom.togglePower();
	} else if( event.getChar() == 'l' ){
		mController.addLantern( mRoom.getRandCeilingPos() );
	}

	switch( event.getCode() ){
		//case KeyEvent::KEY_UP:		mMouseRightPos = Vec2f( 222.0f, 205.0f ) + getWindowCenter();	break;
		case KeyEvent::KEY_UP:		setCameras(Vec3f(mHeadCam0.mEye.x + 1, mHeadCam0.mEye.y, mHeadCam0.mEye.z - 100), true);
									break;
		//case KeyEvent::KEY_LEFT:	mMouseRightPos = Vec2f(-128.0f,-178.0f ) + getWindowCenter();	break;
		case KeyEvent::KEY_LEFT:	setCameras(Vec3f(mHeadCam0.mEye.x - 100, mHeadCam0.mEye.y, mHeadCam0.mEye.z), true);
									break;
			//case KeyEvent::KEY_RIGHT:	mMouseRightPos = Vec2f(-256.0f, 122.0f ) + getWindowCenter();	break;
		case KeyEvent::KEY_RIGHT:	setCameras(Vec3f(mHeadCam0.mEye.x + 100, mHeadCam0.mEye.y, mHeadCam0.mEye.z), true);	break;
		//case KeyEvent::KEY_DOWN:	mMouseRightPos = Vec2f(   0.0f,   0.0f ) + getWindowCenter();	break;
		case KeyEvent::KEY_DOWN:	setCameras(Vec3f(mHeadCam0.mEye.x, mHeadCam0.mEye.y, mHeadCam0.mEye.z + 100), true);
									break;
		default: break;
	}
}

void FlockingApp::setCameras(Vec3f headPosition, bool fromKeyboard = false){
	// Separate out the components	
	float headX = headPosition.x;
	float headY = headPosition.y;
	float headZ = headPosition.z;
	
	// Adjust these away from the wonky coordinates we get from the Kinect,
	//  then normalize them so that they're in units of 100px per foot

	if (!fromKeyboard){
	//3.28 feet in a meter
		headX = headX * 3.28f * 100;
		headY = headY * 3.28f * 100 - ( ROOM_HEIGHT / 3 ); // Offset to bring up the vertical position of the eye
		headZ = headZ * 3.28f * 100;
	}
	
	// Make sure that the cameras are located somewhere in front of the screens
	//  Orientation is   X
	//                   |        ------> Screen 2 x axis
	//                   |   ------ Screen 2
	//                   |   |
	//                   |   |
	//         Z----<----|---o < Screen 1, o is the origin
	//                   |   | 
	//                   v   |

	//console() << "headX: " << headX << std::endl;
	//console() << "headZ: " << headZ << std::endl;

	if (headZ > ROOM_DEPTH / 2){
		mHeadCam0.setEye(Vec3f(headX, headY, headZ));
	}
	else{
		mHeadCam0.mEye = Vec3f(0,0,1200);
	}
	if (headX < -ROOM_WIDTH / 2){
		// headZ is negative here since that axis is backwards on Screen 2
		mHeadCam1.setEye(Vec3f(headX, headY, headZ));
	}
	else{
		mHeadCam1.mEye = Vec3f(-1200,0,0);
	}
}

void FlockingApp::checkOSCMessage(const osc::Message * message){
	// Sanity check that we have our legit head message
	if (message->getAddress() == "/head" && message->getNumArgs() == 3){
		float headX = message->getArgAsFloat(0);
		float headY = message->getArgAsFloat(1);
		float headZ = message->getArgAsFloat(2);

		setCameras(Vec3f(headX, headY, headZ), false);
	}

	if (message->getAddress() == "/hand" && message->getNumArgs() == 4){
		float handX = message->getArgAsFloat(0);
		float handY = message->getArgAsFloat(1);
		float handZ = message->getArgAsFloat(2);
		int ID    = message->getArgAsInt32(3);
		console() << ID << std::endl;
		//3.28 feet in a meter
		handX = handX * 3.28f * 100;
		handY = handY * 3.28f * 100 - ( ROOM_HEIGHT / 3 ) * .8f; // Offset to bring up the vertical position of the eye
		handZ = handZ * 3.28f * 100;
	

		// Front kinects
		if (handZ > 400){
			handZ -= 1500;
			// Make sure that hands stay in the box-ish
			if(handX > 290)
				handX = 290;
			if (handX < -290)
				handX = -290;
			if(handZ > 290)
				handZ = 290;
			if (handZ < -290)
				handZ = -290;
			
			Vec3f pos = Vec3f(handX, handY, handZ);
			// Now add it to the lantern target list
			lanternTargets[ID] = pos;
			// and reset the timeout
			lanternTimeouts[ID] = 50;
		}

		// side kinects
		if (handX <- 400){
			handX += 1200;
			handX *= 1.1f;
			// Make sure that hands stay in the box-ish
			if(handX > 290)
				handX = 290;
			if (handX < -290)
				handX = -290;
			if(handZ > 290)
				handZ = 290;
			if (handZ < -290)
				handZ = -290;
			
			Vec3f pos = Vec3f(handX, handY, handZ);
			// Now add it to the lantern target list
			lanternTargets[ID] = pos;
			// and reset the timeout
			lanternTimeouts[ID] = 50;
		}
	}

	
}

void FlockingApp::update()
{
	// Every update, decrement the timeout for each hand target, so we eventually clear them
	for (int i = 0; i < 100; i++){
		if (lanternTimeouts[i] > 0)
			lanternTimeouts[i]--;
		else{
			// Kill the lantern target
			lanternTargets[i] = Vec3f::zero();
		}
	}

	if( !mInitUpdateCalled ){
		mInitUpdateCalled = true;
	}

	// Get in OSC data
	osc::Message headMessage;

	while( oscListener.hasWaitingMessages() ) {
		osc::Message message;
		oscListener.getNextMessage( &message );
		
		checkOSCMessage(&message);
	}
	// ROOM
	mRoom.update( mSaveFrames );
	
	// CONTROLLER
	// This seems to only update the 'food' particles
	mController.update(lanternTargets);

	// CAMERA
	//if( mMousePressed ){
	//	mActiveCam.dragCam( ( mMouseOffset ) * 0.01f, ( mMouseOffset ).length() * 0.01f );
	//}

	Vec3f projectionEye = mHeadCam0.mEye;
	projectionEye.x = mHeadCam0.mCenter.x;
	projectionEye.y = mHeadCam0.mCenter.y;

	float zOffset = projectionEye.z - mHeadCam0.mCenter.z;
	// We have to adjust the camera to take into account that it
	//  doesn't distort enough past the edge of the screen
	float r = 0.0f; 
	float camXStorage = mHeadCam0.mEye.x;
	if (mHeadCam0.mEye.x < -300){
		r = (mHeadCam0.mEye.x + (ROOM_WIDTH / 2 )) / (mHeadCam0.mEye.z - (ROOM_DEPTH / 2));
		mHeadCam0.mEye.x += r * mHeadCam0.mEye.z;
	}

	Vec3f bottomLeft = Vec3f(-300, -200, -zOffset);
	Vec3f bottomRight = Vec3f(300, -200, -zOffset);
	Vec3f topLeft = Vec3f(-300, 200, -zOffset);

	mHeadCam0.update(projectionEye, bottomLeft, bottomRight, topLeft);
	// Restore our camera position
	mHeadCam0.mEye.x = camXStorage;

	console() << "cam0 position" << mHeadCam0.mEye << std::endl;
	console() << "projectionEye position" << projectionEye << std::endl;
	// Make sure to set it back, so updating doesn't make this fly away...
	// Now update Camera 1
	
	float xOffset = projectionEye.x - mHeadCam1.mCenter.x;
	
	// The values we pass into update for the bounds and the projectionEye need to 
	//  be coordinates relative to the camera, but mHeadCam1 is in global coordinates!
	bottomLeft = Vec3f(-300, -200, mHeadCam1.mEye.x + 300);//xOffset);
	bottomRight = Vec3f(300, -200, mHeadCam1.mEye.x + 300);//xOffset);
	topLeft = Vec3f(-300, 200, mHeadCam1.mEye.x + 300);//xOffset);
	
	projectionEye.y = mHeadCam1.mCenter.y;
	projectionEye.z = mHeadCam1.mCenter.z;

	Vec3f tempEye = mHeadCam1.mEye;
	// Again, I don't know why I've got to multiply this by 2
	mHeadCam1.mEye.x = tempEye.z;
	mHeadCam1.mEye.z = -tempEye.x;
		
	projectionEye = Vec3f(-mHeadCam1.mEye.z,0, 0);

	// Again, we have to adjust for the incorrect camera correction
	if (mHeadCam1.mEye.x > 300){
		r = (mHeadCam1.mEye.x - (ROOM_DEPTH / 2 )) / (mHeadCam1.mEye.z - (ROOM_WIDTH / 2));
		mHeadCam1.mEye.x += r * mHeadCam1.mEye.z;
	}
	
	console() << "ratio is : " << r << std::endl;
	
	mHeadCam1.update(projectionEye, bottomLeft, bottomRight, topLeft);
	
	console() << "cam1 position" << mHeadCam1.mEye << std::endl;
	console() << "projectionEye position" << projectionEye << std::endl;
	mHeadCam1.mEye.x = tempEye.x;
	mHeadCam1.mEye.z = tempEye.z;



	gl::disableAlphaBlending();
	gl::disableDepthRead();
	gl::disableDepthWrite();
	gl::color( Color( 1, 1, 1 ) );

	// This stuff seems to update the positions and velocities in FBOs,
	//  and I think that pushes the computation to the graphics card?
	drawIntoVelocityFbo();
	drawIntoPositionFbo();
	drawIntoPredatorVelocityFbo();
	drawIntoPredatorPositionFbo();

	
	mP_VelocityFbos[ mThisFbo ].unbindFramebuffer();

	
}


// FISH VELOCITY
void FlockingApp::drawIntoVelocityFbo()
{
	gl::setMatricesWindow( mFboSize, false );
	gl::setViewport( mFboBounds );
	
	mVelocityFbos[ mThisFbo ].bindFramebuffer();
	gl::clear( ColorA( 0, 0, 0, 0 ) );
	
	mPositionFbos[ mPrevFbo ].bindTexture( 0 );
	mVelocityFbos[ mPrevFbo ].bindTexture( 1 );
	mP_PositionFbos[ mPrevFbo ].bindTexture( 2 );
	mLanternsFbo.bindTexture( 3 );
	
	mVelocityShader.bind();
	mVelocityShader.uniform( "positionTex", 0 );
	mVelocityShader.uniform( "velocityTex", 1 );
	mVelocityShader.uniform( "predatorPositionTex", 2 );
	mVelocityShader.uniform( "lanternsTex", 3 );
	mVelocityShader.uniform( "numLights", (float)mController.mNumLanterns );
	mVelocityShader.uniform( "invNumLights", 1.0f/(float)MAX_LANTERNS );
	mVelocityShader.uniform( "invNumLightsHalf", 1.0f/(float)MAX_LANTERNS * 0.5f );
	mVelocityShader.uniform( "att", 1.015f );
	mVelocityShader.uniform( "roomBounds", mRoom.getDims() );
	mVelocityShader.uniform( "fboDim", mFboDim );
	mVelocityShader.uniform( "invFboDim", 1.0f/(float)mFboDim );
	mVelocityShader.uniform( "pFboDim", mP_FboDim );
	mVelocityShader.uniform( "pInvFboDim", 1.0f/(float)mP_FboDim );
	mVelocityShader.uniform( "dt", mRoom.mTimeAdjusted );
	mVelocityShader.uniform( "power", mRoom.getPower() );
	gl::drawSolidRect( mFboBounds );
	mVelocityShader.unbind();
	
	mVelocityFbos[ mThisFbo ].unbindFramebuffer();
}

// FISH POSITION
void FlockingApp::drawIntoPositionFbo()
{	
	// Set the viewport to be the 
	gl::setMatricesWindow( mFboSize, false );
	gl::setViewport( mFboBounds );
	
	mPositionFbos[ mThisFbo ].bindFramebuffer();
	mPositionFbos[ mPrevFbo ].bindTexture( 0 );
	mVelocityFbos[ mThisFbo ].bindTexture( 1 );
	
	mPositionShader.bind();
	mPositionShader.uniform( "position", 0 );
	mPositionShader.uniform( "velocity", 1 );
	mPositionShader.uniform( "dt", mRoom.mTimeAdjusted );
	gl::drawSolidRect( mFboBounds );
	mPositionShader.unbind();
	
	mPositionFbos[ mThisFbo ].unbindFramebuffer();
}

// PREDATOR VELOCITY
void FlockingApp::drawIntoPredatorVelocityFbo()
{
	gl::setMatricesWindow( mP_FboSize, false );
	gl::setViewport( mP_FboBounds );
	
	mP_VelocityFbos[ mThisFbo ].bindFramebuffer();
	gl::clear( ColorA( 0, 0, 0, 0 ) );
	
	mP_PositionFbos[ mPrevFbo ].bindTexture( 0 );
	mP_VelocityFbos[ mPrevFbo ].bindTexture( 1 );
	mPositionFbos[ mPrevFbo ].bindTexture( 2 );
	mLanternsFbo.bindTexture( 3 );
	mP_VelocityShader.bind();
	mP_VelocityShader.uniform( "positionTex", 0 );
	mP_VelocityShader.uniform( "velocityTex", 1 );
	mP_VelocityShader.uniform( "preyPositionTex", 2 );
	mP_VelocityShader.uniform( "lanternsTex", 3 );
	mP_VelocityShader.uniform( "numLights", (float)mController.mNumLanterns );
	mP_VelocityShader.uniform( "invNumLights", 1.0f/(float)MAX_LANTERNS );
	mP_VelocityShader.uniform( "invNumLightsHalf", 1.0f/(float)MAX_LANTERNS * 0.5f );
	mP_VelocityShader.uniform( "att", 1.015f );
	mP_VelocityShader.uniform( "roomBounds", mRoom.getDims() );
	mP_VelocityShader.uniform( "fboDim", mP_FboDim );
	mP_VelocityShader.uniform( "invFboDim", 1.0f/(float)mP_FboDim );
	mP_VelocityShader.uniform( "preyFboDim", mFboDim );
	mP_VelocityShader.uniform( "invPreyFboDim", 1.0f/(float)mFboDim );
	
	mP_VelocityShader.uniform( "dt", mRoom.mTimeAdjusted );
	mP_VelocityShader.uniform( "power", mRoom.getPower() );
	gl::drawSolidRect( mP_FboBounds );
	mP_VelocityShader.unbind();
	
	mP_VelocityFbos[ mThisFbo ].unbindFramebuffer();
}

// PREDATOR POSITION
void FlockingApp::drawIntoPredatorPositionFbo()
{
	gl::setMatricesWindow( mP_FboSize, false );
	gl::setViewport( mP_FboBounds );
	
	mP_PositionFbos[ mThisFbo ].bindFramebuffer();
	mP_PositionFbos[ mPrevFbo ].bindTexture( 0 );
	mP_VelocityFbos[ mThisFbo ].bindTexture( 1 );
	
	mP_PositionShader.bind();
	mP_PositionShader.uniform( "position", 0 );
	mP_PositionShader.uniform( "velocity", 1 );
	mP_PositionShader.uniform( "dt", mRoom.mTimeAdjusted );
	gl::drawSolidRect( mP_FboBounds );
	mP_PositionShader.unbind();
	
	mP_PositionFbos[ mThisFbo ].unbindFramebuffer();
}

void FlockingApp::drawIntoRoomFbo()
{
	gl::setMatricesWindow( mRoomFbo.getSize(), false );
	gl::setViewport( mRoomFbo.getBounds() );
	
	mRoomFbo.bindFramebuffer();
	gl::clear( ColorA( 200.0f, 0.0f, 0.0f, 0.0f ), true );
	
	
	gl::disableAlphaBlending();
	gl::enable( GL_TEXTURE_2D );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	Matrix44f m;
	m.setToIdentity();
	m.scale( mRoom.getDims() );
	
	mLanternsFbo.bindTexture();
	mRoomShader.bind();
	mRoomShader.uniform( "lanternsTex", 0 );
	mRoomShader.uniform( "numLights", (float)mController.mNumLanterns );
	mRoomShader.uniform( "invNumLights", 1.0f/(float)MAX_LANTERNS );
	mRoomShader.uniform( "invNumLightsHalf", 1.0f/(float)MAX_LANTERNS * 0.5f );
	mRoomShader.uniform( "att", 1.25f );
	mRoomShader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
	mRoomShader.uniform( "mMatrix", m );
	mRoomShader.uniform( "eyePos", mActiveCam.mEye );
	mRoomShader.uniform( "roomDims", mRoom.getDims() );
	mRoomShader.uniform( "power", mRoom.getPower() );
	mRoomShader.uniform( "lightPower", mRoom.getLightPower() );
	mRoomShader.uniform( "timePer", mRoom.getTimePer() * 1.5f + 0.5f );
	mRoom.draw(); // Actually draws the room
	mRoomShader.unbind();
	
	mRoomFbo.unbindFramebuffer();
	glDisable( GL_CULL_FACE );
}

void FlockingApp::draw(){
	Area mViewArea0 = Area(0, 0, getWindowSize().x / 2,getWindowSize().y);
	Area mViewArea1 = Area(getWindowSize().x / 2, 0, getWindowSize().x, getWindowSize().y);

	gl::clear( ColorA( 0.1f, 0.1f, 0.1f, 0.0f ), true );

	

	mActiveCam = mHeadCam1;
	drawGuts(mViewArea0);

	mActiveCam = mHeadCam0;
	drawGuts(mViewArea1);
	// Shit's probably gonna get messy with this stuff here,
	//  since we now effectively have two draw passes
	mThisFbo	= ( mThisFbo + 1 ) % 2;
	mPrevFbo	= ( mThisFbo + 1 ) % 2;
}

void FlockingApp::drawGuts(Area area)
{
	if( !mInitUpdateCalled ){
		return;
	}

	// This updates the view that the camera sees based on the camera coordinates.
	drawIntoRoomFbo();
	drawIntoLanternsFbo();

	// This next move resets our matricies, screwing up all our hard projection work. WTF.
	gl::setMatricesWindow( getWindowSize(), false );
	//gl::setViewport( getWindowBounds() );
	gl::setViewport( area ); // This is what it will need to be set as

	//gl::clear( ColorA( 0.1f, 0.1f, 0.1f, 0.0f ), true );
	gl::color( ColorA( 1.0f, 1.0f, 1.0f, 1.0f ) );

	gl::disableDepthRead();
	gl::disableDepthWrite();
	gl::enable( GL_TEXTURE_2D );
	gl::enableAlphaBlending();
	
	
	// DRAW ROOM
	// Hooooaky, so the mRoomFbo gets set up,
	//  then we draw a rectangle and project it as a texture
	//  onto that, then layer shit on top.

	mRoomFbo.bindTexture();
	gl::drawSolidRect( getWindowBounds() );
	
	//gl::setMatrices( mActiveCam.getCam() );

	// DRAW LANTERN GLOWS - I should actually write new .vert shaders for these,
	//  but I'm not gonna.
	//  Ugh, even new shaders didn't work, since I couldn't get the texture to show right? Guh.
	if( mRoom.isPowerOn() ){
		gl::disableDepthWrite();
		gl::enableAdditiveBlending();
		float c =  mRoom.getPower();
		gl::color( Color( c, c, c ) );

		mLanternGlowTex.bind(0);
		mGlowShader.bind();	
		mGlowShader.uniform( "glowTex", 0 );
		mGlowShader.uniform( "roomDims", mRoom.getDims() );
		mGlowShader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
		mController.drawLanternGlows( mActiveCam.mBillboardRight, mActiveCam.mBillboardUp );	
		mGlowShader.unbind();

		drawGlows();
		drawNebulas();
	}	
	
//	gl::setViewport( getWindowBounds() );
	
	gl::enableAlphaBlending();
	gl::enable( GL_TEXTURE_2D );
	gl::enableDepthRead();
	gl::enableDepthWrite();
	
	// DRAW PARTICLES
	mPositionFbos[mPrevFbo].bindTexture( 0 );
	mPositionFbos[mThisFbo].bindTexture( 1 );
	mVelocityFbos[mThisFbo].bindTexture( 2 );
	mLanternsFbo.bindTexture( 3 );
	mShader.bind();
	mShader.uniform( "prevPosition", 0 );
	mShader.uniform( "currentPosition", 1 );
	mShader.uniform( "currentVelocity", 2 );
	mShader.uniform( "lightsTex", 3 );
	mShader.uniform( "numLights", (float)mController.mNumLanterns );
	mShader.uniform( "invNumLights", 1.0f/(float)MAX_LANTERNS );
	mShader.uniform( "invNumLightsHalf", 1.0f/(float)MAX_LANTERNS * 0.5f );
	mShader.uniform( "att", 1.05f );

	mShader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
	
	mShader.uniform( "eyePos", mActiveCam.mEye );
	mShader.uniform( "power", mRoom.getPower() );
	
	gl::draw( mVboMesh ); // This line actually draws our prey
	mShader.unbind(); 

	// DRAW PREDATORS
	mP_PositionFbos[mPrevFbo].bindTexture( 0 );
	mP_PositionFbos[mThisFbo].bindTexture( 1 );
	mP_VelocityFbos[mThisFbo].bindTexture( 2 );
	mLanternsFbo.bindTexture( 3 );
	mP_Shader.bind();
	mP_Shader.uniform( "prevPosition", 0 );
	mP_Shader.uniform( "currentPosition", 1 );
	mP_Shader.uniform( "currentVelocity", 2 );
	mP_Shader.uniform( "lightsTex", 3 );
	mP_Shader.uniform( "numLights", (float)mController.mNumLanterns );
	mP_Shader.uniform( "invNumLights", 1.0f/(float)MAX_LANTERNS );
	mP_Shader.uniform( "invNumLightsHalf", 1.0f/(float)MAX_LANTERNS * 0.5f );
	mP_Shader.uniform( "att", 1.05f );
	mP_Shader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
	mP_Shader.uniform( "eyePos", mActiveCam.mEye );
	mP_Shader.uniform( "power", mRoom.getPower() );
	gl::draw( mP_VboMesh );
	mP_Shader.unbind();
	
	gl::disable( GL_TEXTURE_2D );
	gl::enableDepthWrite();
	gl::enableAdditiveBlending();
	gl::color( Color( 1.0f, 1.0f, 1.0f ) );
	
	// DRAW LANTERNS
	mLanternShader.bind();
	mLanternShader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
	mLanternShader.uniform( "eyePos", mActiveCam.mEye );
	mLanternShader.uniform( "mainPower", mRoom.getPower() );
	mLanternShader.uniform( "roomDim", mRoom.getDims() );
	//mController.drawLanterns( &mLanternShader );
	
	mLanternShader.unbind();

	gl::disableDepthWrite();
	gl::enableAlphaBlending();
	
	// DRAW PANEL
	drawInfoPanel();
		
	if( false ){	// DRAW POSITION AND VELOCITY FBOS
		gl::color( Color::white() );
		gl::setMatricesWindow( getWindowSize() );
		gl::enable( GL_TEXTURE_2D );
		mPositionFbos[ mThisFbo ].bindTexture();
		gl::drawSolidRect( Rectf( 5.0f, 5.0f, 105.0f, 105.0f ) );
		
		mPositionFbos[ mPrevFbo ].bindTexture();
		gl::drawSolidRect( Rectf( 106.0f, 5.0f, 206.0f, 105.0f ) );
		
		mVelocityFbos[ mThisFbo ].bindTexture();
		gl::drawSolidRect( Rectf( 5.0f, 106.0f, 105.0f, 206.0f ) );
		
		mVelocityFbos[ mPrevFbo ].bindTexture();
		gl::drawSolidRect( Rectf( 106.0f, 106.0f, 206.0f, 206.0f ) );
	}
	
	
	
//	if( mSaveFrames ){
//		writeImage( getHomeDirectory() + "Flocking/" + toString( mNumSavedFrames ) + ".png", copyWindowSurface() );
//		mNumSavedFrames ++;
//	}
	
	//if( getElapsedFrames()%60 == 0 ){
	//	console() << "FPS = " << getAverageFps() << std::endl;
	//}
}

void FlockingApp::drawGlows()
{
	mGlowTex.bind( 0 );
	mGlowShader.bind();
	mGlowShader.uniform( "glowTex", 0 );
	mGlowShader.uniform( "roomDims", mRoom.getDims() );
	mGlowShader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
	mController.drawGlows( &mGlowShader, mActiveCam);
	mGlowShader.unbind();
}

void FlockingApp::drawNebulas()
{
	mNebulaTex.bind( 0 );
	mNebulaShader.bind();
	mNebulaShader.uniform( "nebulaTex", 0 );
	mNebulaShader.uniform( "roomDims", mRoom.getDims() );	
	mNebulaShader.uniform( "mvpMatrix", mActiveCam.mMvpMatrix );
	mController.drawNebulas( &mNebulaShader, mActiveCam.mBillboardRight, mActiveCam.mBillboardUp );
	mNebulaShader.unbind();
}

void FlockingApp::drawInfoPanel()
{
	gl::pushMatrices();
	gl::translate( mRoom.getDims() );
	gl::scale( Vec3f( -1.0f, -1.0f, 1.0f ) );
	gl::color( Color( 1.0f, 1.0f, 1.0f ) * ( 1.0 - mRoom.getPower() ) );
	gl::enableAlphaBlending();
	
	float iconWidth		= 50.0f;
	
	float X0			= 15.0f;
	float X1			= X0 + iconWidth;
	float Y0			= 300.0f;
	float Y1			= Y0 + iconWidth;
	
	// DRAW ROOM NUM AND DESC
	float c = mRoom.getPower() * 0.5f + 0.5f;
	gl::color( ColorA( c, c, c, 0.5f ) );
	gl::draw( mIconTex, Rectf( X0, Y0, X1, Y1 ) );
	
	c = mRoom.getPower();
	gl::color( ColorA( c, c, c, 0.5f ) );
	gl::disable( GL_TEXTURE_2D );
	
	// DRAW TIME BAR
	float timePer		= mRoom.getTimePer();
	gl::drawSolidRect( Rectf( Vec2f( X0, Y1 + 2.0f ), Vec2f( X0 + timePer * ( iconWidth ), Y1 + 2.0f + 4.0f ) ) );
	
	// DRAW FPS BAR
	float fpsPer		= getAverageFps()/60.0f;
	gl::drawSolidRect( Rectf( Vec2f( X0, Y1 + 4.0f + 4.0f ), Vec2f( X0 + fpsPer * ( iconWidth ), Y1 + 4.0f + 6.0f ) ) );
	
	
	gl::popMatrices();
}


// HOLDS DATA FOR LANTERNS AND PREDATORS
void FlockingApp::drawIntoLanternsFbo()
{
	Surface32f lanternsSurface( mLanternsFbo.getTexture() );
	Surface32f::Iter it = lanternsSurface.getIter();
	while( it.line() ){
		while( it.pixel() ){
			int index = it.x();
			
			if( it.y() == 0 ){ // set light position
				if( index < (int)mController.mLanterns.size() ){
					it.r() = mController.mLanterns[index].mPos.x;
					it.g() = mController.mLanterns[index].mPos.y;
					it.b() = mController.mLanterns[index].mPos.z;
					it.a() = mController.mLanterns[index].mRadius;
				} else { // if the light shouldnt exist, put it way out there
					it.r() = 0.0f;
					it.g() = 0.0f;
					it.b() = 0.0f;
					it.a() = 1.0f;
				}
			} else {	// set light color
				if( index < (int)mController.mLanterns.size() ){
					it.r() = mController.mLanterns[index].mColor.r;
					it.g() = mController.mLanterns[index].mColor.g;
					it.b() = mController.mLanterns[index].mColor.b;
					it.a() = 1.0f;
				} else { 
					it.r() = 0.0f;
					it.g() = 0.0f;
					it.b() = 0.0f;
					it.a() = 1.0f;
				}
			}
		}
	}

	mLanternsFbo.bindFramebuffer();
	gl::setMatricesWindow( mLanternsFbo.getSize(), false );
	gl::setViewport( mLanternsFbo.getBounds() );
	gl::draw( gl::Texture( lanternsSurface ) );
	mLanternsFbo.unbindFramebuffer();
}



CINDER_APP_BASIC( FlockingApp, RendererGl )

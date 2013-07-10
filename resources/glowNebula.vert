uniform mat4 mvpMatrix;

varying vec4 vVertex;

void main()
{
	vVertex			= vec4( gl_Vertex );
	
	gl_TexCoord[0]	= gl_MultiTexCoord0;
//	gl_Position		= ftransform();
	gl_Position		= mvpMatrix * vVertex;
}
/*  Copyright 2005-2006 Guillaume Duhamel
    Copyright 2005-2006 Theo Berkau
    Copyright 2011-2015 Shinya Miyamoto(devmiyax)

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
   
 
//#ifdef __ANDROID__
#include <stdlib.h>
#include <math.h>
#include "ygl.h"
#include "yui.h"
#include "vidshared.h"
#include "shaders/FXAA_DefaultES.h"

int Ygl_useTmpBuffer();
int YglBlitBlur(u32 srcTexture, u32 targetFbo, float w, float h, float * matrix);
int YglBlitMosaic(u32 srcTexture, u32 targetFbo, float w, float h, float * matrix, int * mosaic);

extern float vdp1wratio;
extern float vdp1hratio;
extern int GlHeight;
extern int GlWidth;
static GLuint _prgid[PG_MAX] = { 0 };


static void Ygl_printShaderError( GLuint shader )
{
  GLsizei bufSize;

  glGetShaderiv(shader, GL_INFO_LOG_LENGTH , &bufSize);

  if (bufSize > 1) {
    GLchar *infoLog;

    infoLog = (GLchar *)malloc(bufSize);
    if (infoLog != NULL) {
      GLsizei length;
      glGetShaderInfoLog(shader, bufSize, &length, infoLog);
	  YGLLOG("Shaderlog:\n%s\n", infoLog);
      free(infoLog);
    }
  }
}

int ShaderDrawTest()
{

	GLuint vertexp = glGetAttribLocation(_prgid[PG_NORMAL], (const GLchar *)"a_position");
	GLuint texcoordp = glGetAttribLocation(_prgid[PG_NORMAL], (const GLchar *)"a_texcoord");
	GLuint mtxModelView = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"u_mvpMatrix");
	GLuint mtxTexture = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"u_texMatrix");

	GLfloat vec[] = { 0.0f, 0.0f, -0.5f, 100.0f, 0.0f, -0.5f, 100.0f, 100.0f, -0.5f,
		0.0f, 0.0f, -0.5f, 100.0f, 100.0f, -0.5f, 0.0f, 100.0f, -0.5f };

	/*
	GLfloat vec[]={ 0.0f,0.0f,-0.5f,
	-1.0f,1.0f,-0.5f,
	1.0f,1.0f,-0.5f,
	0.0f,0.0f,-0.5f,
	-1.0f,-1.0f,-0.5f,
	1.0f,-1.0f,-0.5f,
	};
	*/
	GLfloat tex[] = { 0.0f, 0.0f, 2048.0f, 0.0f, 2048.0f, 1024.0f, 0.0f, 0.0f, 2048.0f, 1024.0f, 0.0f, 1024.0f };

	//   GLfloat tex[]={ 0.0f,0.0f,1.0f,0.0f,1.0f,1.0f,
	//                   0.0f,0.0f,1.0f,1.0f,0.0f,1.0f };

	YglMatrix mtx;
	YglMatrix mtxt;
	GLuint id = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"s_texture");

	YglLoadIdentity(&mtx);
	YglLoadIdentity(&mtxt);

	YglOrtho(&mtx, 0.0f, 100.0f, 100.0f, 0.0f, 1.0f, 0.0f);

	glUseProgram(_prgid[PG_NORMAL]);
	glUniform1i(id, 0);

	glEnableVertexAttribArray(vertexp);
	glEnableVertexAttribArray(texcoordp);

	glUniformMatrix4fv(mtxModelView, 1, GL_FALSE, (GLfloat*)&_Ygl->mtxModelView/*mtx*/.m[0][0]);

	glVertexAttribPointer(vertexp, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)vec);
	glVertexAttribPointer(texcoordp, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)tex);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	return 0;

}


void Ygl_Vdp1CommonGetUniformId(GLuint pgid, YglVdp1CommonParam * param){

	param->sprite = glGetUniformLocation(pgid, (const GLchar *)"u_sprite");
	param->tessLevelInner = glGetUniformLocation(pgid, (const GLchar *)"TessLevelInner");
	param->tessLevelOuter = glGetUniformLocation(pgid, (const GLchar *)"TessLevelOuter");
	param->fbo = glGetUniformLocation(pgid, (const GLchar *)"u_fbo");
	param->fbowidth = glGetUniformLocation(pgid, (const GLchar *)"u_fbowidth");
	param->fboheight = glGetUniformLocation(pgid, (const GLchar *)"u_fbohegiht");
}

int Ygl_uniformVdp1CommonParam(void * p){

	YglProgram * prg;
	YglVdp1CommonParam * param;

	prg = p;
	param = prg->ids;

	glEnableVertexAttribArray(prg->vertexp);
	glEnableVertexAttribArray(prg->texcoordp);
	if (prg->vertexAttribute != NULL)
	{
		glEnableVertexAttribArray(prg->vaid);
	}

	if (param == NULL) return 0;

	if (param->sprite != -1){
		glUniform1i(param->sprite, 0);
	}

	if (param->tessLevelInner != -1) {
		glUniform1f(param->tessLevelInner, (float)TESS_COUNT);
	}

	if (param->tessLevelOuter != -1) {
		glUniform1f(param->tessLevelOuter, (float)TESS_COUNT);
	}

	if (param->fbo != -1){
		glUniform1i(param->fbo, 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _Ygl->vdp1FrameBuff[_Ygl->drawframe]);
		glUniform1i(param->fbowidth, GlWidth);
		glUniform1i(param->fboheight, GlHeight);
#if !defined(_OGLES3_)
		if (glTextureBarrierNV) glTextureBarrierNV();
#endif
		glActiveTexture(GL_TEXTURE0);
	}

	return 0;
}

int Ygl_cleanupVdp1CommonParam(void * p){
	YglProgram * prg;
	prg = p;
	glDisableVertexAttribArray(prg->vaid);
	return 0;
}


/*------------------------------------------------------------------------------------
 *  Normal Draw
 * ----------------------------------------------------------------------------------*/
const GLchar Yglprg_normal_v[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "uniform mat4 u_mvpMatrix;    \n"
      "uniform mat4 u_texMatrix;    \n"
      "layout (location = 0) in vec4 a_position;   \n"
      "layout (location = 1) in vec4 a_texcoord;   \n"
      "out  highp vec4 v_texcoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position*u_mvpMatrix; \n"
      "   v_texcoord  = a_texcoord/*u_texMatrix*/; \n"
      "} ";
const GLchar * pYglprg_normal_v[] = {Yglprg_normal_v, NULL};

const GLchar Yglprg_normal_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                            \n"
"in highp vec4 v_texcoord;                            \n"
"uniform vec4 u_color_offset;    \n"
"uniform sampler2D s_texture;                        \n"
"out vec4 fragColor;            \n"
"void main()                                         \n"
"{                                                   \n"
"  ivec2 addr; \n"
"  addr.x = int(v_texcoord.x);                        \n"
"  addr.y = int(v_texcoord.y);                        \n"
"  vec4 txcol = texelFetch( s_texture, addr,0 );         \n"
"  if(txcol.a > 0.0)\n                                 "
"     fragColor = clamp(txcol+u_color_offset,vec4(0.0),vec4(1.0));\n                         "
"  else \n                                            "
"     discard;\n                                      "
"}                                                   \n";
const GLchar * pYglprg_normal_f[] = {Yglprg_normal_f, NULL};
static int id_normal_s_texture = -1;

int Ygl_uniformNormal(void * p)
{

	YglProgram * prg;
	prg = p;
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glUniform1i(id_normal_s_texture, 0);
	glUniform4fv(prg->color_offset, 1, prg->color_offset_val);
	return 0;
}

int Ygl_cleanupNormal(void * p)
{
	YglProgram * prg;
	prg = p;
	return 0;
}

int Ygl_useTmpBuffer(){
	// Create Screen size frame buffer
	if (_Ygl->tmpfbo == 0) {

		GLuint error;
		glGenTextures(1, &_Ygl->tmpfbotex);
		glBindTexture(GL_TEXTURE_2D, _Ygl->tmpfbotex);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glGetError();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _Ygl->rwidth, _Ygl->rheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		if ((error = glGetError()) != GL_NO_ERROR)
		{
			//YGLDEBUG("Fail on VIDOGLVdp1ReadFrameBuffer at %d %04X %d %d", __LINE__, error, _Ygl->rwidth, _Ygl->rheight);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &_Ygl->tmpfbo);
		glBindFramebuffer(GL_FRAMEBUFFER, _Ygl->tmpfbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _Ygl->tmpfbotex, 0);
	}
	// bind Screen size frame buffer
	else{
		glBindFramebuffer(GL_FRAMEBUFFER, _Ygl->tmpfbo);
	}
	return 0;
}

/*------------------------------------------------------------------------------------
*  Mosaic Draw
* ----------------------------------------------------------------------------------*/
int Ygl_uniformMosaic(void * p)
{
	YglProgram * prg;
	prg = p;

	Ygl_useTmpBuffer();
	glViewport(0, 0, _Ygl->rwidth, _Ygl->rheight);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnableVertexAttribArray(prg->vertexp);
	glEnableVertexAttribArray(prg->texcoordp);
	glUniform1i(id_normal_s_texture, 0);
	glUniform4fv(prg->color_offset, 1, prg->color_offset_val);
	glBindTexture(GL_TEXTURE_2D, YglTM->textureID);

	return 0;
}

int Ygl_cleanupMosaic(void * p)
{
	YglProgram * prg;
	prg = p;

	// Bind Default frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Restore Default Matrix
	glViewport(0, 0, _Ygl->width, _Ygl->height);

	// call blit method
	YglBlitMosaic(_Ygl->tmpfbotex, 0, _Ygl->rwidth, _Ygl->rheight, prg->matrix, prg->mosaic );

	glBindTexture(GL_TEXTURE_2D, YglTM->textureID);

	return 0;
}


/*------------------------------------------------------------------------------------
*  Blur
* ----------------------------------------------------------------------------------*/
int Ygl_uniformNormal_blur(void * p)
{
	YglProgram * prg;
	prg = p;

	Ygl_useTmpBuffer();
	glViewport(0, 0, _Ygl->rwidth, _Ygl->rheight);
	glClearColor(0.0f,0.0f,0.0f,0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnableVertexAttribArray(prg->vertexp);
	glEnableVertexAttribArray(prg->texcoordp);
	glUniform1i(id_normal_s_texture, 0);
	glUniform4fv(prg->color_offset, 1, prg->color_offset_val);
	glBindTexture(GL_TEXTURE_2D, YglTM->textureID);
	return 0;
}

int Ygl_cleanupNormal_blur(void * p)
{
	YglProgram * prg;
	prg = p;

	// Bind Default frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Restore Default Matrix
	glViewport(0, 0, _Ygl->width, _Ygl->height);

	// call blit method
	YglBlitBlur(_Ygl->tmpfbotex, 0, _Ygl->rwidth, _Ygl->rheight, prg->matrix);

	glBindTexture(GL_TEXTURE_2D, YglTM->textureID);

	return 0;
}



const GLchar Yglprg_DestinationAlpha_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                                  \n"
"in highp vec4 v_texcoord;                               \n"
"uniform vec4 u_color_offset;                            \n"
"uniform sampler2D s_texture;                            \n"
"uniform sampler2D s_depth;                              \n"
"uniform sampler2D s_dest;                               \n"
"out vec4 fragColor;                                     \n"
"void main()                                             \n"
"{                                                       \n"
"  ivec2 addr;                                           \n"
"  addr.x = int(v_texcoord.x);                           \n"
"  addr.y = int(v_texcoord.y);                           \n"
"  vec4 txcol = texelFetch( s_texture, addr,0 );         \n"
"  ivec2 screenpos = gl_FragCoord.xy;                    \n"
"  float depth = texelFetch( s_depth, screenpos,0 ).x;   \n"
"  if(txcol.a > 0.0 ){                                   \n"
"     fragColor = clamp(txcol+u_color_offset,vec4(0.0),vec4(1.0)); \n"
"     if( depth == gl_FragCoord.z )                                \n"
"         fragColor.a = texelFetch( s_dest, screenpos,0 ).x        \n"
"  }else \n                                            "
"     discard;\n                                      "
"}                                                   \n";

/*------------------------------------------------------------------------------------
 *  Window Operation
 * ----------------------------------------------------------------------------------*/
const GLchar Yglprg_window_v[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "uniform mat4 u_mvpMatrix;    \n"
      "layout (location = 0) in vec4 a_position;               \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position*u_mvpMatrix; \n"
      "} ";
const GLchar * pYglprg_window_v[] = {Yglprg_window_v, NULL};

const GLchar Yglprg_window_f[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "precision highp float;                            \n"
      "out vec4 fragColor;            \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  fragColor = vec4( 1.0,1.0,1.0,1.0 );\n"
      "}                                                   \n";
const GLchar * pYglprg_window_f[] = {Yglprg_window_f, NULL};

int Ygl_uniformWindow(void * p )
{
   YglProgram * prg;
   prg = p;
   glUseProgram(prg->prgid );
   glEnableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   return 0;
}

int Ygl_cleanupWindow(void * p )
{
   YglProgram * prg;
   prg = p;
   return 0;
}

/*------------------------------------------------------------------------------------
 *  VDP1 Normal Draw
 * ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_normal_v[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "uniform mat4 u_mvpMatrix;    \n"
      "layout (location = 0) in vec4 a_position;   \n"
      "layout (location = 1) in vec4 a_texcoord;   \n"
      "out   vec4 v_texcoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position*u_mvpMatrix; \n"
      "   v_texcoord  = a_texcoord; \n"
      "} ";
const GLchar * pYglprg_vdp1_normal_v[] = {Yglprg_vdp1_normal_v, NULL};

const GLchar Yglprg_vpd1_normal_f[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "precision highp float;                            \n"
      "in vec4 v_texcoord;                            \n"
      "uniform sampler2D s_texture;                        \n"
      "out vec4 fragColor;            \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  vec2 addr = v_texcoord.st;                        \n"
      "  addr.s = addr.s / (v_texcoord.q);                 \n"
      "  addr.t = addr.t / (v_texcoord.q);                 \n"
      "  vec4 FragColor = texture( s_texture, addr );      \n"
      "  /*if( FragColor.a == 0.0 ) discard;*/                \n"
      "  fragColor = FragColor;\n "
      "}                                                   \n";
const GLchar * pYglprg_vdp1_normal_f[] = {Yglprg_vpd1_normal_f, NULL};
static int id_vdp1_normal_s_texture = -1;

int Ygl_uniformVdp1Normal(void * p )
{
   YglProgram * prg;
   prg = p;
   glEnableVertexAttribArray(prg->vertexp);
   glEnableVertexAttribArray(prg->texcoordp);
   glUniform1i(id_vdp1_normal_s_texture, 0);
   return 0;
}

int Ygl_cleanupVdp1Normal(void * p )
{
   YglProgram * prg;
   prg = p;
   return 0;
}


/*------------------------------------------------------------------------------------
*  VDP1 GlowShading Operation with tessellation
* ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_gouraudshading_tess_v[] =
#if defined(_OGLES3_)
"#version 320 es \n"
#else
"#version 400 \n"
#endif
"layout (location = 0) in vec3 a_position; \n"
"layout (location = 1) in vec4 a_texcoord; \n"
"layout (location = 2) in vec4 a_grcolor;  \n"
"out vec3 v_position;  \n"
"out vec4 v_texcoord; \n"
"out vec4 v_vtxcolor; \n"
"void main() {                \n"
"   v_position  = a_position; \n"
"   v_vtxcolor  = a_grcolor;  \n"
"   v_texcoord  = a_texcoord; \n"
"}\n";
const GLchar * pYglprg_vdp1_gouraudshading_tess_v[] = { Yglprg_vdp1_gouraudshading_tess_v, NULL };

const GLchar Yglprg_tess_c[] =
#if defined(_OGLES3_)
"#version 320 es \n"
#else
"#version 400 \n"
#endif
"layout(vertices = 4) out; //<???? what does it means? \n"
"in vec3 v_position[];  \n"
"in vec4 v_texcoord[]; \n"
"in vec4 v_vtxcolor[]; \n"
"out vec3 tcPosition[]; \n"
"out vec4 tcTexCoord[]; \n"
"out vec4 tcColor[]; \n"
"uniform float TessLevelInner; \n"
"uniform float TessLevelOuter; \n"
" \n"
"#define ID gl_InvocationID \n"
" \n"
"void main()  \n"
"{  \n"
"	tcPosition[ID] = v_position[ID];  \n"
"	tcTexCoord[ID] = v_texcoord[ID];  \n"
"	tcColor[ID] = v_vtxcolor[ID];  \n"
" \n"
"	if (ID == 0) {  \n"
"		gl_TessLevelInner[0] = TessLevelInner;  \n"
"		gl_TessLevelInner[1] = TessLevelInner;  \n"
"		gl_TessLevelOuter[0] = TessLevelOuter;  \n"
"		gl_TessLevelOuter[1] = TessLevelOuter; \n"
"		gl_TessLevelOuter[2] = TessLevelOuter; \n"
"		gl_TessLevelOuter[3] = TessLevelOuter; \n"
"	} \n"
"} \n";
const GLchar * pYglprg_vdp1_gouraudshading_tess_c[] = { Yglprg_tess_c, NULL };

const GLchar Yglprg_tess_e[] =
#if defined(_OGLES3_)
"#version 320 es \n"
#else
"#version 400 \n"
#endif
"layout(quads, equal_spacing, ccw) in; \n"
"in vec3 tcPosition[]; \n"
"in vec4 tcTexCoord[]; \n"
"in vec4 tcColor[]; \n"
"out vec4 teTexCoord; \n"
"out vec4 teColor; \n"
"uniform mat4 u_mvpMatrix; \n"
" \n"
"void main() \n"
"{ \n"
"	float u = gl_TessCoord.x, v = gl_TessCoord.y; \n"
"	vec3 tePosition; \n"
"	vec3 a = mix(tcPosition[0], tcPosition[3], u); \n"
"	vec3 b = mix(tcPosition[1], tcPosition[2], u); \n"
"	tePosition = mix(a, b, v); \n"
"	gl_Position = vec4(tePosition, 1)*u_mvpMatrix; \n"
"	vec4 ta = mix(tcTexCoord[0], tcTexCoord[3], u); \n"
"	vec4 tb = mix(tcTexCoord[1], tcTexCoord[2], u); \n"
"	teTexCoord = mix(ta, tb, v); \n"
"	vec4 ca = mix(tcColor[0], tcColor[3], u); \n"
"	vec4 cb = mix(tcColor[1], tcColor[2], u); \n"
"	teColor = mix(ca, cb, v); \n"
"} \n";
const GLchar * pYglprg_vdp1_gouraudshading_tess_e[] = { Yglprg_tess_e, NULL };

const GLchar Yglprg_tess_g[] =
#if defined(_OGLES3_)
"#version 320 es \n"
#else
"#version 400 \n"
#endif
"uniform mat4 Modelview; \n"
"uniform mat3 NormalMatrix; \n"
"layout(triangles) in; \n"
"layout(triangle_strip, max_vertices = 3) out; \n"
"in vec4 teTexCoord[3]; \n"
"in vec4 teColor[3]; \n"
"out vec4 v_texcoord; \n"
"out vec4 v_vtxcolor; \n"
" \n"
"void main() \n"
"{ \n"
"	v_texcoord = teTexCoord[0]; \n"
"	v_vtxcolor = teColor[0]; \n"
"	gl_Position = gl_in[0].gl_Position; EmitVertex(); \n"
" \n"
"	v_texcoord = teTexCoord[1]; \n"
"	v_vtxcolor = teColor[1]; \n"
"	gl_Position = gl_in[1].gl_Position; EmitVertex(); \n"
" \n"
"	v_texcoord = teTexCoord[2]; \n"
"	v_vtxcolor = teColor[2]; \n"
"	gl_Position = gl_in[2].gl_Position; EmitVertex(); \n"
" \n"
"	EndPrimitive(); \n"
"} \n";
const GLchar * pYglprg_vdp1_gouraudshading_tess_g[] = { Yglprg_tess_g, NULL };

static YglVdp1CommonParam id_gt = { 0 };


/*------------------------------------------------------------------------------------
 *  VDP1 GlowShading Operation
 * ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_gouraudshading_v[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"uniform mat4 u_mvpMatrix;                \n"
"uniform mat4 u_texMatrix;                \n"
"layout (location = 0) in vec4 a_position;               \n"
"layout (location = 1) in vec4 a_texcoord;               \n"
"layout (location = 2) in vec4 a_grcolor;                \n"
"out  vec4 v_texcoord;               \n"
"out  vec4 v_vtxcolor;               \n"
"void main() {                            \n"
"   v_vtxcolor  = a_grcolor;              \n"
"   v_texcoord  = a_texcoord; \n"
"   gl_Position = a_position*u_mvpMatrix; \n"
"}\n";
const GLchar * pYglprg_vdp1_gouraudshading_v[] = {Yglprg_vdp1_gouraudshading_v, NULL};

const GLchar Yglprg_vdp1_gouraudshading_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                                                 \n"
"uniform sampler2D u_sprite;                                              \n"
"in vec4 v_texcoord;                                                 \n"
"in vec4 v_vtxcolor;                                                 \n"
"out vec4 fragColor;            \n"
"void main() {                                                            \n"
"  vec2 addr = v_texcoord.st;                                             \n"
"  addr.s = addr.s / (v_texcoord.q);                                      \n"
"  addr.t = addr.t / (v_texcoord.q);                                      \n"
"  vec4 spriteColor = texture(u_sprite,addr);                           \n"
"  if( spriteColor.a == 0.0 ) discard;                                      \n"
"  fragColor   = spriteColor;                                          \n"
"  fragColor  = clamp(spriteColor+v_vtxcolor,vec4(0.0),vec4(1.0));     \n"
"  fragColor.a = spriteColor.a;                                        \n"
"  //fragColor = vec4(1.0,0.0,0.0,1.0);                                        \n"
"}\n";
const GLchar * pYglprg_vdp1_gouraudshading_f[] = {Yglprg_vdp1_gouraudshading_f, NULL};

static YglVdp1CommonParam id_g = { 0 };


/*------------------------------------------------------------------------------------
 *  VDP1 GlowShading and Half Trans Operation
 * ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_gouraudshading_hf_v[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "uniform mat4 u_mvpMatrix;                \n"
      "uniform mat4 u_texMatrix;                \n"
      "layout (location = 0) in vec4 a_position;               \n"
      "layout (location = 1) in vec4 a_texcoord;               \n"
      "layout (location = 2) in vec4 a_grcolor;                \n"
      "out  vec4 v_texcoord;               \n"
      "out  vec4 v_vtxcolor;               \n"
      "void main() {                            \n"
      "   v_vtxcolor  = a_grcolor;              \n"
      "   v_texcoord  = a_texcoord/*u_texMatrix*/; \n"
      "   gl_Position = a_position*u_mvpMatrix; \n"
      "}\n";
const GLchar * pYglprg_vdp1_gouraudshading_hf_v[] = {Yglprg_vdp1_gouraudshading_hf_v, NULL};

const GLchar Yglprg_vdp1_gouraudshading_hf_f[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "precision highp float;                                                                     \n"
      "uniform sampler2D u_sprite;                                                                  \n"
      "uniform sampler2D u_fbo;                                                                     \n"
      "uniform int u_fbowidth;                                                                      \n"
      "uniform int u_fbohegiht;                                                                     \n"
      "in vec4 v_texcoord;                                                                     \n"
      "in vec4 v_vtxcolor;                                                                     \n"
      "out vec4 fragColor; \n "
      "void main() {                                                                                \n"
      "  vec2 addr = v_texcoord.st;                                                                 \n"
      "  vec2 faddr = vec2( gl_FragCoord.x/float(u_fbowidth), gl_FragCoord.y/float(u_fbohegiht));   \n"
      "  addr.s = addr.s / (v_texcoord.q);                                                          \n"
      "  addr.t = addr.t / (v_texcoord.q);                                                          \n"
      "  vec4 spriteColor = texture(u_sprite,addr);                                               \n"
      "  if( spriteColor.a == 0.0 ) discard;                                                          \n"
      "  vec4 fboColor    = texture(u_fbo,faddr);                                                 \n"
      "  spriteColor += vec4(v_vtxcolor.r,v_vtxcolor.g,v_vtxcolor.b,0.0);                           \n"
      "  if( fboColor.a > 0.0 && spriteColor.a > 0.0 )                                              \n"
      "  {                                                                                          \n"
      "    fragColor = spriteColor*0.5 + fboColor*0.5;                                           \n"
      "    fragColor.a = fboColor.a;                                                             \n"
      "  }else{                                                                                     \n"
      "    fragColor = spriteColor;                                                              \n"
      "  }                                                                                          \n"
      "}\n";
const GLchar * pYglprg_vdp1_gouraudshading_hf_f[] = {Yglprg_vdp1_gouraudshading_hf_f, NULL};


static YglVdp1CommonParam id_ght = { 0 };
static YglVdp1CommonParam id_ght_tess = { 0 };



/*------------------------------------------------------------------------------------
 *  VDP1 Half Trans Operation
 * ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_halftrans_v[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
        "uniform mat4 u_mvpMatrix;                \n"
        "layout (location = 0) in vec4 a_position;               \n"
        "layout (location = 1) in vec4 a_texcoord;               \n"
        "layout (location = 2) in vec4 a_grcolor;                \n"
        "out  vec4 v_texcoord;               \n"
        "out  vec4 v_vtxcolor;               \n"
        "void main() {                            \n"
        "   v_vtxcolor  = a_grcolor;              \n"
        "   v_texcoord  = a_texcoord/*u_texMatrix*/; \n"
        "   gl_Position = a_position*u_mvpMatrix; \n"
        "}\n";

const GLchar * pYglprg_vdp1_halftrans_v[] = {Yglprg_vdp1_halftrans_v, NULL};

const GLchar Yglprg_vdp1_halftrans_f[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "precision highp float;                                                                     \n"
      "uniform sampler2D u_sprite;                                                                  \n"
      "uniform sampler2D u_fbo;                                                                     \n"
      "uniform int u_fbowidth;                                                                      \n"
      "uniform int u_fbohegiht;                                                                     \n"
      "in vec4 v_texcoord;                                                                     \n"
      "out vec4 fragColor; \n "
      "void main() {                                                                                \n"
      "  vec2 addr = v_texcoord.st;                                                                 \n"
      "  vec2 faddr = vec2( gl_FragCoord.x/float(u_fbowidth), gl_FragCoord.y/float(u_fbohegiht));   \n"
      "  addr.s = addr.s / (v_texcoord.q);                                                          \n"
      "  addr.t = addr.t / (v_texcoord.q);                                                          \n"
      "  vec4 spriteColor = texture(u_sprite,addr);                                               \n"
      "  if( spriteColor.a == 0.0 ) discard;                                                          \n"
      "  vec4 fboColor    = texture(u_fbo,faddr);                                                 \n"
      "  if( fboColor.a > 0.0 && spriteColor.a > 0.0 )                                              \n"
      "  {                                                                                          \n"
      "    fragColor = spriteColor*0.5 + fboColor*0.5;                                           \n"
      "    fragColor.a = fboColor.a;                                                             \n"
      "  }else{                                                                                     \n"
      "    fragColor = spriteColor;                                                              \n"
      "  }                                                                                          \n"
      "}\n";
const GLchar * pYglprg_vdp1_halftrans_f[] = {Yglprg_vdp1_halftrans_f, NULL};

static YglVdp1CommonParam hf = {0};
static YglVdp1CommonParam hf_tess = {0};

/*------------------------------------------------------------------------------------
*  VDP1 Mesh Operaion
* ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_mesh_v[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"uniform mat4 u_mvpMatrix;                \n"
"uniform mat4 u_texMatrix;                \n"
"layout (location = 0) in vec4 a_position;               \n"
"layout (location = 1) in vec4 a_texcoord;               \n"
"layout (location = 2) in vec4 a_grcolor;                \n"
"out  vec4 v_texcoord;               \n"
"out  vec4 v_vtxcolor;               \n"
"void main() {                            \n"
"   v_vtxcolor  = a_grcolor;              \n"
"   v_texcoord  = a_texcoord/*u_texMatrix*/; \n"
"   gl_Position = a_position*u_mvpMatrix; \n"
"}\n";
const GLchar * pYglprg_vdp1_mesh_v[] = { Yglprg_vdp1_mesh_v, NULL };

const GLchar Yglprg_vdp1_mesh_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                                                                     \n"
"uniform sampler2D u_sprite;                                                                  \n"
"uniform sampler2D u_fbo;                                                                     \n"
"uniform int u_fbowidth;                                                                      \n"
"uniform int u_fbohegiht;                                                                     \n"
"in vec4 v_texcoord;                                                                     \n"
"in vec4 v_vtxcolor;                                                                     \n"
"out vec4 fragColor; \n "
"void main() {                                                                                \n"
"  vec2 addr = v_texcoord.st;                                                                 \n"
"  vec2 faddr = vec2( gl_FragCoord.x/float(u_fbowidth), gl_FragCoord.y/float(u_fbohegiht));   \n"
"  addr.s = addr.s / (v_texcoord.q);                                                          \n"
"  addr.t = addr.t / (v_texcoord.q);                                                          \n"
"  vec4 spriteColor = texture(u_sprite,addr);                                               \n"
"  if( spriteColor.a == 0.0 ) discard;                                                          \n"
"  vec4 fboColor    = texture(u_fbo,faddr);                                                 \n"
"  spriteColor += vec4(v_vtxcolor.r,v_vtxcolor.g,v_vtxcolor.b,0.0);                           \n"
"  if( fboColor.a > 0.0  )                                                               \n"
"  {                                                                                          \n"
"    fragColor = spriteColor*0.5 + fboColor*0.5;                                           \n"
"    fragColor.a = fboColor.a;                                                             \n"
"  }else{                                                                                     \n"
"    fragColor = spriteColor;                                                              \n"
"    int additional = int(spriteColor.a * 255.0);                                            \n"
"    fragColor.a = (120.0 + float(additional & 0x07)) / 255.0 ;                              \n"
"  }                                                                                          \n"
"}\n";
const GLchar * pYglprg_vdp1_mesh_f[] = { Yglprg_vdp1_mesh_f, NULL };

static YglVdp1CommonParam mesh = { 0 };
static YglVdp1CommonParam mesh_tess = { 0 };


/*------------------------------------------------------------------------------------
*  VDP1 Shadow Operation
*    hard/vdp1/hon/p06_37.htm
* ----------------------------------------------------------------------------------*/
const GLchar Yglprg_vdp1_shadow_v[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"uniform mat4 u_mvpMatrix;                \n"
"layout (location = 0) in vec4 a_position;               \n"
"layout (location = 1) in vec4 a_texcoord;               \n"
"layout (location = 2) in vec4 a_grcolor;                \n"
"out  vec4 v_texcoord;               \n"
"out  vec4 v_vtxcolor;               \n"
"void main() {                            \n"
"   v_vtxcolor  = a_grcolor;              \n"
"   v_texcoord  = a_texcoord/*u_texMatrix*/; \n"
"   gl_Position = a_position*u_mvpMatrix; \n"
"}\n";

const GLchar * pYglprg_vdp1_shadow_v[] = { Yglprg_vdp1_shadow_v, NULL };

const GLchar Yglprg_vdp1_shadow_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                                                                     \n"
"uniform sampler2D u_sprite;                                                                  \n"
"uniform sampler2D u_fbo;                                                                     \n"
"uniform int u_fbowidth;                                                                      \n"
"uniform int u_fbohegiht;                                                                     \n"
"in vec4 v_texcoord;                                                                     \n"
"out vec4 fragColor; \n "
"void main() {                                                                                \n"
"  vec2 addr = v_texcoord.st;                                                                 \n"
"  vec2 faddr = vec2( gl_FragCoord.x/float(u_fbowidth), gl_FragCoord.y/float(u_fbohegiht));   \n"
"  addr.s = addr.s / (v_texcoord.q);                                                          \n"
"  addr.t = addr.t / (v_texcoord.q);                                                          \n"
"  vec4 spriteColor = texture(u_sprite,addr);                                               \n"
"  if( spriteColor.a == 0.0 ) discard;                                                          \n"
"  vec4 fboColor    = texture(u_fbo,faddr);                                                 \n"
"  if( fboColor.a > 0.0 && spriteColor.a > 0.0 )                                              \n"
"  {                                                                                          \n"
"    fragColor = vec4(fboColor.r*0.5,fboColor.g*0.5,fboColor.b*0.5,fboColor.a);                                           \n"
"  }else{                                                                                     \n"
"  }                                                                                          \n"
"}\n";
const GLchar * pYglprg_vdp1_shadow_f[] = { Yglprg_vdp1_shadow_f, NULL };

static YglVdp1CommonParam shadow = { 0 };
static YglVdp1CommonParam shadow_tess = { 0 };


/*------------------------------------------------------------------------------------
 *  VDP1 UserClip Operation
 * ----------------------------------------------------------------------------------*/
int Ygl_uniformStartUserClip(void * p )
{
   YglProgram * prg;
   prg = p;

   glEnableVertexAttribArray(0);
   glDisableVertexAttribArray(1);

   if( prg->ux1 != -1 )
   {

      GLint vertices[12];
      glColorMask( GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE );
      glStencilMask(0xffffffff);
      glClearStencil(0);
      glClear(GL_STENCIL_BUFFER_BIT);
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_ALWAYS,0x1,0x01);
      glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);

      // render
      vertices[0] = (int)((float)prg->ux1 * vdp1wratio);
      vertices[1] = (int)((float)prg->uy1 * vdp1hratio);
      vertices[2] = (int)((float)(prg->ux2+1) * vdp1wratio);
      vertices[3] = (int)((float)prg->uy1 * vdp1hratio);
      vertices[4] = (int)((float)(prg->ux2+1) * vdp1wratio);
      vertices[5] = (int)((float)(prg->uy2+1) * vdp1hratio);

      vertices[6] = (int)((float)prg->ux1 * vdp1wratio);
      vertices[7] = (int)((float)prg->uy1 * vdp1hratio);
      vertices[8] = (int)((float)(prg->ux2+1) * vdp1wratio);
      vertices[9] = (int)((float)(prg->uy2+1) * vdp1hratio);
      vertices[10] = (int)((float)prg->ux1 * vdp1wratio);
      vertices[11] = (int)((float)(prg->uy2+1) * vdp1hratio);

      glUniformMatrix4fv( prg->mtxModelView, 1, GL_FALSE, (GLfloat*) &_Ygl->mtxModelView.m[0][0]  );
      glVertexAttribPointer(prg->vertexp,2, GL_INT,GL_FALSE, 0, (GLvoid*)vertices );

      glDrawArrays(GL_TRIANGLES, 0, 6);

      glColorMask( GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE );
      glStencilFunc(GL_ALWAYS,0,0x0);
      glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
      glDisable(GL_STENCIL_TEST);
   }

   glEnable(GL_STENCIL_TEST);
   glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
   if( prg->uClipMode == 0x02 )
   {
      glStencilFunc(GL_EQUAL,0x1,0xFF);
   }else if( prg->uClipMode == 0x03 )
   {
      glStencilFunc(GL_EQUAL,0x0,0xFF);
   }else{
      glStencilFunc(GL_ALWAYS,0,0xFF);
   }

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);

   //glDisable(GL_STENCIL_TEST);

   return 0;
}

int Ygl_cleanupStartUserClip(void * p ){return 0;}

int Ygl_uniformEndUserClip(void * p )
{

   YglProgram * prg;
   prg = p;
   glDisable(GL_STENCIL_TEST);
   glStencilFunc(GL_ALWAYS,0,0xFF);

   return 0;
}

int Ygl_cleanupEndUserClip(void * p ){return 0;}


int Ygl_uniformStartVDP2Window(void * p )
{
   YglProgram * prg;
   prg = p;

   glEnable(GL_STENCIL_TEST);
   glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);

   //glEnableVertexAttribArray(0);
   //glDisableVertexAttribArray(1);


   if( prg->bwin0 && !prg->bwin1 )
   {
      if( prg->logwin0 )
      {
         glStencilFunc(GL_EQUAL,0x01,0x01);
      }else{
         glStencilFunc(GL_NOTEQUAL,0x01,0x01);
      }
   }else if( !prg->bwin0 && prg->bwin1 ) {

      if( prg->logwin1 )
      {
         glStencilFunc(GL_EQUAL,0x02,0x02);
      }else{
         glStencilFunc(GL_NOTEQUAL,0x02,0x02);
      }
   }else if( prg->bwin0 && prg->bwin1 ) {
       // and
      if( prg->winmode == 0x0 )
      {
		  if (prg->logwin0 == 1 && prg->logwin0 == 1){
			  glStencilFunc(GL_EQUAL, 0x03, 0x03);
		  }
		  else if (prg->logwin0 == 0 && prg->logwin0 == 0){ 
			  glStencilFunc(GL_NOTEQUAL, 0x03, 0x03);
		  }else{
			  glStencilFunc(GL_ALWAYS, 0, 0xFF);
		  }
      // OR
      }else if( prg->winmode == 0x01 )
      {
		  if (prg->logwin0 == 1 && prg->logwin0 == 1){
			  glStencilFunc(GL_LEQUAL, 0x01, 0x03);
		  }
		  else if (prg->logwin0 == 0 && prg->logwin0 == 0){
			  glStencilFunc(GL_GREATER, 0x01, 0x03);
		  }
		  else{
			  glStencilFunc(GL_ALWAYS, 0, 0xFF);
		  }
      }
   }

   return 0;
}

int Ygl_cleanupStartVDP2Window(void * p ){return 0;}

int Ygl_uniformEndVDP2Window(void * p )
{

   YglProgram * prg;
   prg = p;
   glDisable(GL_STENCIL_TEST);
   glStencilFunc(GL_ALWAYS,0,0xFF);

   return 0;
}

int Ygl_cleanupEndVDP2Window(void * p ){return 0;}


/*------------------------------------------------------------------------------------
 *  VDP2 Draw Frame buffer Operation
 * ----------------------------------------------------------------------------------*/
static int idvdp1FrameBuffer;
static int idfrom;
static int idto;
static int idcoloroffset;

const GLchar Yglprg_vdp1_drawfb_v[] =
#if defined(_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
      "uniform mat4 u_mvpMatrix;                \n"
      "layout (location = 0) in vec4 a_position;               \n"
      "layout (location = 1) in vec2 a_texcoord;               \n"
      "out vec2 v_texcoord;                 \n"
      "void main() {                            \n"
      "   v_texcoord  = a_texcoord;             \n"
      "   gl_Position = a_position*u_mvpMatrix; \n"
      "}\n";
const GLchar * pYglprg_vdp2_drawfb_v[] = {Yglprg_vdp1_drawfb_v, NULL};

const GLchar Yglprg_vdp2_drawfb_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;\n"
"precision highp sampler2D; \n"
"in vec2 v_texcoord;\n"
"uniform sampler2D s_vdp1FrameBuffer;\n"
"uniform float u_from;\n"
"uniform float u_to;\n"
"uniform vec4 u_coloroffset;\n"
"out vec4 fragColor;\n"
"void main()\n"
"{\n"
"  vec2 addr = v_texcoord;\n"
"  highp vec4 fbColor = texture(s_vdp1FrameBuffer,addr);\n"
"  int additional = int(fbColor.a * 255.0);\n"
"  highp float alpha = float((additional/8)*8)/255.0;\n"
"  highp float depth = (float(additional&0x07)/10.0) + 0.05;\n"
"  if( depth < u_from || depth > u_to ){ discard;return;}\n"
"  if( alpha > 0.0){\n"
"     fragColor = fbColor;\n"
"     fragColor += u_coloroffset;  \n"
"     fragColor.a = alpha + 7.0/255.0;\n"
"     gl_FragDepth =  (depth+1.0)/2.0;\n"
"  } else { \n"
"     discard;\n"
"  }\n"
"}\n";

const GLchar * pYglprg_vdp2_drawfb_f[] = {Yglprg_vdp2_drawfb_f, NULL};

void Ygl_uniformVDP2DrawFramebuffer( void * p, float from, float to , float * offsetcol )
{
   YglProgram * prg;
   prg = p;

   glUseProgram(_prgid[PG_VDP2_DRAWFRAMEBUFF]);
   glUniform1i(idvdp1FrameBuffer, 0);
   glActiveTexture(GL_TEXTURE0);
   glUniform1f(idfrom,from);
   glUniform1f(idto,to);
   glUniform4fv(idcoloroffset,1,offsetcol);
   glEnableVertexAttribArray(prg->vertexp);
   glEnableVertexAttribArray(prg->texcoordp);
   _Ygl->renderfb.mtxModelView = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF], (const GLchar *)"u_mvpMatrix");
}

/*------------------------------------------------------------------------------------
*  VDP2 Draw Frame buffer Operation( with Line color insert )
* ----------------------------------------------------------------------------------*/
static int idvdp1FrameBuffer_linecolor;
static int idfrom_linecolor;
static int idto_linecolor;
static int idcoloroffset_linecolor;
static int id_fblinecol_s_line;
static int id_fblinecol_emu_height;
static int id_fblinecol_vheight;

const GLchar * pYglprg_vdp2_drawfb_linecolor_v[] = { Yglprg_vdp1_drawfb_v, NULL };

const GLchar Yglprg_vdp2_drawfb_linecolor_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;\n"
"precision highp sampler2D; \n"
"in vec2 v_texcoord;                             \n"
"uniform sampler2D s_vdp1FrameBuffer;                 \n"
"uniform float u_from;                                  \n"
"uniform float u_to;                                    \n"
"uniform vec4 u_coloroffset;                            \n"
"uniform float u_emu_height;    \n"
"uniform sampler2D s_line;                        \n"
"uniform float u_vheight; \n"
"out vec4 fragColor;            \n"
"void main()                                          \n"
"{                                                    \n"
"  vec2 addr = v_texcoord;                         \n"
"  highp vec4 fbColor = texture(s_vdp1FrameBuffer,addr);  \n"
"  int additional = int(fbColor.a * 255.0);           \n"
"  highp float alpha = float((additional/8)*8)/255.0;  \n"
"  highp float depth = (float(additional&0x07)/10.0) + 0.05; \n"
"  if( depth < u_from || depth > u_to ){ discard;return;} \n"
"  ivec2 linepos; \n "
"  linepos.y = 0; \n "
"  linepos.x = int((u_vheight - gl_FragCoord.y) * u_emu_height);\n"
"  vec4 lncol = texelFetch( s_line, linepos,0 );      \n"
"  if( alpha > 0.0){ \n"
"     fragColor = fbColor;                            \n"
"     fragColor += u_coloroffset;  \n"
"     fragColor += lncol; \n"
"     fragColor.a = alpha + 7.0/255.0; /*1.0;*/ \n"
"     gl_FragDepth =  (depth+1.0)/2.0;\n"
"  } else { \n"
"     discard;\n"
"  }\n"
"}                                                    \n";

const GLchar * pYglprg_vdp2_drawfb_linecolor_f[] = { Yglprg_vdp2_drawfb_linecolor_f, NULL };

void Ygl_uniformVDP2DrawFramebuffer_linecolor(void * p, float from, float to, float * offsetcol)
{
  YglProgram * prg;
  prg = p;

  glUseProgram(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR]);
  glUniform1i(idvdp1FrameBuffer_linecolor, 0);
  glActiveTexture(GL_TEXTURE0);
  glUniform1f(idfrom_linecolor, from);
  glUniform1f(idto_linecolor, to);
  glUniform4fv(idcoloroffset_linecolor, 1, offsetcol);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glUniform1i(id_fblinecol_s_line, 1);
  glUniform1f(id_fblinecol_emu_height, (float)_Ygl->rheight/(float)_Ygl->height);
  glUniform1f(id_fblinecol_vheight, (float)_Ygl->height);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _Ygl->lincolor_tex);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_BLEND);
  _Ygl->renderfb.mtxModelView = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"u_mvpMatrix");

}

/*------------------------------------------------------------------------------------
*  VDP2 Draw Frame buffer Operation( with Line color insert blend with line color alpha value )
* ----------------------------------------------------------------------------------*/
static int idvdp1FrameBuffer_linecolor_destination_alpha;
static int idfrom_linecolor_destination_alpha;
static int idto_linecolor_destination_alpha;
static int idcoloroffset_linecolor_destination_alpha;
static int id_fblinecol_s_line_destination_alpha;
static int id_fblinecol_emu_height_destination_alpha;
static int id_fblinecol_vheight_destination_alpha;

const GLchar * pYglprg_vdp2_drawfb_linecolor_destination_alpha_v[] = { Yglprg_vdp1_drawfb_v, NULL };

const GLchar Yglprg_vdp2_drawfb_linecolor_destination_alpha_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;\n"
"precision highp sampler2D; \n"
"in vec2 v_texcoord;                             \n"
"uniform sampler2D s_vdp1FrameBuffer;                 \n"
"uniform float u_from;                                  \n"
"uniform float u_to;                                    \n"
"uniform vec4 u_coloroffset;                            \n"
"uniform float u_emu_height;    \n"
"uniform sampler2D s_line;                        \n"
"uniform float u_vheight; \n"
"out vec4 fragColor;            \n"
"void main()                                          \n"
"{                                                    \n"
"  vec2 addr = v_texcoord;                         \n"
"  highp vec4 fbColor = texture(s_vdp1FrameBuffer,addr);  \n"
"  int additional = int(fbColor.a * 255.0);           \n"
"  highp float alpha = float((additional/8)*8)/255.0;  \n"
"  highp float depth = (float(additional&0x07)/10.0) + 0.05; \n"
"  if( depth < u_from || depth > u_to ){ discard;return;} \n"
"  ivec2 linepos; \n "
"  linepos.y = 0; \n "
"  linepos.x = int((u_vheight - gl_FragCoord.y) * u_emu_height);\n"
"  vec4 lncol = texelFetch( s_line, linepos,0 );      \n"
"  if( alpha > 0.0){ \n"
"     fragColor = fbColor;                            \n"
"     fragColor += u_coloroffset;  \n"
"     fragColor = (lncol*lncol.a) + fragColor*(1.0-lncol.a); \n"
"     fragColor.a = alpha + 7.0/255.0; /*1.0;*/ \n"
"     gl_FragDepth =  (depth+1.0)/2.0;\n"
"  } else { \n"
"     discard;\n"
"  }\n"
"}                                                    \n";

const GLchar * pYglprg_vdp2_drawfb_linecolor_destination_alpha_f[] = { Yglprg_vdp2_drawfb_linecolor_destination_alpha_f, NULL };

void Ygl_uniformVDP2DrawFramebuffer_linecolor_destination_alpha(void * p, float from, float to, float * offsetcol)
{
	YglProgram * prg;
	prg = p;

	glUseProgram(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA]);
	glUniform1i(idvdp1FrameBuffer_linecolor_destination_alpha, 0);
	glActiveTexture(GL_TEXTURE0);
	glUniform1f(idfrom_linecolor_destination_alpha, from);
	glUniform1f(idto_linecolor_destination_alpha, to);
	glUniform4fv(idcoloroffset_linecolor_destination_alpha, 1, offsetcol);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glUniform1i(id_fblinecol_s_line_destination_alpha, 1);
	glUniform1f(id_fblinecol_emu_height_destination_alpha, (float)_Ygl->rheight / (float)_Ygl->height);
	glUniform1f(id_fblinecol_vheight_destination_alpha, (float)_Ygl->height);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _Ygl->lincolor_tex);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_BLEND);
	_Ygl->renderfb.mtxModelView = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"u_mvpMatrix");

}


const GLchar Yglprg_vdp2_drawfb_addcolor_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;\n"
"precision highp sampler2D; \n"
"in vec2 v_texcoord;\n"
"uniform sampler2D s_vdp1FrameBuffer;\n"
"uniform float u_from;\n"
"uniform float u_to;\n"
"uniform vec4 u_coloroffset;\n"
"out vec4 fragColor;\n"
"void main()\n"
"{\n"
"  vec2 addr = v_texcoord;\n"
"  highp vec4 fbColor = texture(s_vdp1FrameBuffer,addr);\n"
"  highp int additional = int(fbColor.a * 255.0);\n"
"  highp float alpha = float((additional/8)*8)/255.0;\n"
"  highp float depth = ((float(additional&0x07))/10.0) + 0.05;\n"
"  //highp float dv=float(additional-(additional/8*8)); \n"
"  //highp float depth = (dv+1.0)/10.0 + 0.05; \n"
"  if( depth < u_from || depth > u_to ){ discard;return;}\n"
"  if( alpha <= 0.0){\n"
"     discard;\n"
"  }else if( alpha >= 0.75){\n"
"     fragColor = fbColor;\n"
"     fragColor += u_coloroffset;  \n"
"     fragColor.a = 0.0;\n"
"     gl_FragDepth =  (depth+1.0)/2.0;\n"
"  }else{\n"
"     fragColor = fbColor;\n"
"     fragColor += u_coloroffset;\n"
"     fragColor.a = 1.0;\n"
"     gl_FragDepth =  (depth+1.0)/2.0;\n"
"  }\n " 
"}\n";

const GLchar * pYglprg_vdp2_drawfb_addcolor_f[] = { Yglprg_vdp2_drawfb_addcolor_f, NULL };

/*------------------------------------------------------------------------------------
*  VDP2 Draw Frame buffer Operation( with add color operation )
* ----------------------------------------------------------------------------------*/
static int idvdp1FrameBuffer_addcolor;
static int idfrom_addcolor;
static int idto_addcolor;
static int idcoloroffset_addcolor;

int Ygl_uniformVDP2DrawFramebuffer_addcolor(void * p, float from, float to, float * offsetcol)
{
	YglProgram * prg;
	prg = p;

	glUseProgram(_prgid[PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR]);
	glUniform1i(idvdp1FrameBuffer_addcolor, 0);
	glActiveTexture(GL_TEXTURE0);
	glUniform1f(idfrom_addcolor, from);
	glUniform1f(idto_addcolor, to);
	glUniform4fv(idcoloroffset_addcolor, 1, offsetcol);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	_Ygl->renderfb.mtxModelView = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR], (const GLchar *)"u_mvpMatrix");

	glBlendFunc(GL_ONE, GL_SRC_ALPHA);
	return 0;
}

int Ygl_cleanupVDP2DrawFramebuffer_addcolor(void * p){
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return 0;
}

/*------------------------------------------------------------------------------------
 *  VDP2 Add Blend operaiotn
 * ----------------------------------------------------------------------------------*/
int Ygl_uniformAddBlend(void * p )
{
   glBlendFunc(GL_ONE,GL_ONE);
   return 0;
}

int Ygl_cleanupAddBlend(void * p )
{
   glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
   return 0;
}


/*------------------------------------------------------------------------------------
*  11.3 Line Color Insertion
* ----------------------------------------------------------------------------------*/
const GLchar * pYglprg_linecol_v[] = { Yglprg_normal_v, NULL };

const GLchar Yglprg_linecol_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                            \n"
"in highp vec4 v_texcoord;                            \n"
"uniform vec4 u_color_offset;    \n"
"uniform float u_emu_height;    \n"
"uniform float u_vheight; \n"
"uniform sampler2D s_texture;                        \n"
"uniform sampler2D s_line;                        \n"
"out vec4 fragColor;            \n"
"void main()                                         \n"
"{                                                   \n"
"  ivec2 addr; \n"
"  addr.x = int(v_texcoord.x);                        \n"
"  addr.y = int(v_texcoord.y);                        \n"
"  ivec2 linepos; \n "
"  linepos.y = 0; \n "
"  linepos.x = int( (u_vheight-gl_FragCoord.y) * u_emu_height);\n"
"  vec4 txcol = texelFetch( s_texture, addr,0 );      \n"
"  vec4 lncol = texelFetch( s_line, linepos,0 );      \n"
"  if(txcol.a > 0.0){\n                                 "
"     fragColor = txcol+u_color_offset+lncol;\n       "
"     fragColor.a = 1.0;\n                             "
"  }else{ \n                                            "
"     discard;\n                                      "
"  }                                                   \n"
"}                                                   \n";
const GLchar * pYglprg_linecol_f[] = { Yglprg_linecol_f, NULL };
static int id_linecol_s_texture = -1;
static int id_linecol_s_line = -1;
static int id_linecol_color_offset = -1;
static int id_linecol_emu_height = -1;
static int id_linecol_vheight = -1;

int Ygl_uniformLinecolorInsert(void * p)
{

  YglProgram * prg;
  prg = p;
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glUniform1i(id_linecol_s_texture, 0);
  glUniform1i(id_linecol_s_line, 1);
  glUniform4fv(id_linecol_color_offset, 1, prg->color_offset_val);
  glUniform1f(id_linecol_emu_height, (float)_Ygl->rheight / (float)_Ygl->height);
  glUniform1f(id_linecol_vheight, (float)_Ygl->height);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _Ygl->lincolor_tex);

  glActiveTexture(GL_TEXTURE0);

  glDisable(GL_BLEND);

  return 0;
}

int Ygl_cleanupLinecolorInsert(void * p)
{
  YglProgram * prg;
  prg = p;
  
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_BLEND);

  return 0;
}


int YglGetProgramId( int prg )
{
   return _prgid[prg];
}

int YglInitShader(int id, const GLchar * vertex[], const GLchar * frag[], const GLchar * tc[], const GLchar * te[], const GLchar * g[] )
{
    GLint compiled,linked;
    GLuint vshader;
    GLuint fshader;
	GLuint tcsHandle = 0;
	GLuint tesHandle = 0;
	GLuint gsHandle = 0;

   _prgid[id] = glCreateProgram();
    if (_prgid[id] == 0 ) return -1;

    vshader = glCreateShader(GL_VERTEX_SHADER);
	fshader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vshader, 1, vertex, NULL);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
       YGLLOG( "Compile error in vertex shader.\n");
       Ygl_printShaderError(vshader);
       _prgid[id] = 0;
       return -1;
    }

    glShaderSource(fshader, 1,frag, NULL);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
       YGLLOG( "Compile error in fragment shader.\n");
       Ygl_printShaderError(fshader);
       _prgid[id] = 0;
       return -1;
     }

    glAttachShader(_prgid[id], vshader);
    glAttachShader(_prgid[id], fshader);

	if (tc != NULL){
		tcsHandle = glCreateShader(GL_TESS_CONTROL_SHADER);
		if (tcsHandle == 0){
			YGLLOG("GL_TESS_CONTROL_SHADER is not supported\n");
		}
		glShaderSource(tcsHandle, 1, tc, NULL);
		glCompileShader(tcsHandle);
		glGetShaderiv(tcsHandle, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in GL_TESS_CONTROL_SHADER shader.\n");
			Ygl_printShaderError(tcsHandle);
			_prgid[id] = 0;
			return -1;
		}
		glAttachShader(_prgid[id], tcsHandle);
	}
	if (te != NULL){
		tesHandle = glCreateShader(GL_TESS_EVALUATION_SHADER);
		if (tesHandle == 0){
			YGLLOG("GL_TESS_EVALUATION_SHADER is not supported\n");
		}
		glShaderSource(tesHandle, 1, te, NULL);
		glCompileShader(tesHandle);
		glGetShaderiv(tesHandle, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in GL_TESS_EVALUATION_SHADER shader.\n");
			Ygl_printShaderError(tesHandle);
			_prgid[id] = 0;
			return -1;
		}
		glAttachShader(_prgid[id], tesHandle);
	}
	if (g != NULL){
		gsHandle = glCreateShader(GL_GEOMETRY_SHADER);
		if (gsHandle == 0){
			YGLLOG("GL_GEOMETRY_SHADER is not supported\n");
		}
		glShaderSource(gsHandle, 1, g, NULL);
		glCompileShader(gsHandle);
		glGetShaderiv(gsHandle, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in GL_TESS_EVALUATION_SHADER shader.\n");
			Ygl_printShaderError(gsHandle);
			_prgid[id] = 0;
			return -1;
		}
		glAttachShader(_prgid[id], gsHandle);
	}

    glLinkProgram(_prgid[id]);
    glGetProgramiv(_prgid[id], GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
       YGLLOG("Link error..\n");
       Ygl_printShaderError(_prgid[id]);
       _prgid[id] = 0;
       return -1;
    }
    return 0;
}



int YglProgramInit()
{
   YGLLOG("PG_NORMAL\n");
   //
   if( YglInitShader( PG_NORMAL, pYglprg_normal_v, pYglprg_normal_f,NULL,NULL,NULL ) != 0 )
      return -1;

    id_normal_s_texture = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"s_texture");

#if 0
	YGLLOG("PG_VDP2_MOSAIC\n");
	if (YglInitShader(PG_VDP2_MOSAIC, pYglprg_vdp1_normal_v, pYglprg_mosaic_f, NULL, NULL, NULL) != 0)
		return -1;
	id_mosaic_s_texture = glGetUniformLocation(_prgid[PG_VDP2_MOSAIC], (const GLchar *)"s_texture");
	id_mosaic = glGetUniformLocation(_prgid[PG_VDP2_MOSAIC], (const GLchar *)"u_mosaic");
	id_mosaic_color_offset = glGetUniformLocation(_prgid[PG_VDP2_MOSAIC], (const GLchar *)"u_color_offset");
#endif

   _prgid[PG_VFP1_ENDUSERCLIP] = _prgid[PG_NORMAL];
   _prgid[PG_VDP2_ADDBLEND] = _prgid[PG_NORMAL];
   _prgid[PG_VDP2_MOSAIC] = _prgid[PG_NORMAL];

   YGLLOG("PG_VDP1_NORMAL\n");
   //
   if (YglInitShader(PG_VDP1_NORMAL, pYglprg_vdp1_normal_v, pYglprg_vdp1_normal_f, NULL, NULL, NULL) != 0)
      return -1;

   id_vdp1_normal_s_texture = glGetUniformLocation(_prgid[PG_VDP1_NORMAL], (const GLchar *)"s_texture");


   // extentions
   _prgid[PG_VDP2_BLUR] = _prgid[PG_NORMAL];

   //-----------------------------------------------------------------------------------------------------------
   YGLLOG("PG_VFP1_GOURAUDSAHDING\n");

   if (YglInitShader(PG_VFP1_GOURAUDSAHDING, pYglprg_vdp1_gouraudshading_v, pYglprg_vdp1_gouraudshading_f, NULL, NULL, NULL) != 0)
      return -1;

   Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_GOURAUDSAHDING], &id_g);


   YGLLOG("PG_VDP2_DRAWFRAMEBUFF --START--\n");

   //
   if (YglInitShader(PG_VDP2_DRAWFRAMEBUFF, pYglprg_vdp2_drawfb_v, pYglprg_vdp2_drawfb_f, NULL, NULL, NULL) != 0)
      return -1;

   YGLLOG("PG_VDP2_DRAWFRAMEBUFF --END--\n");

   idvdp1FrameBuffer = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF], (const GLchar *)"s_vdp1FrameBuffer");
   idfrom = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF], (const GLchar *)"u_from");
   idto   = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF], (const GLchar *)"u_to");
   idcoloroffset = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF], (const GLchar *)"u_coloroffset");

   _Ygl->renderfb.prgid=_prgid[PG_VDP2_DRAWFRAMEBUFF];
   _Ygl->renderfb.setupUniform    = Ygl_uniformNormal;
   _Ygl->renderfb.cleanupUniform  = Ygl_cleanupNormal;
   _Ygl->renderfb.vertexp         = glGetAttribLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF],(const GLchar *)"a_position");
   _Ygl->renderfb.texcoordp       = glGetAttribLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF],(const GLchar *)"a_texcoord");
   _Ygl->renderfb.mtxModelView    = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF],(const GLchar *)"u_mvpMatrix");

   //-----------------------------------------------------------------------------------------------------------
   YGLLOG("PG_VFP1_HALFTRANS\n");

   //
   if (YglInitShader(PG_VFP1_HALFTRANS, pYglprg_vdp1_halftrans_v, pYglprg_vdp1_halftrans_f, NULL, NULL, NULL) != 0)
      return -1;

   Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_HALFTRANS], &hf);


   //-----------------------------------------------------------------------------------------------------------
   YGLLOG("PG_VFP1_SHADOW\n");

   //
   if (YglInitShader(PG_VFP1_SHADOW, pYglprg_vdp1_shadow_v, pYglprg_vdp1_shadow_f, NULL, NULL, NULL) != 0)
	   return -1;

   shadow.sprite = glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_sprite");
   shadow.fbo = glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_fbo");
   shadow.fbowidth = glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_fbowidth");
   shadow.fboheight = glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_fbohegiht");


   //-----------------------------------------------------------------------------------------------------------
   YGLLOG("PG_VFP1_GOURAUDSAHDING_HALFTRANS\n");

   if (YglInitShader(PG_VFP1_GOURAUDSAHDING_HALFTRANS, pYglprg_vdp1_gouraudshading_hf_v, pYglprg_vdp1_gouraudshading_hf_f, NULL, NULL, NULL) != 0)
      return -1;

   Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_GOURAUDSAHDING_HALFTRANS], &id_ght);


   //-----------------------------------------------------------------------------------------------------------
   YGLLOG("PG_VFP1_MESH\n");

   if (YglInitShader(PG_VFP1_MESH, pYglprg_vdp1_mesh_v, pYglprg_vdp1_mesh_f, NULL, NULL, NULL) != 0)
	   return -1;

   Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_MESH], &mesh);



   YGLLOG("PG_WINDOW\n");
   //
   if (YglInitShader(PG_WINDOW, pYglprg_window_v, pYglprg_window_f, NULL, NULL, NULL) != 0)
      return -1;

   _Ygl->windowpg.prgid=_prgid[PG_WINDOW];
   _Ygl->windowpg.setupUniform    = Ygl_uniformNormal;
   _Ygl->windowpg.cleanupUniform  = Ygl_cleanupNormal;
   _Ygl->windowpg.vertexp         = glGetAttribLocation(_prgid[PG_WINDOW],(const GLchar *)"a_position");
   _Ygl->windowpg.mtxModelView    = glGetUniformLocation(_prgid[PG_WINDOW],(const GLchar *)"u_mvpMatrix");

   _prgid[PG_VFP1_STARTUSERCLIP] = _prgid[PG_WINDOW];

   YGLLOG("PG_LINECOLOR_INSERT\n");
   //
   if (YglInitShader(PG_LINECOLOR_INSERT, pYglprg_linecol_v, pYglprg_linecol_f, NULL, NULL, NULL) != 0)
     return -1;

   id_linecol_s_texture = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"s_texture");
   id_linecol_s_line = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"s_line");
   id_linecol_color_offset = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"u_color_offset");
   id_linecol_emu_height =   glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"u_emu_height");
   id_linecol_vheight = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"u_vheight");

   //
   if (YglInitShader(PG_VDP2_DRAWFRAMEBUFF_LINECOLOR, pYglprg_vdp2_drawfb_linecolor_v, pYglprg_vdp2_drawfb_linecolor_f, NULL, NULL, NULL) != 0)
     return -1;

   idvdp1FrameBuffer_linecolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"s_vdp1FrameBuffer");;
   idfrom_linecolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"u_from");
   idto_linecolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"u_to");
   idcoloroffset_linecolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"u_coloroffset");
   id_fblinecol_s_line = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"s_line");
   id_fblinecol_emu_height = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"u_emu_height");
   id_fblinecol_vheight = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR], (const GLchar *)"u_vheight");

   //
   if (YglInitShader(PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR, pYglprg_vdp2_drawfb_v, pYglprg_vdp2_drawfb_addcolor_f, NULL, NULL, NULL) != 0)
	   return -1;

   idvdp1FrameBuffer_addcolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR], (const GLchar *)"s_vdp1FrameBuffer");;
   idfrom_addcolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR], (const GLchar *)"u_from");
   idto_addcolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR], (const GLchar *)"u_to");
   idcoloroffset_addcolor = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_ADDCOLOR], (const GLchar *)"u_coloroffset");

   //
   if (YglInitShader(PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA, pYglprg_vdp2_drawfb_linecolor_destination_alpha_v, pYglprg_vdp2_drawfb_linecolor_destination_alpha_f, NULL, NULL, NULL) != 0)
	   return -1;

   idvdp1FrameBuffer_linecolor_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"s_vdp1FrameBuffer");;
   idfrom_linecolor_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"u_from");
   idto_linecolor_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"u_to");
   idcoloroffset_linecolor_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"u_coloroffset");
   id_fblinecol_s_line_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"s_line");
   id_fblinecol_emu_height_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"u_emu_height");
   id_fblinecol_vheight_destination_alpha = glGetUniformLocation(_prgid[PG_VDP2_DRAWFRAMEBUFF_LINECOLOR_DESTINATION_ALPHA], (const GLchar *)"u_vheight");


   return 0;
}

int YglTesserationProgramInit()
{
	//-----------------------------------------------------------------------------------------------------------
		YGLLOG("PG_VFP1_GOURAUDSAHDING_TESS");
		if (YglInitShader(PG_VFP1_GOURAUDSAHDING_TESS,
			pYglprg_vdp1_gouraudshading_tess_v,
			pYglprg_vdp1_gouraudshading_f,
			pYglprg_vdp1_gouraudshading_tess_c,
			pYglprg_vdp1_gouraudshading_tess_e,
			pYglprg_vdp1_gouraudshading_tess_g) != 0)
			return -1;

		id_gt.sprite = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING_TESS], (const GLchar *)"u_sprite");
		id_gt.tessLevelInner = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING_TESS], (const GLchar *)"TessLevelInner");
		id_gt.tessLevelOuter = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING_TESS], (const GLchar *)"TessLevelOuter");

		//---------------------------------------------------------------------------------------------------------
		YGLLOG("PG_VFP1_MESH_TESS");
		if (YglInitShader(PG_VFP1_MESH_TESS,
			pYglprg_vdp1_gouraudshading_tess_v,
			pYglprg_vdp1_mesh_f,
			pYglprg_vdp1_gouraudshading_tess_c,
			pYglprg_vdp1_gouraudshading_tess_e,
			pYglprg_vdp1_gouraudshading_tess_g) != 0)
			return -1;

		Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_MESH_TESS], &mesh_tess);

		//---------------------------------------------------------------------------------------------------------
		YGLLOG("PG_VFP1_GOURAUDSAHDING_HALFTRANS_TESS");
		if (YglInitShader(PG_VFP1_GOURAUDSAHDING_HALFTRANS_TESS,
			pYglprg_vdp1_gouraudshading_tess_v,
			pYglprg_vdp1_gouraudshading_hf_f,
			pYglprg_vdp1_gouraudshading_tess_c,
			pYglprg_vdp1_gouraudshading_tess_e,
			pYglprg_vdp1_gouraudshading_tess_g) != 0)
			return -1;

		Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_GOURAUDSAHDING_HALFTRANS_TESS], &id_ght_tess);

		//---------------------------------------------------------------------------------------------------------
		YGLLOG("PG_VFP1_SHADOW_TESS");
		if (YglInitShader(PG_VFP1_SHADOW_TESS,
			pYglprg_vdp1_gouraudshading_tess_v,
			pYglprg_vdp1_shadow_f,
			pYglprg_vdp1_gouraudshading_tess_c,
			pYglprg_vdp1_gouraudshading_tess_e,
			pYglprg_vdp1_gouraudshading_tess_g) != 0)
			return -1;

		shadow_tess.sprite = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"u_sprite");
		shadow_tess.tessLevelInner = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"TessLevelInner");
		shadow_tess.tessLevelOuter = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"TessLevelOuter");
		shadow_tess.fbo = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"u_fbo");
		shadow_tess.fbowidth = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"u_fbowidth");
		shadow_tess.fboheight = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"u_fbohegiht");

		//---------------------------------------------------------------------------------------------------------
		YGLLOG("PG_VFP1_HALFTRANS_TESS");
		if (YglInitShader(PG_VFP1_HALFTRANS_TESS,
			pYglprg_vdp1_gouraudshading_tess_v,
			pYglprg_vdp1_halftrans_f,
			pYglprg_vdp1_gouraudshading_tess_c,
			pYglprg_vdp1_gouraudshading_tess_e,
			pYglprg_vdp1_gouraudshading_tess_g) != 0)
			return -1;

	Ygl_Vdp1CommonGetUniformId(_prgid[PG_VFP1_HALFTRANS_TESS], &hf_tess);

	return 0;
}



int YglProgramChange( YglLevel * level, int prgid )
{
   YglProgram* tmp;
   YglProgram* current;
#if  USEVBO
   int maxsize;
#endif

   level->prgcurrent++;

   if( level->prgcurrent >= level->prgcount)
   {
      level->prgcount++;
      tmp = (YglProgram*)malloc(sizeof(YglProgram)*level->prgcount);
      if( tmp == NULL ) return -1;
      memset(tmp,0,sizeof(YglProgram)*level->prgcount);
      memcpy(tmp,level->prg,sizeof(YglProgram)*(level->prgcount-1));
      free(level->prg);
      level->prg = tmp;

      level->prg[level->prgcurrent].currentQuad = 0;
#if  USEVBO
       level->prg[level->prgcurrent].maxQuad = 14692;
      maxsize = level->prg[level->prgcurrent].maxQuad;
      if( YglGetVertexBuffer(maxsize,
                             (void**)&level->prg[level->prgcurrent].quads,
                             (void**)&level->prg[level->prgcurrent].textcoords,
                             (void**)&level->prg[level->prgcurrent].vertexAttribute  ) != 0 ) {
          return -1;
      }
      if( level->prg[level->prgcurrent].quads == 0 )
      {
          int a=0;
      }
#else
      level->prg[level->prgcurrent].maxQuad = 12*64;
      if ((level->prg[level->prgcurrent].quads = (float *) malloc(level->prg[level->prgcurrent].maxQuad * sizeof(float))) == NULL)
         return -1;

      if ((level->prg[level->prgcurrent].textcoords = (float *) malloc(level->prg[level->prgcurrent].maxQuad * sizeof(float) * 2)) == NULL)
         return -1;

       if ((level->prg[level->prgcurrent].vertexAttribute = (float *) malloc(level->prg[level->prgcurrent].maxQuad * sizeof(float)*2)) == NULL)
         return -1;
#endif
   }

   current = &level->prg[level->prgcurrent];
   level->prg[level->prgcurrent].prgid=prgid;
   level->prg[level->prgcurrent].prg=_prgid[prgid];
   level->prg[level->prgcurrent].vaid = 0;

   if (prgid == PG_NORMAL)
   {
	   current->setupUniform = Ygl_uniformNormal;
	   current->cleanupUniform = Ygl_cleanupNormal;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"u_mvpMatrix");
	   //current->mtxTexture      = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_texMatrix");
	   current->color_offset = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"u_color_offset");
	   //current->tex0 = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"s_texture");
   }
   else if (prgid == PG_VDP2_MOSAIC)
   {
	   current->setupUniform = Ygl_uniformMosaic;
	   current->cleanupUniform = Ygl_cleanupMosaic;
	  current->vertexp = 0;
	  current->texcoordp = 1;
	  current->mtxModelView = glGetUniformLocation(_prgid[PG_VDP2_MOSAIC], (const GLchar *)"u_mvpMatrix");

   }else if( prgid == PG_VDP1_NORMAL )
   {
      current->setupUniform    = Ygl_uniformVdp1Normal;
      current->cleanupUniform  = Ygl_cleanupVdp1Normal;
      current->vertexp = 0;
      current->texcoordp = 1;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_VDP1_NORMAL],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = glGetUniformLocation(_prgid[PG_VDP1_NORMAL],(const GLchar *)"u_texMatrix");
      current->tex0 = glGetUniformLocation(_prgid[PG_VDP1_NORMAL], (const GLchar *)"s_texture");

   }else if( prgid == PG_VFP1_GOURAUDSAHDING )
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &id_g;
	   current->vertexp = 0;
	   current->texcoordp = 1; 
      level->prg[level->prgcurrent].vaid = 2;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING],(const GLchar *)"u_texMatrix");
      current->tex0 = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING], (const GLchar *)"s_texture");
   }
   else if (prgid == PG_VFP1_GOURAUDSAHDING_TESS ){
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &id_gt;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   level->prg[level->prgcurrent].vaid = 2;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING_TESS], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1; // glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING], (const GLchar *)"u_texMatrix");
	   current->tex0 = -1; // glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING], (const GLchar *)"s_texture");
   }
   else if( prgid == PG_VFP1_STARTUSERCLIP )
   {
      level->prg[level->prgcurrent].setupUniform = Ygl_uniformStartUserClip;
      level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupStartUserClip;
      current->vertexp         = 0;
      current->texcoordp       = -1;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_WINDOW],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = -1; //glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_texMatrix");
      
   }
   else if( prgid == PG_VFP1_ENDUSERCLIP )
   {
      level->prg[level->prgcurrent].setupUniform = Ygl_uniformEndUserClip;
      level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupEndUserClip;
      current->vertexp = 0;
      current->texcoordp = 1;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_texMatrix");
      current->tex0 = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"s_texture");
   }
   else if( prgid == PG_VFP1_HALFTRANS )
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	  level->prg[level->prgcurrent].ids = &hf;
      current->vertexp = 0;
      current->texcoordp = 1;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_VFP1_HALFTRANS],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = glGetUniformLocation(_prgid[PG_VFP1_HALFTRANS],(const GLchar *)"u_texMatrix");

   }
   else if (prgid == PG_VFP1_HALFTRANS_TESS)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &hf_tess;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_HALFTRANS_TESS], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1;

   }
   else if (prgid == PG_VFP1_SHADOW)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &shadow;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_texMatrix");

   }
   else if (prgid == PG_VFP1_SHADOW_TESS)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &shadow_tess;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_SHADOW_TESS], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1; // glGetUniformLocation(_prgid[PG_VFP1_SHADOW], (const GLchar *)"u_texMatrix");

   }

   else if (prgid == PG_VFP1_MESH)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &mesh;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   level->prg[level->prgcurrent].vaid = 2;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_MESH], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1;
   }
   else if (prgid == PG_VFP1_MESH_TESS)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &mesh_tess;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   level->prg[level->prgcurrent].vaid = 2;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_MESH_TESS], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1;
   }
   else if (prgid == PG_VFP1_GOURAUDSAHDING_HALFTRANS)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &id_ght;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   level->prg[level->prgcurrent].vaid = 2;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING_HALFTRANS], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1;

   }
   else if (prgid == PG_VFP1_GOURAUDSAHDING_HALFTRANS_TESS)
   {
	   level->prg[level->prgcurrent].setupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].cleanupUniform = Ygl_uniformVdp1CommonParam;
	   level->prg[level->prgcurrent].ids = &id_ght_tess;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   level->prg[level->prgcurrent].vaid = 2;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VFP1_GOURAUDSAHDING_HALFTRANS_TESS], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = -1;

   }else if( prgid == PG_VDP2_ADDBLEND )
   {
      level->prg[level->prgcurrent].setupUniform = Ygl_uniformAddBlend;
      level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupAddBlend;
      current->vertexp = 0;
      current->texcoordp = 1;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_texMatrix");
   }else if( prgid == PG_VDP2_STARTWINDOW )
   {
      level->prg[level->prgcurrent].setupUniform = Ygl_uniformStartVDP2Window;
      level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupStartVDP2Window;
      current->vertexp         = 0;
      current->texcoordp       = -1;
      current->mtxModelView    = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_mvpMatrix");
      current->mtxTexture      = glGetUniformLocation(_prgid[PG_NORMAL],(const GLchar *)"u_texMatrix");
   }
   else if (prgid == PG_VDP2_ENDWINDOW)
   {
     level->prg[level->prgcurrent].setupUniform = Ygl_uniformEndVDP2Window;
     level->prg[level->prgcurrent].cleanupUniform = Ygl_cleanupEndVDP2Window;
     current->vertexp = 0;
     current->texcoordp = 1;
     current->mtxModelView = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"u_mvpMatrix");
     current->mtxTexture = glGetUniformLocation(_prgid[PG_NORMAL], (const GLchar *)"u_texMatrix");
   }
   else if (prgid == PG_LINECOLOR_INSERT)
   {
	   current->setupUniform = Ygl_uniformLinecolorInsert;
	   current->cleanupUniform = Ygl_cleanupLinecolorInsert;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"u_texMatrix");
	   current->color_offset = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"u_color_offset");
	   current->tex0 = glGetUniformLocation(_prgid[PG_LINECOLOR_INSERT], (const GLchar *)"s_texture");
   }
   else if (prgid == PG_VDP2_BLUR)
   {
	   current->setupUniform = Ygl_uniformNormal_blur;
	   current->cleanupUniform = Ygl_cleanupNormal_blur;
	   current->vertexp = 0;
	   current->texcoordp = 1;
	   current->mtxModelView = glGetUniformLocation(_prgid[PG_VDP2_BLUR], (const GLchar *)"u_mvpMatrix");
	   current->mtxTexture = glGetUniformLocation(_prgid[PG_VDP2_BLUR], (const GLchar *)"u_texMatrix");
	   current->color_offset = glGetUniformLocation(_prgid[PG_VDP2_BLUR], (const GLchar *)"u_color_offset");
	   current->tex0 = glGetUniformLocation(_prgid[PG_VDP2_BLUR], (const GLchar *)"s_texture");
   }else{
      level->prg[level->prgcurrent].setupUniform = NULL;
      level->prg[level->prgcurrent].cleanupUniform = NULL;
   }
   return 0;

}

//----------------------------------------------------------------------------------------
static int blit_prg = -1;
static int u_w;
static int u_h;

static const char vblit_img[] =
#if defined (_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
"layout (location = 0) in vec2 a_Position; \n"
"layout (location = 1) in vec2 a_Uv;       \n"
"uniform float u_w; \n"
"uniform float u_h; \n"
"out vec2	v_Uv;                    \n"  
"void main()                              \n"
"{                                        \n"
"   gl_Position = vec4((a_Position.x*u_w)-1.0, (a_Position.y*u_h)-1.0, 0.0, 1.0);  \n" 
"   v_Uv = a_Uv;                          \n"
"}";

static const char fblit_img[] =
#if defined (_OGLES3_)
      "#version 300 es \n"
#else
      "#version 330 \n"
#endif
"precision mediump float;     \n"
"uniform sampler2D u_Src; \n"
"in vec2  v_Uv;          \n"
"out vec4 fragColor;            \n"
"void main () \n"
"{ \n"
"  vec4 src = texture2D( u_Src, v_Uv ); \n"
"  fragColor = src; \n"
"}\n";



int YglBlitFramebuffer(u32 srcTexture, u32 targetFbo, float w, float h) {

  float vb[] = { 0, 0, 
    2.0, 0.0, 
    2.0, 2.0, 
    0, 2.0, };

  float tb[] = { 0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    0.0, 1.0 };

  glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);

  if (blit_prg == -1){
    GLuint vshader;
    GLuint fshader;
    GLint compiled, linked;

    const GLchar * vblit_img_v[] = { vblit_img, NULL };
    const GLchar * fblit_img_v[] = { fblit_img, NULL };

    blit_prg = glCreateProgram();
    if (blit_prg == 0) return -1;

    glUseProgram(blit_prg);
    vshader = glCreateShader(GL_VERTEX_SHADER);
    fshader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vshader, 1, vblit_img_v, NULL);
    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
      YGLLOG("Compile error in vertex shader.\n");
      Ygl_printShaderError(vshader);
      blit_prg = -1;
      return -1;
    }

    glShaderSource(fshader, 1, fblit_img_v, NULL);
    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
      YGLLOG("Compile error in fragment shader.\n");
      Ygl_printShaderError(fshader);
      blit_prg = -1;
      return -1;
    }

    glAttachShader(blit_prg, vshader);
    glAttachShader(blit_prg, fshader);
    glLinkProgram(blit_prg);
    glGetProgramiv(blit_prg, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
      YGLLOG("Link error..\n");
      Ygl_printShaderError(blit_prg);
      blit_prg = -1;
      return -1;
    }

     glUniform1i(glGetUniformLocation(blit_prg, "u_Src"), 0);
     u_w = glGetUniformLocation(blit_prg, "u_w");
     u_h = glGetUniformLocation(blit_prg, "u_h");

  }
  else{
    glUseProgram(blit_prg);
  }


  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vb);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tb);
  glUniform1f(u_w, w);
  glUniform1f(u_h, h);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, srcTexture);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  // Clean up
  glActiveTexture(GL_TEXTURE0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  return 0;
}



//----------------------------------------------------------------------------------------
static int fxaa_prg = -1;
static int u_frame = 0;
static int a_PosCoord = 0;
static int a_TexCoord = 0;

int YglBlitFXAA(u32 sourceTexture, float w, float h) {

	float aspectRatio = 1.0;
	float vb[] = { 0, 0,
		2.0, 0.0,
		2.0, 2.0,
		0, 2.0, };

	float tb[] = { 0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0 };

	if (fxaa_prg == -1){
		GLuint vshader;
		GLuint fshader;
		GLint compiled, linked;

		const GLchar * fxaa_v[] = { Yglprg_fxaa_v, NULL };
		const GLchar * fxaa_f[] = { Yglprg_fxaa_f, NULL };

		fxaa_prg = glCreateProgram();
		if (fxaa_prg == 0) return -1;

		glUseProgram(fxaa_prg);
		vshader = glCreateShader(GL_VERTEX_SHADER);
		fshader = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(vshader, 1, fxaa_v, NULL);
		glCompileShader(vshader);
		glGetShaderiv(vshader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			printf("Compile error in vertex shader.\n");
			Ygl_printShaderError(vshader);
			fxaa_prg = -1;
			return -1;
		}

		glShaderSource(fshader, 1, fxaa_f, NULL);
		glCompileShader(fshader);
		glGetShaderiv(fshader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			printf("Compile error in fragment shader.\n");
			Ygl_printShaderError(fshader);
			fxaa_prg = -1;
			return -1;
		}

		glAttachShader(fxaa_prg, vshader);
		glAttachShader(fxaa_prg, fshader);
		glLinkProgram(fxaa_prg);
		glGetProgramiv(fxaa_prg, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			printf("Link error..\n");
			Ygl_printShaderError(fxaa_prg);
			fxaa_prg = -1;
			return -1;
		}

		glUniform1i(glGetUniformLocation(fxaa_prg, "uSourceTex"), 0);
		u_frame = glGetUniformLocation(fxaa_prg, "RCPFrame");
		a_PosCoord = glGetAttribLocation(fxaa_prg,"aPosition");
		a_TexCoord = glGetAttribLocation(fxaa_prg, "aTexCoord");

	}
	else{
		glUseProgram(fxaa_prg);
	}


	float const vertexPosition[] = {
		aspectRatio, -1.0f,
		-aspectRatio, -1.0f,
		aspectRatio, 1.0f,
		-aspectRatio, 1.0f };

	float const textureCoord[] = {
		1.0f, 0.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f };

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sourceTexture);

	glUniform1i(glGetUniformLocation(fxaa_prg, "uSourceTex"), 0);
	glUniform2f(u_frame, (float)(1.0 / (float)(w)), (float)(1.0 / (float)(h)));


	glVertexAttribPointer(a_PosCoord, 2, GL_FLOAT, GL_FALSE, 0, vertexPosition);
	glVertexAttribPointer(a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, textureCoord);
	glEnableVertexAttribArray(a_PosCoord);
	glEnableVertexAttribArray(a_TexCoord);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);


	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(a_PosCoord);
	glDisableVertexAttribArray(a_TexCoord);


	// Clean up
	glActiveTexture(GL_TEXTURE0);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	return 0;
}


/*
hard/vdp2/hon/p12_13.htm
*/

const GLchar blur_blit_v[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"uniform mat4 u_mvpMatrix;    \n"
"layout (location = 0) in vec4 a_position;   \n"
"layout (location = 1) in vec2 a_texcoord;   \n"
"out  highp vec2 v_texcoord;     \n"
"void main()                  \n"
"{                            \n"
"   gl_Position = a_position*u_mvpMatrix; \n"
"   v_texcoord  = a_texcoord; \n"
"} ";

const GLchar blur_blit_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                            \n"
"in highp vec2 v_texcoord;                            \n"
"uniform sampler2D u_Src;                        \n"
"uniform float u_tw; \n"
"uniform float u_th; \n"
"out vec4 fragColor;            \n"
"void main()                                         \n"
"{                                                   \n"
"  ivec2 addr; \n"
"  addr.x = int(u_tw * v_texcoord.x);          \n"
"  addr.y = int(u_th) - int(u_th * v_texcoord.y);          \n"
"  vec4 txcol = texelFetch( u_Src, addr,0 ) ;      \n"
"  txcol.r = txcol.r / 4.0 * 2.0;"
"  txcol.g = txcol.g / 4.0 * 2.0;"
"  txcol.b = txcol.b / 4.0 * 2.0;"
"  addr.x -= 1; \n"
"  vec4 txcoll = texelFetch( u_Src, addr,0 );      \n"
"  txcoll.r = txcoll.r / 4.0;"
"  txcoll.g = txcoll.g / 4.0;"
"  txcoll.b = txcoll.b / 4.0;"
"  addr.x -= 1; \n"
"  vec4 txcolll = texelFetch( u_Src, addr,0 );      \n"
"  txcolll.r = txcolll.r / 4.0;"
"  txcolll.g = txcolll.g / 4.0;"
"  txcolll.b = txcolll.b / 4.0;"
"  txcol.r = txcol.r + txcoll.r + txcolll.r; "
"  txcol.g = txcol.g + txcoll.g + txcolll.g; "
"  txcol.b = txcol.b + txcoll.b + txcolll.b;  "
"  if(txcol.a > 0.0)\n                                 "
"     fragColor = txcol; \n                        "
"  else \n                                            "
"     discard;\n                                      "
"}                                                   \n";

static int blur_prg = -1;
static int u_blur_mtxModelView = -1;
static int u_blur_tw = -1;
static int u_blur_th = -1;

int YglBlitBlur(u32 srcTexture, u32 targetFbo, float w, float h, float * matrix) {

	float vb[] = { 0, 0,
		2.0, 0.0,
		2.0, 2.0,
		0, 2.0, };

	float tb[] = { 0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0 };

	vb[2] = w;
	vb[4] = w;
	vb[5] = h;
	vb[7] = h;

	glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);

	if (blur_prg == -1){
		GLuint vshader;
		GLuint fshader;
		GLint compiled, linked;

		const GLchar * vblit_img_v[] = { blur_blit_v, NULL };
		const GLchar * fblit_img_v[] = { blur_blit_f, NULL };

		blur_prg = glCreateProgram();
		if (blur_prg == 0) return -1;

		glUseProgram(blur_prg);
		vshader = glCreateShader(GL_VERTEX_SHADER);
		fshader = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(vshader, 1, vblit_img_v, NULL);
		glCompileShader(vshader);
		glGetShaderiv(vshader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in vertex shader.\n");
			Ygl_printShaderError(vshader);
			blur_prg = -1;
			return -1;
		}

		glShaderSource(fshader, 1, fblit_img_v, NULL);
		glCompileShader(fshader);
		glGetShaderiv(fshader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in fragment shader.\n");
			Ygl_printShaderError(fshader);
			blur_prg = -1;
			return -1;
		}

		glAttachShader(blur_prg, vshader);
		glAttachShader(blur_prg, fshader);
		glLinkProgram(blur_prg);
		glGetProgramiv(blur_prg, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			YGLLOG("Link error..\n");
			Ygl_printShaderError(blur_prg);
			blur_prg = -1;
			return -1;
		}

		glUniform1i(glGetUniformLocation(blur_prg, "u_Src"), 0);
		u_blur_mtxModelView = glGetUniformLocation(blur_prg, (const GLchar *)"u_mvpMatrix");
		u_blur_tw = glGetUniformLocation(blur_prg, "u_tw");
		u_blur_th = glGetUniformLocation(blur_prg, "u_th");

	}
	else{
		glUseProgram(blur_prg);
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vb);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tb);
	glUniformMatrix4fv(u_blur_mtxModelView, 1, GL_FALSE, matrix);
	glUniform1f(u_blur_tw, w);
	glUniform1f(u_blur_th, h);


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcTexture);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Clean up
	glActiveTexture(GL_TEXTURE0);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	return 0;
}


/*
hard/vdp2/hon/p12_13.htm
*/

const GLchar mosaic_blit_v[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"uniform mat4 u_mvpMatrix;    \n"
"layout (location = 0) in vec4 a_position;   \n"
"layout (location = 1) in vec2 a_texcoord;   \n"
"out  highp vec2 v_texcoord;     \n"
"void main()                  \n"
"{                            \n"
"   gl_Position = a_position*u_mvpMatrix; \n"
"   v_texcoord  = a_texcoord; \n"
"} ";

const GLchar mosaic_blit_f[] =
#if defined(_OGLES3_)
"#version 300 es \n"
#else
"#version 330 \n"
#endif
"precision highp float;                            \n"
"in highp vec2 v_texcoord;                            \n"
"uniform sampler2D u_Src;                        \n"
"uniform float u_tw; \n"
"uniform float u_th; \n"
"uniform ivec2 u_mosaic;			 \n"
"out vec4 fragColor;            \n"
"void main()                                         \n"
"{                                                   \n"
"  ivec2 addr; \n"
"  addr.x = int(u_tw * v_texcoord.x);          \n"
"  addr.y = int(u_th) - int(u_th * v_texcoord.y);          \n"
"  addr.x = addr.x / u_mosaic.x * u_mosaic.x;               \n"
"  addr.y = addr.y / u_mosaic.y * u_mosaic.y;               \n"
"  vec4 txcol = texelFetch( u_Src, addr,0 ) ;      \n"
"  if(txcol.a > 0.0)\n                                 "
"     fragColor = txcol; \n                        "
"  else \n                                            "
"     discard;\n                                      "
"}                                                   \n";

static int mosaic_prg = -1;
static int u_mosaic_mtxModelView = -1;
static int u_mosaic_tw = -1;
static int u_mosaic_th = -1;
static int u_mosaic = -1;

int YglBlitMosaic(u32 srcTexture, u32 targetFbo, float w, float h, float * matrix, int * mosaic) {

	float vb[] = { 0, 0,
		2.0, 0.0,
		2.0, 2.0,
		0, 2.0, };

	float tb[] = { 0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0 };

	vb[2] = w;
	vb[4] = w;
	vb[5] = h;
	vb[7] = h;

	glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);

	if (mosaic_prg == -1){
		GLuint vshader;
		GLuint fshader;
		GLint compiled, linked;

		const GLchar * vblit_img_v[] = { mosaic_blit_v, NULL };
		const GLchar * fblit_img_v[] = { mosaic_blit_f, NULL };

		mosaic_prg = glCreateProgram();
		if (mosaic_prg == 0) return -1;

		glUseProgram(mosaic_prg);
		vshader = glCreateShader(GL_VERTEX_SHADER);
		fshader = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(vshader, 1, vblit_img_v, NULL);
		glCompileShader(vshader);
		glGetShaderiv(vshader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in vertex shader.\n");
			Ygl_printShaderError(vshader);
			mosaic_prg = -1;
			return -1;
		}

		glShaderSource(fshader, 1, fblit_img_v, NULL);
		glCompileShader(fshader);
		glGetShaderiv(fshader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE) {
			YGLLOG("Compile error in fragment shader.\n");
			Ygl_printShaderError(fshader);
			mosaic_prg = -1;
			return -1;
		}

		glAttachShader(mosaic_prg, vshader);
		glAttachShader(mosaic_prg, fshader);
		glLinkProgram(mosaic_prg);
		glGetProgramiv(mosaic_prg, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			YGLLOG("Link error..\n");
			Ygl_printShaderError(mosaic_prg);
			mosaic_prg = -1;
			return -1;
		}

		glUniform1i(glGetUniformLocation(mosaic_prg, "u_Src"), 0);
		u_mosaic_mtxModelView = glGetUniformLocation(mosaic_prg, (const GLchar *)"u_mvpMatrix");
		u_mosaic_tw = glGetUniformLocation(mosaic_prg, "u_tw");
		u_mosaic_th = glGetUniformLocation(mosaic_prg, "u_th");
		u_mosaic = glGetUniformLocation(mosaic_prg, "u_mosaic");

	}
	else{
		glUseProgram(mosaic_prg);
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vb);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tb);
	glUniformMatrix4fv(u_mosaic_mtxModelView, 1, GL_FALSE, matrix);
	glUniform1f(u_mosaic_tw, w);
	glUniform1f(u_mosaic_th, h);
	glUniform2iv(u_mosaic, 1, mosaic);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcTexture);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Clean up
	glActiveTexture(GL_TEXTURE0);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	return 0;
}
// The MIT License (MIT)
//
// Copyright (c) 2013 Dan Ginsburg, Budirijanto Purnomo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//
// Book:      OpenGL(R) ES 3.0 Programming Guide, 2nd Edition
// Authors:   Dan Ginsburg, Budirijanto Purnomo, Dave Shreiner, Aaftab Munshi
// ISBN-10:   0-321-93388-5
// ISBN-13:   978-0-321-93388-1
// Publisher: Addison-Wesley Professional
// URLs:      http://www.opengles-book.com
//            http://my.safaribooksonline.com/book/animation-and-3d/9780133440133
//
// Simple_VertexShader.c
//
//    This is a simple example that draws a rotating cube in perspective
//    using a vertex shader to transform the object
//
#include <stdlib.h>
#include <math.h>
#include "esUtil.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PI 3.1415926535897932384626433832795f

typedef struct
{
	// Handle to a program object
	GLuint programObject;

	// Uniform locations
	GLint  mvpLoc;
	GLint  mvLoc;

	// Vertex daata
	GLfloat  *vertices;
	GLuint   *indices;
	int       numIndices;

	// Rotation angle
	GLfloat   angle;

	// MVP matrix
	ESMatrix  mvpMatrix;
	ESMatrix  mvMatrix;

	GLint textureID;
	GLint textureIdGrass;
	GLint textureIdBricks;
	GLint samplerLoc;
	GLint samplerLocGrass;
	GLint samplerLocBricks;

	GLuint vboIDs[5];
	GLuint vaoID;

	// light source define
	GLuint lightProgramObject;
	GLint  lightMvpLoc;
	ESMatrix  lightMvpMatrix;
	GLuint lightVboIDs[2];
	GLuint lightVaoID;
	GLuint lightPosLoc;
	GLuint lightColorLoc;
	GLuint eyePosLoc;

	GLuint grassProgramObject;
} UserData;

GLint loadTexture(const char* name)
{
	unsigned int texture;
	glGenTextures(1, &texture);

	int width, height, nrChannels;
	unsigned char *data = stbi_load(name, &width, &height, &nrChannels, 0);
	GLint format = GL_RGB;
	if (data)
	{
		switch (nrChannels) {
		case 1:
			format = GL_RED;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			break;
		}

		esLogMessage("%s: nrChannels = %d\n", name, nrChannels);

		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (format == GL_RGBA) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (format == GL_RGBA) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		esLogMessage("Failed to load texture\n");
	}
	stbi_image_free(data);
	return texture;
}

///
// Initialize the shader and program object
//
int Init(ESContext *esContext)
{
	UserData *userData = esContext->userData;
	const char vShaderStr[] =
		"#version 300 es                                                                          \n"
		"uniform mat4 u_mvMatrix;               					          \n"
		"uniform mat4 u_mvpMatrix;                                                         \n"
		"layout(location = 0) in vec4 a_position; // 立方体各个定点的坐标       \n"
		"layout(location = 1) in vec4 a_color;   // 立方体各个面的颜色            \n"
		"layout(location = 2) in vec2 vTexCoord; // 各个点对应的纹理坐标      \n"
		"layout(location = 3) in vec3 vNormal;  // 法向量                            \n"
		"out vec2 v_texCoord;                                                                  \n"
		"out vec4 v_color;                                                                        \n"
		"out vec3 v_normal;                                                                     \n"
		"out vec3 fragPos;                                                                        \n"
		"void main()                                                                                 \n"
		"{                                                                                                \n"
		"	v_color = a_color;                 						          \n"
		"	fragPos = vec3(u_mvMatrix * a_position);                              \n"
		"	gl_Position = u_mvpMatrix * a_position;                                \n"
		"	v_texCoord = vTexCoord;                                                      \n"
		"	//v_normal = vec3(u_mvMatrix * vec4(vNormal, 0.0f)); // 矩形转动，法向量也要变化  \n"
		"	v_normal = mat3(transpose(inverse(u_mvMatrix))) * vNormal;      \n"
		"}                                           \n";

	const char fShaderStr[] =
		"#version 300 es												\n"
		"precision mediump float;										\n"
		"in vec4 v_color;												\n"
		"in vec2 v_texCoord;											\n"
		"in vec3 v_normal;								              			\n"
		"in vec3 fragPos;                                                                                       \n"
		"layout(location = 0) out vec4 outColor;					           	        \n"
		"uniform sampler2D s_texture;		// 贴图1-箱子					 \n"
		"uniform vec3 lightPos;       // 光源位置                                                        \n"
		"uniform vec3 lightColor;     // 光源颜色                                                       \n"
		"uniform vec3 eyePos;     //  眼睛位置                                                          \n"
		"void main()												        \n"
		"{														        \n"
		"    // ambient                                                                                            \n"
		"    float ambientStrength = 0.1;                                                                 \n"
		"    vec3 ambient = ambientStrength * lightColor;  // 计算光照下的环境颜色      \n"
		"    // diffuse                                                                                               \n"
		"    vec3 norm = normalize(v_normal);                                                         \n"
		"    vec3 lightDir = normalize(lightPos - fragPos);                                          \n"
		"    float diff = max(dot(norm, lightDir), 0.0);                                                \n"
		"    vec3 diffuse = diff * lightColor;             // 计算光的漫反射颜色                     \n"
		"    float specularStrength = 0.5;                                                                  \n"
		"    vec3 eyeDir = normalize(eyePos - fragPos);   // 计算眼睛看frag的方向          \n"
		"    vec3 reflectDir = reflect(-lightDir, norm);   // 计算光反射的方向                   \n"
		"    float spec = pow(max(dot(eyeDir, reflectDir), 0.0), 32.0);                         \n"
		"    vec3 specular = specularStrength * spec * lightColor;    // 计算反光度         \n"
		"    vec4 color1 = vec4(0.0f, 0.0f, 0.0f, 0.0f);                                                 \n"
		"    color1 = (texture(s_texture, v_texCoord) + v_color * 0.5);                       \n"
		"    outColor = color1 * vec4((ambient + diffuse + specular), 1.0f);                 \n"
		"}														      	   \n";

	const char fShaderStr_grass[] =
		"#version 300 es												\n"
		"precision mediump float;										\n"
		"in vec4 v_color;												\n"
		"in vec2 v_texCoord;											\n"
		"in vec3 v_normal;											       \n"
		"in vec3 fragPos;                                                                                       \n"
		"layout(location = 0) out vec4 outColor;						               \n"
		"uniform sampler2D s_texture_bricks;	// 贴图2-砖头					\n"
		"uniform sampler2D s_texture_grass;	// 贴图2-草					\n"
		"uniform vec3 lightPos;       // 光源位置                                                        \n"
		"uniform vec3 lightColor;     // 光源颜色                                                       \n"
		"uniform vec3 eyePos;     //  眼睛位置                                                          \n"
		"void main()												        \n"
		"{														        \n"
		"	// ambient                                                                                         \n"
		"	float ambientStrength = 0.1;                                                               \n"
		"	vec3 ambient = ambientStrength * lightColor;  // 计算光照下的环境颜色    \n"
		"	// diffuse                                                                                             \n"
		"	vec3 norm = normalize(v_normal);                                                      \n"
		"	vec3 lightDir = normalize(lightPos - fragPos);                                        \n"
		"	float diff = max(dot(norm, lightDir), 0.0);                                             \n"
		"	vec3 diffuse = diff * lightColor;             // 计算光的漫反射颜色                  \n"
		"	float specularStrength = 0.5;                                                               \n"
		"	vec3 eyeDir = normalize(eyePos - fragPos);   // 计算眼睛看frag的方向       \n"
		"	vec3 reflectDir = reflect(-lightDir, norm);   // 计算光反射的方向                \n"
		"	float spec = pow(max(dot(eyeDir, reflectDir), 0.0), 32.0);                      \n"
		" 	vec3 specular = specularStrength * spec * lightColor;    // 计算反光度      \n"
		"	vec4 color1 = vec4(0.0f, 0.0f, 0.0f, 0.0f);                                              \n"
		"	vec4 color2 = vec4(0.0f, 0.0f, 0.0f, 0.0f);                                              \n"
		"	color1 = texture(s_texture_bricks, v_texCoord);                                    \n"
		"	color2 = texture(s_texture_grass, v_texCoord);                                     \n"
		"	if(color2.a < 0.1){                                                                               \n"
		"		outColor = color1 * vec4((ambient + diffuse + specular), 1.0f);       \n"
		"	}else{                                                                                                 \n"
		"		outColor = color1 * color2 * vec4((ambient + diffuse + specular), 1.0f);      \n"
		"	}                                                                                                      \n"
		"	//outColor = (color1 + color2) * vec4((ambient + diffuse + specular), 1.0f);      \n"
		"}															\n";
	const char fShaderStr_light[] =
		"#version 300 es												\n"
		"precision mediump float;									        \n"
		"layout(location = 0) out vec4 outColor;				           		        \n"
		"void main()												        \n"
		"{														        \n"
		"    outColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);    //光的颜色为纯白                        \n"
		"}															 \n";

	// Load the shaders and get a linked program object
	userData->programObject = esLoadProgram(vShaderStr, fShaderStr);
	userData->lightProgramObject = esLoadProgram(vShaderStr, fShaderStr_light);
	userData->grassProgramObject = esLoadProgram(vShaderStr, fShaderStr_grass);

	// Get the uniform locations
	userData->mvpLoc = glGetUniformLocation(userData->programObject, "u_mvpMatrix");
	userData->mvLoc = glGetUniformLocation(userData->programObject, "u_mvMatrix");
	userData->lightMvpLoc = glGetUniformLocation(userData->lightProgramObject, "u_mvpMatrix");

	// Get light pos and color locations
	userData->lightPosLoc = glGetUniformLocation(userData->programObject, "lightPos");
	userData->lightColorLoc = glGetUniformLocation(userData->programObject, "lightColor");
	userData->eyePosLoc = glGetUniformLocation(userData->programObject, "eyePos");

	// Generate the vertex data
	userData->numIndices = esGenCube(1.0, &userData->vertices,
		NULL, NULL, &userData->indices);

	// Starting rotation angle for the cube
	userData->angle = 45.0f;

	userData->textureID = loadTexture("container.jpg");
	userData->samplerLoc = glGetUniformLocation(userData->programObject, "s_texture");

	userData->textureIdGrass = loadTexture("grass.png");
	userData->samplerLocGrass = glGetUniformLocation(userData->grassProgramObject, "s_texture_grass");
	userData->textureIdBricks = loadTexture("bricks.jpg");
	userData->samplerLocBricks = glGetUniformLocation(userData->grassProgramObject, "s_texture_bricks");

	// generate VBO IDs and load the VBOs with Data
	glGenBuffers(5, userData->vboIDs);
	// VBO0
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[0]);
	glBufferData(GL_ARRAY_BUFFER, 3 * 24 * sizeof(GLfloat), userData->vertices, GL_STATIC_DRAW);

	// VBO1
	// R G B A
	GLfloat cubeColors[] = {
		 1.0f, 0.0f, 0.0f, 1.0f,
		 1.0f, 0.0f, 0.0f, 1.0f,
		 1.0f, 0.0f, 0.0f, 1.0f,
		 1.0f, 0.0f, 0.0f, 1.0f,  // cube0: red

		 0.0f, 1.0f, 0.0f, 1.0f,
		 0.0f, 1.0f, 0.0f, 1.0f,
		 0.0f, 1.0f, 0.0f, 1.0f,
		 0.0f, 1.0f, 0.0f, 1.0f,	// cube1: green

		 0.0f, 0.0f, 1.0f, 1.0f,
		 0.0f, 0.0f, 1.0f, 1.0f,
		 0.0f, 0.0f, 1.0f, 1.0f,
		 0.0f, 0.0f, 1.0f, 1.0f,	// cube2: blue

		 1.0f, 1.0f, 0.0f, 1.0f,
		 1.0f, 1.0f, 0.0f, 1.0f,
		 1.0f, 1.0f, 0.0f, 1.0f,
		 1.0f, 1.0f, 0.0f, 1.0f,	// cube3: yellow

		 1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, 0.0f, 1.0f, 1.0f,	// cube4: purple

		 0.0f, 1.0f, 1.0f, 1.0f,
		 0.0f, 1.0f, 1.0f, 1.0f,
		 0.0f, 1.0f, 1.0f, 1.0f,
		 0.0f, 1.0f, 1.0f, 1.0f	// cube5: light blue
	};
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeColors), cubeColors, GL_STATIC_DRAW);

	// VBO2
	GLfloat cubeTexCoord[] =
	{
	   0.0f, 0.0f,
	   0.0f, 1.0f,
	   1.0f, 1.0f,
	   1.0f, 0.0f,  //cube0

	   1.0f, 0.0f,
	   1.0f, 1.0f,
	   0.0f, 1.0f,
	   0.0f, 0.0f,  //cube1

	   0.0f, 0.0f,
	   0.0f, 1.0f,
	   1.0f, 1.0f,
	   1.0f, 0.0f,  // cube2

	   0.0f, 0.0f,
	   0.0f, 1.0f,
	   1.0f, 1.0f,
	   1.0f, 0.0f,  //bube3

	   0.0f, 0.0f,
	   0.0f, 1.0f,
	   1.0f, 1.0f,
	   1.0f, 0.0f,  //cube4

	   0.0f, 0.0f,
	   0.0f, 1.0f,
	   1.0f, 1.0f,
	   1.0f, 0.0f,  //cube5
	};
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeTexCoord), cubeTexCoord, GL_STATIC_DRAW);

	// VBO3
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, userData->vboIDs[3]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * userData->numIndices, userData->indices, GL_STATIC_DRAW);

	// VBO4
	GLfloat cubeNormals[] =
	{
	   0.0f, -1.0f, 0.0f,
	   0.0f, -1.0f, 0.0f,
	   0.0f, -1.0f, 0.0f,
	   0.0f, -1.0f, 0.0f,  // cube0

	   0.0f, 1.0f, 0.0f,
	   0.0f, 1.0f, 0.0f,
	   0.0f, 1.0f, 0.0f,
	   0.0f, 1.0f, 0.0f,  // cube1

	   0.0f, 0.0f, -1.0f,
	   0.0f, 0.0f, -1.0f,
	   0.0f, 0.0f, -1.0f,
	   0.0f, 0.0f, -1.0f,  // cube2

	   0.0f, 0.0f, 1.0f,
	   0.0f, 0.0f, 1.0f,
	   0.0f, 0.0f, 1.0f,
	   0.0f, 0.0f, 1.0f,  // cube3

	   -1.0f, 0.0f, 0.0f,
	   -1.0f, 0.0f, 0.0f,
	   -1.0f, 0.0f, 0.0f,
	   -1.0f, 0.0f, 0.0f,  // cube4

	   1.0f, 0.0f, 0.0f,
	   1.0f, 0.0f, 0.0f,
	   1.0f, 0.0f, 0.0f,
	   1.0f, 0.0f, 0.0f,  // cube5
	};

	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[4]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeNormals), cubeNormals, GL_STATIC_DRAW);

	/********************************* add VBOs to VAO **************************************/
	// Generate VAO Id
	glGenVertexArrays(1, &userData->vaoID);
	// Bind the VAO and then setup the vertex
	// attributes
	glBindVertexArray(userData->vaoID);

	// Load the vertex position
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const void*)0);
	// Load the cube colors
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[1]);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void*)0);
	// Load the cure texture coord
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[2]);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, userData->vboIDs[3]);

	// Load the cure texture coord
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[4]);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const void*)0);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	// Reset to the default VAO
	glBindVertexArray(0);
	/****************************************************************************************/

	// light VAO 
	/***************************************************************************************/
	// Generate VAO Id
	glGenVertexArrays(1, &userData->lightVaoID);
	// Bind the VAO and then setup the vertex
	// attributes
	glBindVertexArray(userData->lightVaoID);
	// 只需要绑定VBO不用再次设置VBO的数据，因为箱子的VBO数据中已经包含了正确的立方体顶点数据
	glBindBuffer(GL_ARRAY_BUFFER, userData->vboIDs[0]);
	// 设置灯立方体的顶点属性（对我们的灯来说仅仅只有位置数据）
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, userData->vboIDs[3]);
	glEnableVertexAttribArray(0);
	/***************************************************************************************/
	// Reset to the default VAO
	glBindVertexArray(0);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_DEPTH_TEST); // must enable depth otherwise the cue look very strange
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return GL_TRUE;
}


///
// Update MVP matrix based on time
//
void Update(ESContext *esContext, float deltaTime)
{
	UserData *userData = esContext->userData;

	// Compute a rotation angle based on time to rotate the cube
	userData->angle += (deltaTime * 40.0f);

	if (userData->angle >= 360.0f)
	{
		userData->angle -= 360.0f;
	}
}

void objectMvpSet(ESContext *esContext, GLint i, GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ)
{
	UserData *userData = esContext->userData;
	ESMatrix perspective;
	ESMatrix modelview;
	ESMatrix view;
	float    aspect;

	GLfloat cubePositions[] = {
	  0.0f,  0.0f,  0.0f,
	  2.0f,  5.0f, -15.0f,
	  -1.5f, -2.2f, -2.5f,
	  -3.8f, -2.0f, -12.3f,
	  2.4f, -0.4f, -3.5f,
	  -1.7f,  3.0f, -7.5f,
	  1.3f, -2.0f, -2.5f,
	  1.5f,  2.0f, -2.5f,
	  0.0f,  0.0f, -2.0f,
	  -1.3f,  1.0f, -1.5f
	};

	GLfloat cubeRoateDir[] = {
		1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 1.0f
	};

	// Compute the window aspect ratio
	aspect = (GLfloat)esContext->width / (GLfloat)esContext->height;

	// Generate a perspective matrix with a 60 degree FOV
	esMatrixLoadIdentity(&perspective);
	esPerspective(&perspective, 60.0f, aspect, 1.0f, 100.0f);

	// Generate a model view matrix to rotate/translate the cube
	esMatrixLoadIdentity(&modelview);

	// Translate away from the viewer
	esTranslate(&modelview, cubePositions[3 * i], cubePositions[3 * i + 1], cubePositions[3 * i + 2]);

	// Rotate the cube
	esRotate(&modelview, userData->angle, cubeRoateDir[3 * i], cubeRoateDir[3 * i + 1], cubeRoateDir[3 * i + 2]);
	memcpy(&userData->mvMatrix, &modelview, sizeof(ESMatrix));

	//esLogMessage("eyeZ = %f\n", eyeZ);

	esMatrixLookAt(&view,
		eyeX, eyeY, eyeZ,
		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f);

	esMatrixMultiply(&modelview, &modelview, &view);

	// Compute the final MVP by multiplying the
	// modevleiw and perspective matrices together
	esMatrixMultiply(&userData->mvpMatrix, &modelview, &perspective);
}

void lightMvpSet(ESContext *esContext, GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ)
{
	UserData *userData = esContext->userData;
	ESMatrix perspective;
	ESMatrix modelview;
	ESMatrix view;
	float    aspect;

	// Compute the window aspect ratio
	aspect = (GLfloat)esContext->width / (GLfloat)esContext->height;

	// Generate a perspective matrix with a 60 degree FOV
	esMatrixLoadIdentity(&perspective);
	esPerspective(&perspective, 60.0f, aspect, 1.0f, 100.0f);

	// Generate a model view matrix to rotate/translate the cube
	esMatrixLoadIdentity(&modelview);

	// Translate away from the viewer
	esTranslate(&modelview, 6.0f, 6.0f, -1.0f);

	esMatrixLookAt(&view,
		eyeX, eyeY, eyeZ,    // eye position
		0.0f, 0.0f, 0.0f,    // eye look at location
		0.0f, 1.0f, 0.0f);   // up direction vec

	esMatrixMultiply(&modelview, &modelview, &view);
	// Compute the final MVP by multiplying the
	// modevleiw and perspective matrices together
	esMatrixMultiply(&userData->lightMvpMatrix, &modelview, &perspective);
}

///
// Draw a triangle using the shader pair created in Init()
//
void Draw(ESContext *esContext)
{
	UserData *userData = esContext->userData;

	// Set the viewport
	glViewport(0, 0, esContext->width, esContext->height);

	// Clear the color buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//caculate eye position
	static GLfloat delt = -0.005f;
	static GLfloat eyeZ = 10.0f;
	if (eyeZ > 10.0f || eyeZ < 0.0f) {
		delt = -1 * delt;
	}

	eyeZ += delt;

	/********(1) 绘制1到5个箱子--贴图为箱子加不同的六面颜色 *********/
	// Use the program object
	glUseProgram(userData->programObject);

	// Bind the VAO
	glBindVertexArray(userData->vaoID);

	// Bind the texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, userData->textureID);
	glUniform1i(userData->samplerLoc, 0);

	glUniform3f(userData->lightColorLoc, 1.0f, 1.0f, 1.0f);

	GLfloat sinAngle = sinf(userData->angle * PI / 180.0f);
	GLfloat cosAngle = cosf(userData->angle * PI / 180.0f);
	//GLfloat lightPosX = 6.0f * cosAngle;
	//GLfloat lightPosZ = 6.0f * sinAngle;
	GLfloat eyeX = 6.0f * cosAngle;
	GLfloat eyeY = 6.0f * sinAngle;
	glUniform3f(userData->lightPosLoc, 6.0f, 6.0f, -1.0f);
	//glUniform3f(userData->lightPosLoc, lightPosX, 6.0f, lightPosZ);
	glUniform3f(userData->eyePosLoc, 0.0f, 0.0f, eyeZ);
	//glUniform3f(userData->eyePosLoc, eyeX, eyeY, 10.0f);

	GLint i = 0;
	for (i = 5; i < 10; i++) {
		objectMvpSet(esContext, i, 0.0f, 0.0f, eyeZ);
		//objectMvpSet(esContext, i, eyeX, eyeY, 10.0f);
		// Load the M matrix
		glUniformMatrix4fv(userData->mvLoc, 1, GL_FALSE, (GLfloat *)&userData->mvMatrix.m[0][0]);
		// Load the MVP matrix
		glUniformMatrix4fv(userData->mvpLoc, 1, GL_FALSE, (GLfloat *)&userData->mvpMatrix.m[0][0]);

		// Draw the cube
		glDrawElements(GL_TRIANGLES, userData->numIndices, GL_UNSIGNED_INT, (const void *)0);
	}

	/********(2) 绘制6到10个箱子--贴图为草和砖头 *********/
	// Use the program object
	glUseProgram(userData->grassProgramObject);
	// Bind the VAO
	glBindVertexArray(userData->vaoID);
	// Bind the texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, userData->textureIdBricks);
	glUniform1i(userData->samplerLocBricks, 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, userData->textureIdGrass);
	glUniform1i(userData->samplerLocGrass, 2);

	glUniform3f(userData->lightColorLoc, 1.0f, 1.0f, 1.0f);
	glUniform3f(userData->lightPosLoc, 6.0f, 6.0f, -1.0f);
	glUniform3f(userData->eyePosLoc, 0.0f, 0.0f, eyeZ);
	//glUniform3f(userData->eyePosLoc, eyeX, eyeY, 10.0f);

	for (i = 0; i < 5; i++) {
		objectMvpSet(esContext, i, 0.0f, 0.0f, eyeZ);
		//objectMvpSet(esContext, i, eyeX, eyeY, 10.0f);
		// Load the M matrix
		glUniformMatrix4fv(userData->mvLoc, 1, GL_FALSE, (GLfloat *)&userData->mvMatrix.m[0][0]);
		// Load the MVP matrix
		glUniformMatrix4fv(userData->mvpLoc, 1, GL_FALSE, (GLfloat *)&userData->mvpMatrix.m[0][0]);

		// Draw the cube
		glDrawElements(GL_TRIANGLES, userData->numIndices, GL_UNSIGNED_INT, (const void *)0);
	}

	// Return to the default VAO
	glBindVertexArray(0);

	/********(3) 绘制光源 *********/
	// Use the program object
	glUseProgram(userData->lightProgramObject);

	// Bind the VAO
	glBindVertexArray(userData->lightVaoID);

	lightMvpSet(esContext, 0.0f, 0.0f, eyeZ);
	//lightMvpSet(esContext, eyeX, eyeY, 10.0f);

	// Load the MVP matrix
	glUniformMatrix4fv(userData->lightMvpLoc, 1, GL_FALSE, (GLfloat *)&userData->lightMvpMatrix.m[0][0]);

	// Draw the cube
	glDrawElements(GL_TRIANGLES, userData->numIndices, GL_UNSIGNED_INT, (const void *)0);

	// Return to the default VAO
	glBindVertexArray(0);

}

///
// Cleanup
//
void Shutdown(ESContext *esContext)
{
	UserData *userData = esContext->userData;

	if (userData->vertices != NULL)
	{
		free(userData->vertices);
	}

	if (userData->indices != NULL)
	{
		free(userData->indices);
	}

	glDeleteTextures(1, &userData->textureID);
	// Delete program object
	glDeleteProgram(userData->programObject);
}


int esMain(ESContext *esContext)
{
	esContext->userData = malloc(sizeof(UserData));

	esCreateWindow(esContext, "Simple_VertexShader", 1280, 720, ES_WINDOW_RGB | ES_WINDOW_ALPHA | ES_WINDOW_DEPTH);

	if (!Init(esContext))
	{
		return GL_FALSE;
	}

	esRegisterShutdownFunc(esContext, Shutdown);
	esRegisterUpdateFunc(esContext, Update);
	esRegisterDrawFunc(esContext, Draw);

	return GL_TRUE;
}


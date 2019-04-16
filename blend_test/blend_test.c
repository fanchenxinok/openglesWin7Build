/**
*** AUTHOR: Fanchenxin
*** DATE: 2018/11/09
***/
#include <stdio.h>
#include <stdlib.h>
#include "esUtil.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_TEXTURE_PER_LAYER   (20)  // the textures that each layer can have
#define MUTI_PROGRAM_ENABLE   (0) //if enable each layer can control alpha value, else each layer just have show or hide two status

#define PI 3.1415926535897932384626433832795f

typedef enum
{
	LAYER_ID_0 = 0,
	LAYER_ID_1,
	LAYER_ID_2,
	LAYER_ID_3,
	LAYER_MAX
};

typedef enum
{
	SCREEN_TO_TEXTURE,
	TEXTURE_TO_SCREEN
}enTRANS_TYPE;

typedef enum
{
	RESET_DISP_AREA,
	RESET_CLIP_AREA,
	ROTATE_AREA_CENTER
}enVERTEX_SET_TYPE;

typedef struct
{
	GLint left;
	GLint top;
	GLint width;
	GLint height;
}stRect;

typedef struct
{
	GLuint width;
	GLuint height;
}stTexSize;

typedef struct
{
	GLfloat x;
	GLfloat y;
}stPos;

typedef struct
{
#if MUTI_PROGRAM_ENABLE
	GLuint programObjects[LAYER_MAX];	// Handle to a program object
	GLint samplerLocs[LAYER_MAX];	    // Sampler location
	GLint ctlAlphaLocs[LAYER_MAX];      // control alpha location
#else
	GLuint programObject;
	GLint samplerLoc;
#endif

	GLfloat alphas[LAYER_MAX];
	GLuint textureIds[LAYER_MAX][MAX_TEXTURE_PER_LAYER];	// Texture handle
	GLuint textureNumPerLayer[LAYER_MAX];
	GLuint vboIds[LAYER_MAX][MAX_TEXTURE_PER_LAYER];	// VBO IDs
	GLuint vboIndiceId;                                 // indice VBO Id
	GLuint vaoIds[LAYER_MAX][MAX_TEXTURE_PER_LAYER];	// VAO IDs
	stRect dispArea[LAYER_MAX][MAX_TEXTURE_PER_LAYER]; // display area of each texture in window
	stRect clipArea[LAYER_MAX][MAX_TEXTURE_PER_LAYER]; // clip area of each texture
	stTexSize texSize[LAYER_MAX][MAX_TEXTURE_PER_LAYER]; // texture width and height
	stPos texCenterPos[LAYER_MAX][MAX_TEXTURE_PER_LAYER]; // save every texture center vexture coordinate
	GLboolean texVisable[LAYER_MAX][MAX_TEXTURE_PER_LAYER]; // indicate the texture is visable
	GLuint winWidth;  // windows width
	GLuint winHeight;

	GLfloat *vertices[LAYER_MAX][MAX_TEXTURE_PER_LAYER];
	GLushort *indices;
	GLushort verticeSize;
	GLushort indiceNum;

	GLubyte *dumpPixels;
	GLubyte *holePixes;
} stUserData;

static const char* s_images[LAYER_MAX][MAX_TEXTURE_PER_LAYER] = {
	{"bricks.jpg", "crack.png", "christmas.png"},
	{"window.png", "tacho_bg.png", "pointer_0_160x40_25600.png"},
	{"grass.png", "grass.png", "grass.png", "grass.png"},
	{"sun.png", "FC_level.png",  "FC_level_1.png", "FC_level_2.png", "FC_level_3.png", "FC_level_4.png"}
};

///
// Load texture from disk
//

GLint loadTexture(const char* name, GLint *outWidth, GLint *outHeight)
{
	unsigned int texture;
	glGenTextures(1, &texture);

	int width, height, nrChannels;
	unsigned char *data = stbi_load(name, &width, &height, &nrChannels, 0);
	GLint format = GL_RGB;
	if (data) {
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
		if (outWidth) *outWidth = width;
		if (outHeight) *outHeight = height;

		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (format == GL_RGBA) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (format == GL_RGBA) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		//glGenerateMipmap(GL_TEXTURE_2D);
		stbi_image_free(data);
	}
	else {
		esLogMessage("Failed to load texture: %s\n", name);
	}

	return texture;
}

// texture must RGBA format 
void digHoleInTexture(stUserData *userData, GLint texture, stRect *pRect, GLubyte alpha)
{
	GLint i = 0;
	for (i = 3; i < userData->winWidth * userData->winHeight * 4; i += 4) {
		userData->holePixes[i] = alpha;
	}
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, pRect->left, pRect->top, pRect->width, pRect->height, GL_RGBA, GL_UNSIGNED_BYTE, userData->holePixes);
}

static GLfloat coordinateTrans(GLfloat coord, enTRANS_TYPE type)
{
	return (type == SCREEN_TO_TEXTURE) ? (coord + 1) / 2 : 2 * coord - 1;
}

static void initVertexData(stUserData *pUser, GLuint layer, GLuint texIdx)
{
	GLfloat vVertices[] = { -1.0f,  1.0f, 0.0f,  // Position 0
						0.0f,  0.0f,        // TexCoord 0 
					   -1.0f, -1.0f, 0.0f,  // Position 1
						0.0f,  1.0f,        // TexCoord 1
						1.0f, -1.0f, 0.0f,  // Position 2
						1.0f,  1.0f,        // TexCoord 2
						1.0f,  1.0f, 0.0f,  // Position 3
						1.0f,  0.0f         // TexCoord 3
	};

	pUser->verticeSize = sizeof(vVertices);
	pUser->vertices[layer][texIdx] = (GLfloat*)malloc(pUser->verticeSize);
	memcpy(pUser->vertices[layer][texIdx], vVertices, pUser->verticeSize);
}

static void setVertexData(stUserData *pUser, GLuint layer, GLuint texIdx, enVERTEX_SET_TYPE setType, GLfloat angle)
{


	switch (setType)
	{

	case RESET_DISP_AREA:
	{
		stRect *pDispArea = &pUser->dispArea[layer][texIdx];
		GLfloat left = coordinateTrans((GLfloat)pDispArea->left / pUser->winWidth, TEXTURE_TO_SCREEN);
		GLfloat top = -1.0f * coordinateTrans((GLfloat)pDispArea->top / pUser->winHeight, TEXTURE_TO_SCREEN);
		GLfloat right = coordinateTrans((GLfloat)(pDispArea->left + pDispArea->width) / pUser->winWidth, TEXTURE_TO_SCREEN);
		GLfloat bottom = -1.0f * coordinateTrans((GLfloat)(pDispArea->top + pDispArea->height) / pUser->winHeight, TEXTURE_TO_SCREEN);

		//printf("[%f, %f] - [%f, %f] \n", left, top, right, bottom);

		enum {
			LEFT_TOP_X = 0,
			LEFT_TOP_Y = 1,  // left top coord index
			LEFT_BOT_X = 5,
			LEFT_BOT_Y = 6,  // left bottom coord index
			RIGHT_BOT_X = 10,
			RIGHT_BOT_Y = 11,
			RIGHT_TOP_X = 15,
			RIGHT_TOP_Y = 16
		};

		pUser->vertices[layer][texIdx][LEFT_TOP_X] = left;
		pUser->vertices[layer][texIdx][LEFT_TOP_Y] = top;

		pUser->vertices[layer][texIdx][LEFT_BOT_X] = left;
		pUser->vertices[layer][texIdx][LEFT_BOT_Y] = bottom;

		pUser->vertices[layer][texIdx][RIGHT_BOT_X] = right;
		pUser->vertices[layer][texIdx][RIGHT_BOT_Y] = bottom;

		pUser->vertices[layer][texIdx][RIGHT_TOP_X] = right;
		pUser->vertices[layer][texIdx][RIGHT_TOP_Y] = top;

		pUser->texCenterPos[layer][texIdx].x = (right - left) / 2 + left;
		pUser->texCenterPos[layer][texIdx].y = (top - bottom) / 2 + bottom;
	}break;
	case RESET_CLIP_AREA:
	{
		stRect *pClipArea = &pUser->clipArea[layer][texIdx];
		GLfloat left = (GLfloat)pClipArea->left / pUser->texSize[layer][texIdx].width;
		GLfloat top = (GLfloat)pClipArea->top / pUser->texSize[layer][texIdx].height;
		GLfloat right = (GLfloat)(pClipArea->left + pClipArea->width) / pUser->texSize[layer][texIdx].width;
		GLfloat bottom = (GLfloat)(pClipArea->top + pClipArea->height) / pUser->texSize[layer][texIdx].height;

		//printf("[%f, %f] - [%f, %f] \n", left, top, right, bottom);

		enum {
			LEFT_TOP_X = 3,
			LEFT_TOP_Y = 4,  // left top coord index
			LEFT_BOT_X = 8,
			LEFT_BOT_Y = 9,  // left bottom coord index
			RIGHT_BOT_X = 13,
			RIGHT_BOT_Y = 14,
			RIGHT_TOP_X = 18,
			RIGHT_TOP_Y = 19
		};

		pUser->vertices[layer][texIdx][LEFT_TOP_X] = left;
		pUser->vertices[layer][texIdx][LEFT_TOP_Y] = top;

		pUser->vertices[layer][texIdx][LEFT_BOT_X] = left;
		pUser->vertices[layer][texIdx][LEFT_BOT_Y] = bottom;

		pUser->vertices[layer][texIdx][RIGHT_BOT_X] = right;
		pUser->vertices[layer][texIdx][RIGHT_BOT_Y] = bottom;

		pUser->vertices[layer][texIdx][RIGHT_TOP_X] = right;
		pUser->vertices[layer][texIdx][RIGHT_TOP_Y] = top;
	}break;
	case ROTATE_AREA_CENTER:
	{
		// rotate the texture around the center point of this texture, rotate angle base current angle
		GLfloat sinAngle, cosAngle;
		sinAngle = sinf(angle * PI / 180.0f);
		cosAngle = cosf(angle * PI / 180.0f);
		enum {
			LEFT_TOP_X = 0,
			LEFT_TOP_Y = 1,  // left top coord index
			LEFT_BOT_X = 5,
			LEFT_BOT_Y = 6,  // left bottom coord index
			RIGHT_BOT_X = 10,
			RIGHT_BOT_Y = 11,
			RIGHT_TOP_X = 15,
			RIGHT_TOP_Y = 16
		};

		GLfloat ratio_x = 1.0, ratio_y = 1.0;
		if (pUser->winWidth > pUser->winHeight) {
			ratio_y = (GLfloat)pUser->winWidth / pUser->winHeight;
		}
		else if (pUser->winWidth < pUser->winHeight) {
			ratio_x = (GLfloat)pUser->winHeight / pUser->winWidth;
		}
		else {
			// DO NOTHING
		}

		GLfloat center_x = pUser->texCenterPos[layer][texIdx].x;
		GLfloat center_y = pUser->texCenterPos[layer][texIdx].y;
		GLfloat x = pUser->vertices[layer][texIdx][LEFT_TOP_X] - center_x;
		GLfloat y = pUser->vertices[layer][texIdx][LEFT_TOP_Y] - center_y;
		pUser->vertices[layer][texIdx][LEFT_TOP_X] = x * cosAngle - y * sinAngle + center_x;
		pUser->vertices[layer][texIdx][LEFT_TOP_Y] = x * sinAngle + y * cosAngle + center_y;

		x = pUser->vertices[layer][texIdx][LEFT_BOT_X] - center_x;
		y = pUser->vertices[layer][texIdx][LEFT_BOT_Y] - center_y;
		pUser->vertices[layer][texIdx][LEFT_BOT_X] = x * cosAngle - y * sinAngle + center_x;
		pUser->vertices[layer][texIdx][LEFT_BOT_Y] = x * sinAngle + y * cosAngle + center_y;

		x = pUser->vertices[layer][texIdx][RIGHT_BOT_X] - center_x;
		y = pUser->vertices[layer][texIdx][RIGHT_BOT_Y] - center_y;
		pUser->vertices[layer][texIdx][RIGHT_BOT_X] = x * cosAngle - y * sinAngle + center_x;
		pUser->vertices[layer][texIdx][RIGHT_BOT_Y] = x * sinAngle + y * cosAngle + center_y;

		x = pUser->vertices[layer][texIdx][RIGHT_TOP_X] - center_x;
		y = pUser->vertices[layer][texIdx][RIGHT_TOP_Y] - center_y;
		pUser->vertices[layer][texIdx][RIGHT_TOP_X] = x * cosAngle - y * sinAngle + center_x;
		pUser->vertices[layer][texIdx][RIGHT_TOP_Y] = x * sinAngle + y * cosAngle + center_y;
	}break;
	default:
		// do nothing
		break;
	}
}

void initDispArea(stUserData *pUser, GLuint layer, GLint x, GLint y, GLint width, GLint height)
{
	if (pUser) {
		x = (x < 0) ? 0 : x;
		y = (y < 0) ? 0 : y;
		width = ((x + width) > pUser->winWidth) ? (pUser->winWidth - x) : width;
		height = ((y + height) > pUser->winHeight) ? (pUser->winHeight - y) : height;
		if (pUser->textureNumPerLayer[layer] < MAX_TEXTURE_PER_LAYER) {
			pUser->dispArea[layer][pUser->textureNumPerLayer[layer]].left = x;
			pUser->dispArea[layer][pUser->textureNumPerLayer[layer]].top = y;
			pUser->dispArea[layer][pUser->textureNumPerLayer[layer]].width = width;
			pUser->dispArea[layer][pUser->textureNumPerLayer[layer]].height = height;

			initVertexData(pUser, layer, pUser->textureNumPerLayer[layer]);
			/** Load vertex data **/
			setVertexData(pUser, layer, pUser->textureNumPerLayer[layer], RESET_DISP_AREA, 0.0);
			pUser->textureNumPerLayer[layer]++;
		}
		else {
			esLogMessage("Layer: %d is full, texture number = %d\n", layer, pUser->textureNumPerLayer[layer]);
		}
	}
}

void setDispArea(stUserData *pUser, GLuint layer, GLuint texIdx, stRect *pRect)
{
	if (pUser && pRect) {
		pRect->left = (pRect->left < 0) ? 0 : pRect->left;
		pRect->top = (pRect->top < 0) ? 0 : pRect->top;
		pRect->width = ((pRect->left + pRect->width) > pUser->winWidth) ? (pUser->winWidth - pRect->left) : pRect->width;
		pRect->height = ((pRect->top + pRect->height) > pUser->winHeight) ? (pUser->winHeight - pRect->top) : pRect->height;
		if (texIdx < MAX_TEXTURE_PER_LAYER) {
			pUser->dispArea[layer][texIdx].left = pRect->left;
			pUser->dispArea[layer][texIdx].top = pRect->top;
			pUser->dispArea[layer][texIdx].width = pRect->width;
			pUser->dispArea[layer][texIdx].height = pRect->height;

			setVertexData(pUser, layer, texIdx, RESET_DISP_AREA, 0.0);
		}
		else {
			esLogMessage("Layer: %d is full, texture number = %d is Invalid\n", layer, texIdx);
		}
	}
}

void setClipArea(stUserData *pUser, GLuint layer, GLuint texIdx, stRect *pRect)
{
	if (pUser && pRect) {
		pRect->left = (pRect->left < 0) ? 0 : pRect->left;
		pRect->top = (pRect->top < 0) ? 0 : pRect->top;
		pRect->width = ((pRect->left + pRect->width) > pUser->texSize[layer][texIdx].width) ? (pUser->texSize[layer][texIdx].width - pRect->left) : pRect->width;
		pRect->height = ((pRect->top + pRect->height) > pUser->texSize[layer][texIdx].height) ? (pUser->texSize[layer][texIdx].height - pRect->top) : pRect->height;
		if (texIdx < MAX_TEXTURE_PER_LAYER) {
			pUser->clipArea[layer][texIdx].left = pRect->left;
			pUser->clipArea[layer][texIdx].top = pRect->top;
			pUser->clipArea[layer][texIdx].width = pRect->width;
			pUser->clipArea[layer][texIdx].height = pRect->height;

			setVertexData(pUser, layer, texIdx, RESET_CLIP_AREA, 0.0);
		}
		else {
			esLogMessage("Layer: %d is full, texture number = %d is Invalid\n", layer, texIdx);
		}
	}
}

// rotate texture around pCenterPos, rotate angle base 0 degree.
void setRotateArea(stUserData *pUser, GLuint layer, GLuint texIdx, GLfloat angle, stPos *pCenterPos)
{

	stRect *pDispArea = &pUser->dispArea[layer][texIdx];
	GLfloat x1 = coordinateTrans((GLfloat)pDispArea->left / pUser->winWidth, TEXTURE_TO_SCREEN);
	GLfloat y1 = -1.0f * coordinateTrans((GLfloat)pDispArea->top / pUser->winHeight, TEXTURE_TO_SCREEN);
	GLfloat x2 = coordinateTrans((GLfloat)(pDispArea->left + pDispArea->width) / pUser->winWidth, TEXTURE_TO_SCREEN);
	GLfloat y2 = -1.0f * coordinateTrans((GLfloat)(pDispArea->top + pDispArea->height) / pUser->winHeight, TEXTURE_TO_SCREEN);

	GLfloat sinAngle, cosAngle;
	sinAngle = sinf(angle * PI / 180.0f);
	cosAngle = cosf(angle * PI / 180.0f);
	enum {
		LEFT_TOP_X = 0,
		LEFT_TOP_Y = 1,  // left top coord index
		LEFT_BOT_X = 5,
		LEFT_BOT_Y = 6,  // left bottom coord index
		RIGHT_BOT_X = 10,
		RIGHT_BOT_Y = 11,
		RIGHT_TOP_X = 15,
		RIGHT_TOP_Y = 16
	};

	//esLogMessage("	x1 = %f, y1 = %f\n", x1, y1);
	//esLogMessage("	x1 = %f, y2 = %f\n", x1, y2);
	//esLogMessage("	x2 = %f, y2 = %f\n", x2, y2);
	//esLogMessage("	x2 = %f, y1 = %f\n", x2, y1);

	GLfloat ratio_x = 1.0, ratio_y = 1.0;
	if (pUser->winWidth > pUser->winHeight) {
		ratio_y = (GLfloat)pUser->winWidth / pUser->winHeight;
	}
	else if (pUser->winWidth < pUser->winHeight) {
		ratio_x = (GLfloat)pUser->winHeight / pUser->winWidth;
	}
	else {
		// DO NOTHING
	}

	GLfloat center_x = coordinateTrans((GLfloat)pCenterPos->x / pUser->winWidth, TEXTURE_TO_SCREEN);
	GLfloat center_y = -1.0f * coordinateTrans((GLfloat)pCenterPos->y / pUser->winHeight, TEXTURE_TO_SCREEN);
	x1 -= center_x;
	y1 -= center_y;
	x2 -= center_x;
	y2 -= center_y;

	// x1, y1
	pUser->vertices[layer][texIdx][LEFT_TOP_X] = (x1* cosAngle - y1 * sinAngle) * ratio_x + center_x;
	pUser->vertices[layer][texIdx][LEFT_TOP_Y] = (x1 * sinAngle + y1 * cosAngle) * ratio_y + center_y;
	// x1, y2
	pUser->vertices[layer][texIdx][LEFT_BOT_X] = (x1 * cosAngle - y2 * sinAngle) * ratio_x + center_x;
	pUser->vertices[layer][texIdx][LEFT_BOT_Y] = (x1 * sinAngle + y2 * cosAngle) * ratio_y + center_y;
	// x2, y2
	pUser->vertices[layer][texIdx][RIGHT_BOT_X] = (x2 * cosAngle - y2 * sinAngle) * ratio_x + center_x;
	pUser->vertices[layer][texIdx][RIGHT_BOT_Y] = (x2 * sinAngle + y2 * cosAngle) * ratio_y + center_y;
	// x2, y1
	pUser->vertices[layer][texIdx][RIGHT_TOP_X] = (x2 * cosAngle - y1 * sinAngle) * ratio_x + center_x;
	pUser->vertices[layer][texIdx][RIGHT_TOP_Y] = (x2 * sinAngle + y1 * cosAngle) * ratio_y + center_y;

	//esLogMessage("center: x = %f, y = %f\n", center_x, center_y);
	//esLogMessage("	left = %f, top = %f\n", pUser->vertices[layer][texIdx][LEFT_TOP_X], pUser->vertices[layer][texIdx][LEFT_TOP_Y]);
	//esLogMessage("	left = %f, bottom = %f\n", pUser->vertices[layer][texIdx][LEFT_BOT_X], pUser->vertices[layer][texIdx][LEFT_BOT_Y]);
	//esLogMessage("	right = %f, bottom = %f\n", pUser->vertices[layer][texIdx][RIGHT_BOT_X], pUser->vertices[layer][texIdx][RIGHT_BOT_Y]);
	//esLogMessage("	right = %f, top = %f\n", pUser->vertices[layer][texIdx][RIGHT_TOP_X], pUser->vertices[layer][texIdx][RIGHT_TOP_Y]);
	//esLogMessage("-------------------------------------------------------\n");
}

void rotateTextureCoord(stUserData *pUser, GLuint layer, GLuint texIdx, GLfloat angle)
{
	GLfloat sinAngle, cosAngle;
	sinAngle = sinf(angle * PI / 180.0f);
	cosAngle = cosf(angle * PI / 180.0f);

	enum {
		LEFT_TOP_X = 3,
		LEFT_TOP_Y = 4,  // left top coord index
		LEFT_BOT_X = 8,
		LEFT_BOT_Y = 9,  // left bottom coord index
		RIGHT_BOT_X = 13,
		RIGHT_BOT_Y = 14,
		RIGHT_TOP_X = 18,
		RIGHT_TOP_Y = 19
	};

	// (0, 0) -> -0.5, -0.5
	pUser->vertices[layer][texIdx][LEFT_TOP_X] = -0.5 * cosAngle + 0.5 * sinAngle + 0.5;
	pUser->vertices[layer][texIdx][LEFT_TOP_Y] = -0.5 * sinAngle - 0.5 * cosAngle + 0.5;
	// (0, 1) -> -0.5, 0.5
	pUser->vertices[layer][texIdx][LEFT_BOT_X] = -0.5 * cosAngle - 0.5 * sinAngle + 0.5;
	pUser->vertices[layer][texIdx][LEFT_BOT_Y] = -0.5 * sinAngle + 0.5 * cosAngle + 0.5;
	// (1, 1) -> 0.5, 0.5
	pUser->vertices[layer][texIdx][RIGHT_BOT_X] = 0.5 * cosAngle - 0.5 * sinAngle + 0.5;
	pUser->vertices[layer][texIdx][RIGHT_BOT_Y] = 0.5 * sinAngle + 0.5 * cosAngle + 0.5;
	// (1, 0) -> 0.5, -0.5
	pUser->vertices[layer][texIdx][RIGHT_TOP_X] = 0.5 * cosAngle + 0.5 * sinAngle + 0.5;
	pUser->vertices[layer][texIdx][RIGHT_TOP_Y] = 0.5 * sinAngle - 0.5 * cosAngle + 0.5;
}


void setLayerAlpha(stUserData *userData, GLuint layer, GLfloat alpha)
{
	userData->alphas[layer] = alpha;
}

void updateVAO(stUserData *pUser, GLuint layer, GLuint texId)
{
	if ((layer >= LAYER_MAX) || (texId >= MAX_TEXTURE_PER_LAYER)) {
		return;
	}
	else {
		glBindBuffer(GL_ARRAY_BUFFER, pUser->vboIds[layer][texId]);
		glBufferData(GL_ARRAY_BUFFER, pUser->verticeSize, pUser->vertices[layer][texId], GL_STATIC_DRAW);

		// Bind the VAO and then setup the vertex
		glBindVertexArray(pUser->vaoIds[layer][texId]);
		// Load the vertex position
		glBindBuffer(GL_ARRAY_BUFFER, pUser->vboIds[layer][texId]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const void*)(3 * sizeof(GLfloat)));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pUser->vboIndiceId);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		// Reset to the default VAO
		glBindVertexArray(0);
	}
}

void createVAOs(stUserData *pUser)
{
	stUserData *userData = pUser;
	glGenBuffers(1, &userData->vboIndiceId);
	// VBO of indice
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, userData->vboIndiceId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, userData->indiceNum * sizeof(GLushort), userData->indices, GL_STATIC_DRAW);

	GLuint layer = LAYER_ID_0;
	/********* BIND VBOs *********/
	for (layer = LAYER_ID_0; layer < LAYER_MAX; layer++) {
		glGenBuffers(userData->textureNumPerLayer[layer], userData->vboIds[layer]);
		esLogMessage("layer: %d, VBO cnt = %d\n", layer, userData->textureNumPerLayer[layer]);

		// Generate VAO Id
		glGenVertexArrays(userData->textureNumPerLayer[layer], userData->vaoIds[layer]);
		GLuint text_idx = 0;
		for (; text_idx < userData->textureNumPerLayer[layer]; text_idx++) {
			glBindBuffer(GL_ARRAY_BUFFER, userData->vboIds[layer][text_idx]);
			glBufferData(GL_ARRAY_BUFFER, userData->verticeSize, userData->vertices[layer][text_idx], GL_STATIC_DRAW);

			// Bind the VAO and then setup the vertex
			glBindVertexArray(userData->vaoIds[layer][text_idx]);
			// Load the vertex position
			glBindBuffer(GL_ARRAY_BUFFER, userData->vboIds[layer][text_idx]);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const void*)0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const void*)(3 * sizeof(GLfloat)));

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, userData->vboIndiceId);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);

			// Reset to the default VAO
			glBindVertexArray(0);
		}
	}
}
///
// Initialize the shader and program object
//
int Init(ESContext *esContext)
{
	stUserData *userData = esContext->userData;
	memset(userData->textureNumPerLayer, 0, sizeof(userData->textureNumPerLayer));

	userData->dumpPixels = (GLubyte*)malloc(userData->winWidth * userData->winHeight * 4 * sizeof(GLubyte));

	userData->holePixes = (GLubyte*)malloc(sizeof(GLubyte) * userData->winWidth * userData->winHeight * 4);
	memset(userData->holePixes, 0, sizeof(GLubyte) * userData->winWidth * userData->winHeight * 4);

	// layer0 have 3 texture
	initDispArea(userData, LAYER_ID_0, 0, 0, userData->winWidth, userData->winHeight);
	initDispArea(userData, LAYER_ID_0, 0, 0, userData->winWidth, userData->winHeight);
	initDispArea(userData, LAYER_ID_0, 200, 180, 200, 128);

	// layer1 have 3 texture
	initDispArea(userData, LAYER_ID_1, 100, 100, 400, 300);
	initDispArea(userData, LAYER_ID_1, 640, 100, 582, 582);
	initDispArea(userData, LAYER_ID_1, 931, 391, 160, 40);

	// layer2 have 4 texture
	initDispArea(userData, LAYER_ID_2, 320, 420, 500, 300);
	initDispArea(userData, LAYER_ID_2, 520, 420, 500, 300);
	initDispArea(userData, LAYER_ID_2, 900, 520, 300, 200);
	initDispArea(userData, LAYER_ID_2, 100, 700, 30, 20);

	// layer3 have 6 texture
	initDispArea(userData, LAYER_ID_3, 900, 100, 200, 200);
	initDispArea(userData, LAYER_ID_3, 160, 691, 90, 29);
	initDispArea(userData, LAYER_ID_3, 160, 691, 90, 29);
	initDispArea(userData, LAYER_ID_3, 160, 691, 90, 29);
	initDispArea(userData, LAYER_ID_3, 160, 691, 90, 29);
	initDispArea(userData, LAYER_ID_3, 160, 691, 90, 29);

	char vShaderStr[] =
		"#version 300 es                            \n"
		"layout(location = 0) in vec4 a_position;   \n"
		"layout(location = 1) in vec2 a_texCoord;   \n"
		"out vec2 v_texCoord;                       \n"
		"void main()                                \n"
		"{                                          \n"
		"   gl_Position = a_position;               \n"
		"   v_texCoord = a_texCoord;                \n"
		"}                                          \n";

	char fShaderStr[] =
		"#version 300 es                                     \n"
		"precision mediump float;                            \n"
		"in vec2 v_texCoord;                                 \n"
		"layout(location = 0) out vec4 outColor;             \n"
		"uniform sampler2D s_sampler;                       \n"
		"uniform float ctl_alpha;                              \n"
		"void main()                                         \n"
		"{                                                   \n"
		"  outColor = texture( s_sampler, v_texCoord );   \n"
#if MUTI_PROGRAM_ENABLE 
		"  outColor.a = outColor.a * ctl_alpha;             \n"
#endif
		"}                                                   \n";

#if !MUTI_PROGRAM_ENABLE
	// Load the shaders and get a linked program object
	userData->programObject = esLoadProgram(vShaderStr, fShaderStr);
	// Get the sampler location
	userData->samplerLoc = glGetUniformLocation(userData->programObject, "s_sampler");
#endif	

	GLint layer = LAYER_ID_0;
	for (; layer < LAYER_MAX; layer++) {
#if MUTI_PROGRAM_ENABLE
		// Load the shaders and get a linked program object
		userData->programObjects[layer] = esLoadProgram(vShaderStr, fShaderStr);

		// Get the sampler location
		userData->samplerLocs[layer] = glGetUniformLocation(userData->programObjects[layer], "s_sampler");
		userData->ctlAlphaLocs[layer] = glGetUniformLocation(userData->programObjects[layer], "ctl_alpha");
#endif

		GLint texIdx = 0;
		for (; texIdx < userData->textureNumPerLayer[layer]; texIdx++) {
			// Load the textures
			userData->textureIds[layer][texIdx] = loadTexture(s_images[layer][texIdx], &userData->texSize[layer][texIdx].width, &userData->texSize[layer][texIdx].height);
			if (userData->textureIds[layer][texIdx] == 0) {
				return FALSE;
			}

			userData->texVisable[layer][texIdx] = GL_TRUE;
			esLogMessage("Texture: %s size [%d, %d]\n", s_images[layer][texIdx], userData->texSize[layer][texIdx].width, userData->texSize[layer][texIdx].height);
		}

		setLayerAlpha(userData, layer, 1.0f);
	}

	/** Load indice data **/
	GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
	GLushort indicesSize = sizeof(indices);
	userData->indices = (GLushort*)malloc(indicesSize);
	memcpy(userData->indices, indices, indicesSize);
	userData->indiceNum = 6;

	/** Create VAOs **/
	createVAOs(userData);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	stRect hole = { 64, 64, 128, 128 };
	digHoleInTexture(userData, userData->textureIds[LAYER_ID_1][0], &hole, 0xff);
	return TRUE;
}

///
// Update MVP matrix based on time
//
void Update(ESContext *esContext, float deltaTime)
{
	static GLint x = 0;
	stUserData *pUserData = esContext->userData;
	stRect dispArea = { 0 };
	dispArea.left = x;
	dispArea.top = 100;
	dispArea.width = 200;
	dispArea.height = 200;
	setDispArea(pUserData, LAYER_ID_3, 0, &dispArea);
	stRect clipArea = { 0, 0, 640, 640 };
	if (x > (1280 - 200)) {
		clipArea.width = (1280 - x) * pUserData->texSize[LAYER_ID_3][0].width / 200;
	}
	setClipArea(pUserData, LAYER_ID_3, 0, &clipArea);
	updateVAO(pUserData, LAYER_ID_3, 0);
	x = (x++ > pUserData->winWidth) ? 0 : x;

	static GLint cnt = 1;
	if (cnt % 4 == 0) {
		static GLint delta = 1;
		dispArea.left = 200 - delta / 2;
		dispArea.top = 720 - delta;
		dispArea.width = delta;
		dispArea.height = delta;
		delta = (delta++ > 300) ? 1 : delta;
		setDispArea(pUserData, LAYER_ID_2, 3, &dispArea);
		updateVAO(pUserData, LAYER_ID_2, 3);
	}
	cnt++;

#if 0
	if (cnt % 200 < 100) {
#if 0  // method 1
		setLayerAlpha(userData, LAYER_ID_0, 1.0f);  // show
#else   // method 2 is more effective
		dispArea.left = 0;
		dispArea.top = 0;
		dispArea.width = userData->winWidth;
		dispArea.height = userData->winHeight;
		setDispArea(userData, LAYER_ID_0, 1, &dispArea);
		updateVAO(userData, LAYER_ID_0, 1);
#endif
	}
	else {
#if 0
		setLayerAlpha(userData, LAYER_ID_0, 0.0f);  // hide
#else
		memset(&dispArea, 0, sizeof(dispArea));
		setDispArea(userData, LAYER_ID_0, 1, &dispArea);
		updateVAO(userData, LAYER_ID_0, 1);
#endif
	}
#else
	static GLuint h = 0;
	clipArea.left = 0;
	clipArea.top = 0;
	clipArea.width = pUserData->texSize[LAYER_ID_0][1].width;
	h = (++h > pUserData->texSize[LAYER_ID_0][1].height) ? 1 : h;
	clipArea.height = h;
	setClipArea(pUserData, LAYER_ID_0, 1, &clipArea);
	dispArea.left = 0;
	dispArea.top = 0;
	dispArea.width = pUserData->winWidth;
	dispArea.height = h * pUserData->winHeight / pUserData->texSize[LAYER_ID_0][1].height;
	setDispArea(pUserData, LAYER_ID_0, 1, &dispArea);
	updateVAO(pUserData, LAYER_ID_0, 1);

#endif

	static GLubyte alpha = 0xff;
	stRect hole = { 64, 64, 128, 128 };
	digHoleInTexture(pUserData, pUserData->textureIds[LAYER_ID_1][0], &hole, alpha);
	alpha = alpha > 0 ? --alpha : 0xff;

	// rotate test
	static GLfloat angle = 0.0;
	setVertexData(pUserData, LAYER_ID_0, 2, ROTATE_AREA_CENTER, angle);

	stPos rotateCenter = { 0.0 };
	rotateCenter.x = pUserData->dispArea[LAYER_ID_1][2].left;
	rotateCenter.y = pUserData->dispArea[LAYER_ID_1][2].top + pUserData->dispArea[LAYER_ID_1][2].height / 2;
	updateVAO(pUserData, LAYER_ID_0, 2);

	setRotateArea(pUserData, LAYER_ID_1, 2, angle, &rotateCenter);
	//rotateTextureCoord(pUserData, LAYER_ID_1, 2, angle);
	updateVAO(pUserData, LAYER_ID_1, 2);

	angle += 0.5;
	angle = (angle > 360.0) ? 0.0 : angle;

	//rotateTextureCoord(pUserData, LAYER_ID_2, 1, 90);
	//updateVAO(pUserData, LAYER_ID_2, 1);

	// update FC_level image
	memset(&pUserData->texVisable[LAYER_ID_3][1], GL_FALSE, sizeof(GLboolean) * 5);
	static int tex_idx = 1;
	if (cnt % 8 == 0) {
		tex_idx++;
		tex_idx = (tex_idx > 5) ? 1 : tex_idx;
	}
	pUserData->texVisable[LAYER_ID_3][tex_idx] = GL_TRUE;
}


///
// Draw a triangle using the shader pair created in Init()
//
void Draw(ESContext *esContext)
{
	stUserData *userData = esContext->userData;

	// Set the viewport
	glViewport(0, 0, esContext->width, esContext->height);

	// Clear the color buffer
	glClear(GL_COLOR_BUFFER_BIT);

#if !MUTI_PROGRAM_ENABLE
	glUseProgram(userData->programObject);
	// Set the base map sampler to texture unit to 0
	glUniform1i(userData->samplerLoc, 0);
#endif

	GLint layer = LAYER_ID_0;
	for (; layer < LAYER_MAX; layer++) {
#if MUTI_PROGRAM_ENABLE
		glUseProgram(userData->programObjects[layer]);
		// Set the base map sampler to texture unit to 0
		glUniform1i(userData->samplerLocs[layer], 0);
#endif

		glActiveTexture(GL_TEXTURE0);

#if MUTI_PROGRAM_ENABLE
		glUniform1f(userData->ctlAlphaLocs[layer], userData->alphas[layer]);
#else
		if (userData->alphas[layer] == 0) continue; // this layer not show
#endif

		GLint texIdx = 0;
		for (; texIdx < userData->textureNumPerLayer[layer]; texIdx++) {
			if (userData->texVisable[layer][texIdx] == GL_FALSE) continue;
			// Bind the VAO
			glBindVertexArray(userData->vaoIds[layer][texIdx]);
			// Bind the base map
			glBindTexture(GL_TEXTURE_2D, userData->textureIds[layer][texIdx]);

			glDrawElements(GL_TRIANGLES, userData->indiceNum, GL_UNSIGNED_SHORT, (const void *)0);
		}
	}

	// Reset to the default VAO
	glBindVertexArray(0);

#if 0
	if (userData->dumpPixels) {
		GLint size = 1280 * 720 * 4 * sizeof(unsigned char);
		glReadPixels(0, 0, 1280, 720, GL_RGBA, GL_UNSIGNED_BYTE, userData->dumpPixels);
		FILE *pf = fopen("pixels.rgba", "wb");
		fwrite(userData->dumpPixels, size, 1, pf);
		fclose(pf);
	}
#endif
}

///
// Cleanup
//
void ShutDown(ESContext *esContext)
{
	stUserData *userData = esContext->userData;
#if !MUTI_PROGRAM_ENABLE
	glDeleteProgram(userData->programObject);
#endif

	GLint i = 0;
	for (i = 0; i < LAYER_MAX; i++) {
#if MUTI_PROGRAM_ENABLE
		// Delete program object
		glDeleteProgram(userData->programObjects[i]);
#endif

		// Delete texture object
		glDeleteTextures(userData->textureNumPerLayer[i], userData->textureIds[i]);
		GLint texIdx = 0;
		for (; texIdx < userData->textureNumPerLayer[i]; texIdx++) {
			if (userData->vertices[i][texIdx]) {
				free(userData->vertices[i][texIdx]);
				userData->vertices[i][texIdx] = NULL;
			}
		}
	}

	if (userData->indices) {
		free(userData->indices);
		userData->indices = NULL;
	}

	if (userData->dumpPixels) {
		free(userData->dumpPixels);
		userData->dumpPixels = NULL;
	}

	free(userData->holePixes);
	userData->holePixes = NULL;
}

int esMain(ESContext *esContext)
{
	esContext->userData = malloc(sizeof(stUserData));

	stUserData *pUserData = (stUserData*)esContext->userData;
	pUserData->winWidth = 1280;
	pUserData->winHeight = 720;

	esCreateWindow(esContext, "Blend Test", pUserData->winWidth, pUserData->winHeight, ES_WINDOW_RGB);

	if (!Init(esContext)) {
		return GL_FALSE;
	}

	esRegisterDrawFunc(esContext, Draw);
	esRegisterUpdateFunc(esContext, Update);
	esRegisterShutdownFunc(esContext, ShutDown);

	return GL_TRUE;
}

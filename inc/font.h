#pragma once

#include <std_truetype.h>

class UIFont
{
	friend class UIText;

public:

	UIFont();
	virtual ~UIFont();

	bool	loadTTF(const char* path, float fontSize, int bitmapWidth, int bitmapHeight);

	void	release();

private:

	static void initShaders();
	static unsigned int sm_fontCount;
	static unsigned int sm_program;

	stbtt_bakedchar	glyphs[96];
	unsigned int 	texture;
	int				bitmapWidth, bitmapHeight;
};

class UIText
{
public:

	UIText(UIFont* font, const char* text);
	UIText(UIFont* font, unsigned int maxLength);
	virtual ~UIText();

	void	setFont(UIFont* font);
	UIFont*	getFont() const;

	void				setPosition(const glm::vec2& position);
	const glm::vec2&	getPosition() const;

	void	set(const char* text);

	void	release();

private:

	UIFont*			m_font;
	glm::vec2		m_position;
	unsigned int 	m_length;
	unsigned int 	m_maxLength;
};


#ifndef _FONT_H_
#define _FONT_H_

#include "stb_truetype.h"

namespace OrangeEngine2
{
	namespace Font
	{
		namespace _Private
		{
			static GLuint Font_Shader = 0;

			const char* FontVertexShader = "#version 150\n"
            "in vec2 Position;\n"
			"in vec2 UV;\n"
			"in vec4 Colour;\n"
            "out vec2 vUV;\n"
			"out vec4 vColour;\n"
            "void main()\n"
            "{\n"
                "vUV = UV;\n"
				"vColour = Colour;\n"
				"gl_Position = vec4(Position.x, Position.y, 0, 1);\n"
            "}\n";
            
            const char* FontPixelShader = "#version 150\n"
            "in vec2 vUV;\n"
			"in vec4 vColour;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D diffuseTexture;\n"
            "void main()\n"
            "{\n"
                "vec4 tFrag = texture(diffuseTexture, vUV);\n"
				"FragColor = vec4(vColour.r, vColour.g, vColour.b, tFrag.r);\n"
            "}\n";
		};

		struct Vertex2D
		{
			glm::vec2 Position;
			glm::vec2 UV;
			glm::vec4 Colour;
		};

		struct Font
		{
			stbtt_bakedchar Glyphs[96]; // ASCII 32..126 is 95 glyphs (lets only get what we need)
			unsigned int FontTexture, PBO;
			unsigned short SizeOfBytesX, SizeOfBytesY;
		};

		struct Text
		{
			unsigned short QuadCount;
			unsigned int VAO, VBO;
			stbtt_aligned_quad Q;
			Vertex2D Vertex[6];
		};

		inline const void Init()
		{
			GLuint vsHandle = glCreateShader(GL_VERTEX_SHADER);
			GLuint fsHandle = glCreateShader(GL_FRAGMENT_SHADER);

			glShaderSource(vsHandle, 1, (const char**)&_Private::FontVertexShader, 0);
			glShaderSource(fsHandle, 1, (const char**)&_Private::FontPixelShader, 0);

			glCompileShader(vsHandle);
			glCompileShader(fsHandle);
            
			_Private::Font_Shader = glCreateProgram();

			glAttachShader(_Private::Font_Shader, vsHandle);
			glAttachShader(_Private::Font_Shader, fsHandle);

			//glBindAttribLocation(Shader, 0, "Position");
			//glBindAttribLocation(Shader, 1, "UV");
			//glBindAttribLocation(Shader, 2, "Colour");

			glLinkProgram(_Private::Font_Shader);

			glDetachShader(_Private::Font_Shader, vsHandle);
			glDetachShader(_Private::Font_Shader, fsHandle);
			
			glDeleteShader(vsHandle);
			glDeleteShader(fsHandle);
            
            int success;
            glGetProgramiv(_Private::Font_Shader, GL_LINK_STATUS, &success);
            if (success == GL_FALSE)
            {
                int infoLogLength = 0;
                glGetProgramiv(Shader, GL_INFO_LOG_LENGTH, &infoLogLength);
                char* infoLog = new char[infoLogLength];
                
                glGetProgramInfoLog(Shader, infoLogLength, 0, infoLog);
                printf("Error: Failed to link shader program!\n");
                printf("%s",infoLog);
                printf("\n");
                delete[] infoLog;
            }

			glUseProgram(0);
		}

		inline const void DrawText(Font& Font, Text& text)
		{
			glActiveTexture(GL_TEXTURE0);
			glUseProgram(_Private::Font_Shader);
			glBindVertexArray(text.VAO);
			glBindTexture(GL_TEXTURE_2D, Font.FontTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6 * text.QuadCount);

			//glBindVertexArray(0);
			//glBindTexture( GL_TEXTURE_2D, 0 );
			//glUseProgram(0);
		}

		inline const void Shutdown()
		{
			glDeleteProgram(_Private::Font_Shader);
		}

		inline const void LoadFont(Font& font, const char* FontLocation, const unsigned short& FontHeight)
		{
			unsigned char ttf_buffer[4096 * 8];

			font.SizeOfBytesX = FontHeight / 16 * 128;
			font.SizeOfBytesY = FontHeight / 16 * 128;

			if (FontHeight <= 16)
			{
				font.SizeOfBytesX = 128;
				font.SizeOfBytesY = 128;
			}

			glGenBuffers(1, &font.PBO);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, font.PBO);
			glBufferData(GL_PIXEL_UNPACK_BUFFER, font.SizeOfBytesX * font.SizeOfBytesY, NULL, GL_STREAM_COPY);
			unsigned char* temp_bitmap = (GLubyte*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, font.SizeOfBytesX * font.SizeOfBytesY, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            
			fread(ttf_buffer, 1, 4096 * 8, fopen(FontLocation, "rb"));
			stbtt_BakeFontBitmap(ttf_buffer, 0, FontHeight, temp_bitmap, font.SizeOfBytesX, font.SizeOfBytesY, 32, 96, font.Glyphs);

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

			glGenTextures(1, &font.FontTexture);
			glBindTexture(GL_TEXTURE_2D, font.FontTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			
			if (__glewTexStorage2D && __glewTextureStorage2DEXT)
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, font.SizeOfBytesX, font.SizeOfBytesY);
				//glGenerateMipmap();
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.SizeOfBytesX, font.SizeOfBytesY, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
			}
			
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, font.SizeOfBytesX, font.SizeOfBytesY, GL_RED, GL_UNSIGNED_BYTE, NULL);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}

		inline const void UnloadFont(Font& font)
		{
			glDeleteTextures(1, &font.FontTexture);
			glDeleteBuffers(1, &font.PBO);
		}

		inline const void LoadText(Text& text, Font& font, char* ctext, glm::vec2& Position, const glm::vec4& Colour)
		{
			text.QuadCount = 0;

			if (!text.VAO)
			{
				glGenBuffers(1, &text.VBO);
				glGenVertexArrays(1, &text.VAO);
			}

			glBindVertexArray(text.VAO);
			glBindBuffer(GL_ARRAY_BUFFER, text.VBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * 6 * strlen(ctext), NULL, GL_STATIC_DRAW);

			while (*ctext)
			{
				if (*ctext >= 32)
				{
					stbtt_GetBakedQuad(font.Glyphs, font.SizeOfBytesX, font.SizeOfBytesY, *ctext-32, &Position.x, &Position.y, &text.Q, 1); //1=opengl,0=old d3d
					
					text.Vertex[0].Position = glm::vec2(-1.0f + (text.Q.x0 / 1280) * 2.0f, 1.0f - (text.Q.y1 / 720) * 2.0f);
					text.Vertex[1].Position = glm::vec2(-1.0f + (text.Q.x1 / 1280) * 2.0f, 1.0f - (text.Q.y1 / 720) * 2.0f);
					text.Vertex[2].Position = glm::vec2(-1.0f + (text.Q.x0 / 1280) * 2.0f, 1.0f - (text.Q.y0 / 720) * 2.0f);
					text.Vertex[3].Position = glm::vec2(-1.0f + (text.Q.x1 / 1280) * 2.0f, 1.0f - (text.Q.y0 / 720) * 2.0f);
					text.Vertex[4].Position = glm::vec2(-1.0f + (text.Q.x0 / 1280) * 2.0f, 1.0f - (text.Q.y0 / 720) * 2.0f);
					text.Vertex[5].Position = glm::vec2(-1.0f + (text.Q.x1 / 1280) * 2.0f, 1.0f - (text.Q.y1 / 720) * 2.0f);
					
					text.Vertex[0].UV = glm::vec2(text.Q.s0, text.Q.t1);
					text.Vertex[1].UV = glm::vec2(text.Q.s1, text.Q.t1);
					text.Vertex[2].UV = glm::vec2(text.Q.s0, text.Q.t0);
					text.Vertex[3].UV = glm::vec2(text.Q.s1, text.Q.t0);
					text.Vertex[4].UV = glm::vec2(text.Q.s0, text.Q.t0);
					text.Vertex[5].UV = glm::vec2(text.Q.s1, text.Q.t1);
                   	
					text.Vertex[0].Colour = Colour;
					text.Vertex[1].Colour = Colour;
					text.Vertex[2].Colour = Colour;
					text.Vertex[3].Colour = Colour;
					text.Vertex[4].Colour = Colour;
					text.Vertex[5].Colour = Colour;

					glBufferSubData(GL_ARRAY_BUFFER, sizeof(Vertex2D) * 6 * text.QuadCount, sizeof(Vertex2D) * 6, &text.Vertex);
				}
				++ctext;
				text.QuadCount++;
			}

			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), 0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (char*)8);
			glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (char*)16);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		inline const void UnloadText(Text& text)
		{
			glDeleteBuffers(1, &text.VAO);
			glDeleteBuffers(1, &text.VBO);
		}
	};
};

#endif
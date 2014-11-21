
#define STB_TRUETYPE_IMPLEMENTATION

#include "font.h"
#include "gl_core_4_4.h"
#include "utilities.h"
#include <GLFW/glfw3.h>

unsigned int UIFont::sm_fontCount = 0;
unsigned int UIFont::sm_program = 0;

UIFont::UIFont()
	: m_texture(0),
	m_bitmapWidth(0),
	m_bitmapHeight(0)
{
	if (sm_fontCount++ == 0)
		initShader();
}

UIFont::UIFont(const char* path, float fontSize, int bitmapWidth, int bitmapHeight)
	: m_texture(0),
	m_bitmapWidth(0),
	m_bitmapHeight(0)
{
	if (sm_fontCount++ == 0)
		initShader();

	loadTTF(path, fontSize, bitmapWidth, bitmapHeight);
}

UIFont::~UIFont()
{
	release();

	if (sm_fontCount-- == 1)
	{
		glDeleteProgram(sm_program);
		sm_program = 0;
	}
}

void UIFont::release()
{
	if (m_texture != 0)
	{
		glDeleteTextures(1, &m_texture);
		m_texture = 0;
		m_bitmapWidth = 0;
		m_bitmapHeight = 0;
	}
}

void UIFont::initShader()
{
	const char* vsSource = "#version 330\n"
		"layout(location = 0) in vec2 Position;\n"
		"layout(location = 1) in vec2 UV;\n"
		"out vec2 vUV;\n"
		"uniform vec2 offset;"
            "void main()\n"
            "{\n"
                "vUV = UV;\n"
				"gl_Position = vec4(Position + offset, 0, 1);\n"
            "}\n";
            
	const char* fsSource = "#version 330\n"
		"in vec2 vUV;\n"
		"out vec4 FragColor;\n"
		"uniform vec4 colour;\n"
		"uniform sampler2D diffuseTexture;\n"
		"void main()\n"
		"{\n"
		"float alpha = texture(diffuseTexture, vUV).r * colour.a;\n"
		"if (alpha < 0.5f) discard;"
		"FragColor = colour;\n"
         "}\n";

    unsigned int vsHandle = glCreateShader(GL_VERTEX_SHADER);
	unsigned int fsHandle = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vsHandle, 1, &vsSource, 0);
	glShaderSource(fsHandle, 1, &fsSource, 0);

	glCompileShader(vsHandle);
	glCompileShader(fsHandle);
    
	sm_program = glCreateProgram();

	glAttachShader(sm_program, vsHandle);
	glAttachShader(sm_program, fsHandle);

	glLinkProgram(sm_program);

	glDetachShader(sm_program, vsHandle);
	glDetachShader(sm_program, fsHandle);
	
	glDeleteShader(vsHandle);
	glDeleteShader(fsHandle);
	glUseProgram(0);
}

bool UIFont::loadTTF(const char* path, float fontSize, int bitmapWidth, int bitmapHeight)
{
	if (m_texture != 0)
		release();

	size_t size = 0;
	unsigned char* ttf_buffer = nullptr;
	FILE* file = fopen(path, "rb");
	if (file != nullptr)
	{
		fseek(file, 0, SEEK_END);
		size_t s = ftell(file);
		fseek(file, 0, SEEK_SET);
		ttf_buffer = new unsigned char[s];
		fread(ttf_buffer, sizeof(unsigned char), s, file);
		fclose(file);
	}
	if (ttf_buffer == nullptr)
	{
		printf("Failed to load TTF: %s\n", path);
		return false;
	}

	unsigned char* tempBitmap = new unsigned char[bitmapWidth * bitmapHeight];
    
	stbtt_BakeFontBitmap(ttf_buffer, 0, fontSize,
		tempBitmap, bitmapWidth, bitmapHeight,
		32, 96, m_glyphs);

	delete[] ttf_buffer;

	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 
		bitmapWidth, bitmapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, tempBitmap);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	delete[] tempBitmap;

	m_bitmapWidth = bitmapWidth;
	m_bitmapHeight = bitmapHeight;

	return true;
}

UIText::UIText(UIFont* font, const char* text)
	: m_font(font),
	m_position(0, 0),
	m_colour(1),
	m_length(0),
	m_maxLength(strlen(text) + 1)
{
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	glGenBuffers(1, &m_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 6 * m_maxLength, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (char*)8);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	set(text);
}

glm::vec2 UIFont::getDimensions(const char* text)
{
	stbtt_aligned_quad Q;

	float x = 0, y = 0;
	glm::vec2 size(0);

	while (*text)
	{
		if (*text >= 32)
		{
			stbtt_GetBakedQuad(m_glyphs, m_bitmapWidth, m_bitmapHeight,*text - 32, &x, &y, &Q, 1);
			size.x = glm::max(glm::max(glm::abs(Q.x1), glm::abs(Q.x0)), size.x);
			size.y = glm::max(glm::max(glm::abs(Q.y1), glm::abs(Q.y0)), size.y);
		}
		++text;
	}
	return size;
}

UIText::UIText(UIFont* font, unsigned int maxLength)
: m_font(font),
m_position(0),
m_colour(1),
m_length(0),
m_maxLength(maxLength)
{
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	glGenBuffers(1, &m_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 6 * m_maxLength, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (char*)8);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

UIText::~UIText()
{
	release();
}

void UIText::release()
{
	if (m_vbo != 0)
	{
		glDeleteBuffers(1, &m_vbo);
		m_vbo = 0;
	}
	if (m_vao != 0)
	{
		glDeleteVertexArrays(1, &m_vao);
		m_vao = 0;
	}
	m_position = glm::vec2(0);
	m_font = 0;
	m_length = 0;
	m_maxLength = 0;
}

void UIText::setPosition(float x, float y)
{
	GLFWwindow* window = glfwGetCurrentContext();
	int w = 0, h = 0;
	glfwGetWindowSize(window, &w, &h);

	m_position = glm::vec2(x,y) / glm::vec2(w, h) * 2.0f;
}

void UIText::setPosition(const glm::vec2& position) 
{
	GLFWwindow* window = glfwGetCurrentContext();
	int w = 0, h = 0;
	glfwGetWindowSize(window, &w, &h);

	m_position = position / glm::vec2(w,h) * 2.0f; 
}

void UIText::set(const char* text)
{
	m_length = 0;

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

	Vertex quadVerts[6];
	stbtt_aligned_quad Q;

	float x = 0, y = 0;

	GLFWwindow* window = glfwGetCurrentContext();
	int w = 0, h = 0;
	glfwGetWindowSize(window, &w, &h);

	while (*text &&
		m_length < m_maxLength)
	{
		if (*text >= 32)
		{
			stbtt_GetBakedQuad(m_font->m_glyphs, m_font->m_bitmapWidth, m_font->m_bitmapHeight,
				*text - 32, &x, &y, &Q, 1);

			quadVerts[0].position = glm::vec2(-1.0f + (Q.x0 / w) * 2.0f, -1.0f - (Q.y1 / h) * 2.0f);
			quadVerts[1].position = glm::vec2(-1.0f + (Q.x1 / w) * 2.0f, -1.0f - (Q.y1 / h) * 2.0f);
			quadVerts[2].position = glm::vec2(-1.0f + (Q.x0 / w) * 2.0f, -1.0f - (Q.y0 / h) * 2.0f);
			quadVerts[3].position = glm::vec2(-1.0f + (Q.x1 / w) * 2.0f, -1.0f - (Q.y0 / h) * 2.0f);
			quadVerts[4].position = glm::vec2(-1.0f + (Q.x0 / w) * 2.0f, -1.0f - (Q.y0 / h) * 2.0f);
			quadVerts[5].position = glm::vec2(-1.0f + (Q.x1 / w) * 2.0f, -1.0f - (Q.y1 / h) * 2.0f);

			quadVerts[0].texCoord = glm::vec2(Q.s0, Q.t1);
			quadVerts[1].texCoord = glm::vec2(Q.s1, Q.t1);
			quadVerts[2].texCoord = glm::vec2(Q.s0, Q.t0);
			quadVerts[3].texCoord = glm::vec2(Q.s1, Q.t0);
			quadVerts[4].texCoord = glm::vec2(Q.s0, Q.t0);
			quadVerts[5].texCoord = glm::vec2(Q.s1, Q.t1);

			glBufferSubData(GL_ARRAY_BUFFER, sizeof(Vertex) * 6 * m_length, sizeof(Vertex) * 6, &quadVerts);
		}
		++text;
		++m_length;
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void UIText::draw()
{
	glActiveTexture(GL_TEXTURE0);
	glUseProgram(UIFont::sm_program);
	glUniform2fv(glGetUniformLocation(UIFont::sm_program,"offset"), 1, &m_position[0]);
	glUniform4fv(glGetUniformLocation(UIFont::sm_program, "colour"), 1, &m_colour[0]);
	glBindVertexArray(m_vao);
	glBindTexture(GL_TEXTURE_2D, m_font->m_texture);
	glDrawArrays(GL_TRIANGLES, 0, 6 * m_length);
}
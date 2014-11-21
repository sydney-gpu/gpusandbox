#pragma once

#include <stb_truetype.h>
#include <glm/glm.hpp>

class UIFont
{
	friend class UIText;

public:

	UIFont();
	UIFont(const char* path, float fontSize, int bitmapWidth, int bitmapHeight);
	virtual ~UIFont();

	bool	loadTTF(const char* path, float fontSize, int bitmapWidth, int bitmapHeight);

	glm::vec2	getDimensions(const char* text);

	void	release();

private:

	static void initShader();
	static unsigned int sm_fontCount;
	static unsigned int sm_program;

	stbtt_bakedchar	m_glyphs[96];
	unsigned int 	m_texture;
	int				m_bitmapWidth, m_bitmapHeight;
};

class UIText
{
public:

	UIText(UIFont* font, const char* text);
	UIText(UIFont* font, unsigned int maxLength);
	virtual ~UIText();

	void	setFont(UIFont* font) { m_font = font; }
	UIFont*	getFont() const { return m_font; }

	void				setPosition(float x, float y);
	void				setPosition(const glm::vec2& position);
	const glm::vec2&	getPosition() const	{ return m_position; }

	void				setColour(float r, float g, float b) { m_colour = glm::vec4(r, g, b, m_colour.a); }
	void				setColour(float r, float g, float b, float a) { m_colour = glm::vec4(r, g, b, a); }
	void				setColour(const glm::vec4& colour) { m_colour = colour; }
	const glm::vec4&	getColour() const	{ return m_colour; }

	void				setAlpha(float a) { m_colour.a = a; }
	float				getAlpha() const { return m_colour.a; }

	void	set(const char* text);

	void	release();

	void	draw();

private:

	struct Vertex
	{
		glm::vec2 position;
		glm::vec2 texCoord;
	};

	UIFont*			m_font;
	glm::vec2		m_position;
	glm::vec4		m_colour;
	unsigned int 	m_length;
	unsigned int 	m_maxLength;
	unsigned int	m_vao, m_vbo;
};
#include "font.h"
#include "gl_core_4_4.h"

unsigned int UIFont::sm_fontCount = 0;
unsigned int UIFont::sm_program = 0;

UIFont::UIFont(const char* path)
{
	if (sm_fontCount++ == 0)
		initShader();

	if (path != nullptr)
		loadTTF(path);
}

UIFont::~UIFont()
{
	if (sm_fontCount-- == 1)
	{
		glDeletePogram(sm_program);
		sm_program = 0;
	}
}

void UIFont::initShader()
{
	const char* vsSource = "#version 150\n"
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
            
    const char* fsSource = "#version 150\n"
            "in vec2 vUV;\n"
			"in vec4 vColour;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D diffuseTexture;\n"
            "void main()\n"
            "{\n"
                "vec4 tFrag = texture(diffuseTexture, vUV);\n"
				"FragColor = vec4(vColour.r, vColour.g, vColour.b, tFrag.r);\n"
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

bool UIFont::loadTTF(const char* path, unsigned int size)
{
	SizeOfBytesX = FontHeight / 16 * 128;
	SizeOfBytesY = FontHeight / 16 * 128;

	if (FontHeight <= 16)
	{
		SizeOfBytesX = 128;
		SizeOfBytesY = 128;
	}

	glGenBuffers(1, &PBO);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, SizeOfBytesX * SizeOfBytesY, NULL, GL_STREAM_COPY);

	unsigned char* temp_bitmap = (unsigned char*)glMapBufferRange(
		GL_PIXEL_UNPACK_BUFFER, 0, SizeOfBytesX * SizeOfBytesY, 
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	size_t size = 0;
	char* ttf_buffer = readFileContents(path, &size);
    
	stbtt_BakeFontBitmap(ttf_buffer, 0, FontHeight, 
		temp_bitmap, SizeOfBytesX, SizeOfBytesY, 
		32, 96, font.Glyphs);

	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	glGenTextures(1, &FontTexture);
	glBindTexture(GL_TEXTURE_2D, FontTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	/*if (__glewTexStorage2D && __glewTextureStorage2DEXT)
	{
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, font.SizeOfBytesX, font.SizeOfBytesY);
		//glGenerateMipmap();
	}
	else
	{*/
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SizeOfBytesX, SizeOfBytesY, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	//}
	
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SizeOfBytesX, SizeOfBytesY, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	delete[] ttf_buffer;
}

UIText::UIText(unsigned int maxLength, UIFont* font)
: m_font(font),
m_length(0),
m_maxLength(maxLength)
{

}
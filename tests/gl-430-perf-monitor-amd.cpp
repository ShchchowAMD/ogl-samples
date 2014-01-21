//**********************************
// OpenGL Performance Monitor
// 20/01/2014 - 20/01/2014
//**********************************
// Christophe Riccio
// ogl-samples@g-truc.net
//**********************************
// G-Truc Creation
// www.g-truc.net
//**********************************

#include <glf/glf.hpp>
#include <glf/compiler.hpp>

namespace
{
	char const * VERT_SHADER_SOURCE_TEXTURE("gl-430/fbo-texture-2d.vert");
	char const * FRAG_SHADER_SOURCE_TEXTURE("gl-430/fbo-texture-2d.frag");
	char const * VERT_SHADER_SOURCE_SPLASH("gl-430/fbo-splash.vert");
	char const * FRAG_SHADER_SOURCE_SPLASH("gl-430/fbo-splash.frag");
	char const * TEXTURE_DIFFUSE("kueken1-dxt1.dds");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);

	glf::window Window("gl-430-perf-monitor-amd", glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));
	//glf::window Window(glm::ivec2(256, 256));

	GLsizei const VertexCount(4);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f))
	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	namespace buffer
	{
		enum type
		{
			VERTEX,
			ELEMENT,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			COLORBUFFER,
			RENDERBUFFER,
			MAX
		};
	}//namespace texture
	
	namespace pipeline
	{
		enum type
		{
			TEXTURE,
			SPLASH,
			MAX
		};
	}//namespace pipeline

	GLuint FramebufferName(0);
	std::vector<GLuint> ProgramName(pipeline::MAX);
	std::vector<GLuint> VertexArrayName(pipeline::MAX);
	std::vector<GLuint> BufferName(buffer::MAX);
	std::vector<GLuint> TextureName(texture::MAX);
	std::vector<GLuint> PipelineName(pipeline::MAX);

	class monitor
	{
		struct group
		{
			group
			(
				std::string const & Name,
				std::vector<GLuint>	const Counters
			) :
				Name(Name),
				Counters(Counters)
			{}

			std::string const Name;
			std::vector<GLuint>	const Counters;
		};

	public:
		monitor() :
			Name(0)
		{
			GLsizei const GROUP_NAME_SIZE(256);

			GLint GroupSize(0);
			glGetPerfMonitorGroupsAMD(&GroupSize, 0, NULL);

			std::vector<GLuint> Groups(static_cast<std::size_t>(GroupSize));
			glGetPerfMonitorGroupsAMD(NULL, GroupSize, &Groups[0]);

			for (std::size_t GroupIndex(0); GroupIndex < Groups.size(); ++GroupIndex)
			{
				char GroupName[GROUP_NAME_SIZE];
				memset(GroupName, 0, sizeof(GroupName));
				glGetPerfMonitorGroupStringAMD(Groups[GroupIndex], GROUP_NAME_SIZE, NULL, GroupName);

				GLint NumCounters(0);
				GLint MaxActiveCounters(0);
				glGetPerfMonitorCountersAMD(Groups[GroupIndex], &NumCounters, &MaxActiveCounters, 0, NULL);

				std::vector<GLuint>	Counters(static_cast<std::size_t>(NumCounters));
				glGetPerfMonitorCountersAMD(Groups[GroupIndex], NULL, NULL, NumCounters, &Counters[0]);

				for (std::size_t CounterIndex(0); CounterIndex < Counters.size(); ++CounterIndex)
				{
					char CounterName[GROUP_NAME_SIZE];
					memset(CounterName, 0, sizeof(CounterName));

					glGetPerfMonitorCounterStringAMD(Groups[GroupIndex], Counters[CounterIndex], GROUP_NAME_SIZE, NULL, CounterName);
				}

				std::string GroupString;
				for (std::size_t i = 0; i < sizeof(GroupName); ++i)
				{
					if(GroupName[i] == '\0')
						break;
					else
						GroupString += GroupName[i];
				}

				this->Groups.insert(std::pair<GLuint, group>(Groups[GroupIndex], group(GroupString, Counters)));
				this->StringToGroup.insert(std::pair<std::string, GLuint>(GroupString, Groups[GroupIndex]));
			}

			glGenPerfMonitorsAMD(1, &this->Name);
		}

		~monitor()
		{
			glDeletePerfMonitorsAMD(1, &this->Name);
		}

		void begin()
		{
			glBeginPerfMonitorAMD(this->Name);
		}

		void end()
		{
			glEndPerfMonitorAMD(this->Name);
		}

		void record(std::string const & GroupString, std::size_t CounterCount)
		{
			std::map<std::string, GLuint>::iterator NameIt = this->StringToGroup.find(GroupString);
			assert(NameIt != this->StringToGroup.end());

			std::map<GLuint, group>::iterator GroupIt = this->Groups.find(NameIt->second);
			assert(GroupIt != this->Groups.end());

			//glSelectPerfMonitorCountersAMD(this->Name, GL_TRUE, GroupIt->first, static_cast<GLint>(GroupIt->second.Counters.size()), const_cast<GLuint*>(&GroupIt->second.Counters[0]));
			glSelectPerfMonitorCountersAMD(this->Name, GL_TRUE, GroupIt->first, static_cast<GLint>(glm::min(CounterCount, GroupIt->second.Counters.size())), const_cast<GLuint*>(&GroupIt->second.Counters[0]));
		}

		void log()
		{
			// read the counters
			GLuint resultSize;
			glGetPerfMonitorCounterDataAMD(this->Name, GL_PERFMON_RESULT_SIZE_AMD, sizeof(GLint), &resultSize, NULL);

			std::vector<GLuint> Result;
			Result.resize(resultSize);

			GLsizei resultWritten;
			glGetPerfMonitorCounterDataAMD(this->Name, GL_PERFMON_RESULT_AMD, resultSize, &Result[0], &resultWritten);

			GLsizei wordCount = 0;

			while ((4 * wordCount) < resultWritten)
			{
				GLuint GroupId = Result[wordCount];
				GLuint CounterId = Result[wordCount + 1];

				std::map<GLuint, group>::iterator GroupIt = this->Groups.find(GroupId);
				assert(GroupIt != this->Groups.end());

				// Determine the counter type
				GLuint CounterType;
				glGetPerfMonitorCounterInfoAMD(GroupId, CounterId, GL_COUNTER_TYPE_AMD, &CounterType);

				switch(CounterType)
				{
					case GL_UNSIGNED_INT64_AMD:
					{
						unsigned __int64 counterResult = *reinterpret_cast<unsigned __int64*>(&Result[wordCount + 2]);
						printf("%s(%d): %ld\n", GroupIt->second.Name.c_str(), CounterId, counterResult);
						wordCount += 4;
						break;
					}
					case GL_FLOAT:
					{
						float counterResult = *reinterpret_cast<float*>(&Result[wordCount + 2]);
						printf("%s(%d): %f\n", GroupIt->second.Name.c_str(), CounterId, counterResult);
						wordCount += 3;
						break;
					}
					case GL_UNSIGNED_INT:
					{
						unsigned int counterResult = *reinterpret_cast<unsigned int*>(&Result[wordCount + 2]);
						printf("%s(%d): %d\n", GroupIt->second.Name.c_str(), CounterId, counterResult);
						wordCount += 3;
						break;
					}
					case GL_PERCENTAGE_AMD:
					{
						unsigned int counterResult = *reinterpret_cast<unsigned int*>(&Result[wordCount + 2]);
						printf("%s(%d): %f\n", GroupIt->second.Name.c_str(), CounterId, counterResult);
						wordCount += 3;
						break;
					}
					default:
						assert(0);
				}
			}
		}

	private:
		GLuint Name;
		std::map<std::string , GLuint> StringToGroup;
		std::map<GLuint, group> Groups;
	};

	monitor* Monitor(0);
}//namespace

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(pipeline::MAX, &PipelineName[0]);

	if(Validated)
	{
		glf::compiler Compiler;
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_TEXTURE, "--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_TEXTURE, "--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[pipeline::TEXTURE] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::TEXTURE], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::TEXTURE], VertShaderName);
		glAttachShader(ProgramName[pipeline::TEXTURE], FragShaderName);
		glLinkProgram(ProgramName[pipeline::TEXTURE]);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		Validated = Validated && glf::checkProgram(ProgramName[pipeline::TEXTURE]);
	}

	if(Validated)
		glUseProgramStages(PipelineName[pipeline::TEXTURE], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[pipeline::TEXTURE]);

	if(Validated)
	{
		glf::compiler Compiler;
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_SPLASH, "--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_SPLASH, "--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[pipeline::SPLASH] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::SPLASH], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::SPLASH], VertShaderName);
		glAttachShader(ProgramName[pipeline::SPLASH], FragShaderName);
		glLinkProgram(ProgramName[pipeline::SPLASH]);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		Validated = Validated && glf::checkProgram(ProgramName[pipeline::SPLASH]);
	}

	if(Validated)
		glUseProgramStages(PipelineName[pipeline::SPLASH], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[pipeline::SPLASH]);

	return Validated;
}

bool initBuffer()
{
	glGenBuffers(buffer::MAX, &BufferName[0]);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint UniformBufferOffset(0);
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &UniformBufferOffset);

	GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return true;
}

bool initTexture()
{
	bool Validated(true);

	gli::texture2D Texture(gli::load_dds((glf::DATA_DIRECTORY + TEXTURE_DIFFUSE).c_str()));
	assert(!Texture.empty());

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(texture::MAX, &TextureName[0]);

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glTexStorage2D(GL_TEXTURE_2D, GLint(Texture.levels()), GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GLsizei(Texture.dimensions().x), GLsizei(Texture.dimensions().y));

	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glCompressedTexSubImage2D(
			GL_TEXTURE_2D,
			GLint(Level),
			0, 0,
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 
			GLsizei(Texture[Level].size()), 
			Texture[Level].data());
	}
	
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexStorage2D(GL_TEXTURE_2D, GLint(1), GL_RGBA8, GLsizei(Window.Size.x), GLsizei(Window.Size.y));

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::RENDERBUFFER]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexStorage2D(GL_TEXTURE_2D, GLint(1), GL_DEPTH_COMPONENT24, GLsizei(Window.Size.x), GLsizei(Window.Size.y));

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	return Validated;
}

bool initVertexArray()
{
	glGenVertexArrays(pipeline::MAX, &VertexArrayName[0]);
	glBindVertexArray(VertexArrayName[pipeline::TEXTURE]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[pipeline::SPLASH]);
	glBindVertexArray(0);

	return true;
}

bool initFramebuffer()
{
	bool Validated(true);

	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::COLORBUFFER], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, TextureName[texture::RENDERBUFFER], 0);

	if(glf::checkFramebuffer(FramebufferName))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

bool begin()
{
	bool Validated(true);
	Validated = Validated && glf::checkExtension("GL_AMD_performance_monitor");

	if(Validated)
	{
		::Monitor = new monitor();
		::Monitor->record("CP", 1);
	}

	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();

	return Validated;
}

bool end()
{
	bool Validated(true);

	delete Monitor;
	Monitor = 0;

	glDeleteProgramPipelines(pipeline::MAX, &PipelineName[0]);
	glDeleteProgram(ProgramName[pipeline::SPLASH]);
	glDeleteProgram(ProgramName[pipeline::TEXTURE]);
	glDeleteBuffers(buffer::MAX, &BufferName[0]);
	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteTextures(texture::MAX, &TextureName[0]);
	glDeleteVertexArrays(pipeline::MAX, &VertexArrayName[0]);

	return Validated;
}

void display()
{
	::Monitor->begin();

	// Update of the uniform buffer
	{
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
			GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		//glm::mat4 Projection = glm::perspectiveFov(glm::pi<float>() * 0.25f, 640.f, 480.f, 0.1f, 100.0f);
		glm::mat4 Projection = glm::perspective(glm::pi<float>() * 0.25f, 4.0f / 3.0f, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);
		
		*Pointer = Projection * View * Model;

		// Make sure the uniform buffer is uploaded
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glViewportIndexedf(0, 0, 0, GLfloat(Window.Size.x), GLfloat(Window.Size.y));

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH , 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	// Bind rendering objects
	glBindProgramPipeline(PipelineName[pipeline::TEXTURE]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glBindVertexArray(VertexArrayName[pipeline::TEXTURE]);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);

	glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, 0, 2, 0, 0);

	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindProgramPipeline(PipelineName[pipeline::SPLASH]);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(VertexArrayName[pipeline::SPLASH]);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);

	glDrawArraysInstanced(GL_TRIANGLES, 0, 3, 1);

	::Monitor->end();
	::Monitor->log();
}

int main(int argc, char* argv[])
{
	return glf::run(argc, argv, glf::CORE, 4, 3);
}
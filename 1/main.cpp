#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <functional>
#include <initializer_list>
#include <GL/glew.h>
#include <GL/freeglut.h>

typedef std::function<void()> proc;
typedef std::function<void(float)> procf;

const float 
	pi = acosf(-1),
	SX0 = 0,
	SY0 = 0,
	SZ0 = 0,
	OX0 = 0,
	OY0 = 0,
	OZ0 = 300,
	zN = 0,
	zF = 500,
	scale = 3,
	SPIN_SPEED = pi/180,
	OFFSET_SPEED = 0.1;

std::vector< std::pair<proc,int> > keyMap[256];

struct Vec4
{
	float val[4];
	Vec4() { memset(val, 0, sizeof val); }
	Vec4(std::initializer_list<float> _val)
	{
		auto iter = _val.begin();
		for (int i=0; i<4; i++)
			val[i] = *iter++;
	}
	void setTo(const char*) const;
};

const int lightCnt = 2;
const Vec4 lightDir[2] = { { -0.5, 0.5, 0.5, 0.0 }, { 0.5, 0.5, 0.5, 0.0 } };
const Vec4 lightCol[2] = { { 0.8, 0.4, 0.4, 1.0 }, { 0.4, 0.8, 0.4, 1.0 } };

struct Mat4
{
	float val[4][4];
	Mat4() { memset(val, 0, sizeof val); }
	Mat4(std::initializer_list<float> _val)
	{
		auto iter = _val.begin();
		for (int i=0; i<4; i++)
			for (int j=0; j<4; j++)
				val[i][j] = *iter++;
	}
	void setTo(const char *) const;
};

inline Mat4 operator*(const Mat4 &a, const Mat4 &b)
{
	Mat4 ret;
	for (int i=0; i<4; i++)
		for (int j=0; j<4; j++)
			for (int k=0; k<4; k++)
				ret.val[i][j] += a.val[i][k] * b.val[k][j];
	return ret;
}

struct Triangle
{
	Vec4 norm, vert[3];
	unsigned short msg;
};

struct Shader
{
	GLuint id;

	Shader(GLenum type, const char filename[])
	{
		std::ifstream fs(filename, std::ios::in);
		std::ostringstream oss;
		oss << fs.rdbuf();
		fs.close();
		std::string content(oss.str());
		id = glCreateShader(type);
		const char *str = content.c_str();
		glShaderSource(id, 1, &str, 0);
		glCompileShader(id);
		GLint status;
		glGetShaderiv(id, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint infoLogLength;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
			GLchar *strInfoLog = new GLchar[infoLogLength+1];
			glGetShaderInfoLog(id, infoLogLength, 0, strInfoLog);
			std::string errMsg(
				std::string("Compile failure in ") +
				(type == GL_VERTEX_SHADER ? "vertex" : type == GL_GEOMETRY_SHADER ? "geometry" : "fragment") +
				"shader: " +
				strInfoLog
			);
			delete[] strInfoLog;
			throw std::runtime_error(errMsg);
		}
	}
	
	~Shader()
	{
		glDeleteShader(id);
	}

	Shader(Shader &&other) : id(other.id) { other.id = 0; }
};

struct Program
{
	GLuint id;

	Program(const std::vector<Shader> &sha)
	{
		id = glCreateProgram();
		for (const auto &item : sha)
			glAttachShader(id, item.id);
		glLinkProgram(id);
		GLint status;
		glGetProgramiv(id, GL_LINK_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint infoLogLength;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
			GLchar *strInfoLog = new GLchar[infoLogLength+1];
			glGetShaderInfoLog(id, infoLogLength, 0, strInfoLog);
			std::string errMsg(std::string("Link failuer: ") + strInfoLog);
			delete strInfoLog;
			throw std::runtime_error(errMsg);
		}
	}
	
	~Program()
	{
		glDeleteProgram(id);
	}

	Program(Program &&other) : id(other.id) { other.id = 0; }
} *prog;

void Vec4::setTo(const char name[]) const
{
	GLuint id = glGetUniformLocation(prog->id, name);
	glUniform4fv(id, 1, val);
}

void Mat4::setTo(const char name[]) const
{
	GLuint id = glGetUniformLocation(prog->id, name);
	glUniformMatrix4fv(id, 1, GL_FALSE, (GLfloat*)val);
}

void setUniformInt(int x, const char name[])
{
	GLuint id = glGetUniformLocation(prog->id, name);
	glUniform1i(id, x);
}

class ParamData
{
	float val, step;
	unsigned char up_key, down_key;
	const int up_id, down_id;
	static int func_cnt;

public:
	procf trigger;

	ParamData(float _val, float _step, procf _trigger) :
		val(_val), step(_step), up_key(0), down_key(0), up_id(func_cnt++), down_id(func_cnt++), trigger(_trigger) {}
	
	void unbindUp()
	{
		if (up_key)
			for (auto i=keyMap[up_key].begin(); i!=keyMap[up_key].end(); i++)
				if (i->second == up_id)
				{
					keyMap[up_key].erase(i);
					return;
				}
	}
	
	void unbindDown()
	{
		if (down_key)
			for (auto i=keyMap[down_key].begin(); i!=keyMap[down_key].end(); i++)
				if (i->second == down_id)
				{
					keyMap[down_key].erase(i);
					return;
				}
	}
	
	void bindUp(unsigned char key)
	{
		unbindUp();
		keyMap[up_key = key].push_back(std::pair<proc,int>([&]() { trigger(val+=step); }, up_id));
	}
	
	void bindDown(unsigned char key)
	{
		unbindDown();
		keyMap[down_key = key].push_back(std::pair<proc,int>([&]() { trigger(val-=step); }, down_id));
	}
};
int ParamData::func_cnt = 0;

class Param
{
public:
	ParamData *data;
	ParamData *operator->() { return data; }
	const ParamData *operator->() const { return data; }

protected:
	Param(float _val, float _step) : data(new ParamData(_val, _step, 0)) {}
	Param(Param &&other) : data(other.data) { other.data = 0; }
	~Param()
	{
		if (data)
		{
			data->unbindUp();
			data->unbindDown();
		}
	}
};

class SpinX : public Param
{
public:
	Mat4 *matrix;

	SpinX(float dgr) : Param(dgr, SPIN_SPEED), matrix(new Mat4())
	{
		auto mat = matrix;
		(
			data->trigger = [=](float dgr)
			{
				std::clog << "SpinX set to " << dgr << std::endl;
				*mat = {
					1,			0,			0,			0,
					0,			cosf(dgr),	-sinf(dgr),	0,
					0,			sinf(dgr),	cosf(dgr),	0,
					0,			0,			0,			1
				};
			}
		)(dgr);
	}
	SpinX(SpinX &&other) : Param(std::move(other)), matrix(other.matrix) { other.matrix=0; }
	~SpinX() { if (matrix) delete matrix; }
};

class SpinY : public Param
{
public:
	Mat4 *matrix;

	SpinY(float dgr) : Param(dgr, SPIN_SPEED), matrix(new Mat4())
	{
		auto mat = matrix;
		(
			data->trigger = [=](float dgr)
			{
				std::clog << "SpinY set to " << dgr << std::endl;
				*mat = {
					cosf(dgr),	0,			-sinf(dgr),	0,
					0,			1,			0,			0,
					sinf(dgr),	0,			cosf(dgr),	0,
					0,			0,			0,			1
				};
			}
		)(dgr);
	}
	SpinY(SpinY &&other) : Param(std::move(other)), matrix(other.matrix) { other.matrix=0; }
	~SpinY() { if (matrix) delete matrix; }
};

class SpinZ : public Param
{
public:
	Mat4 *matrix;

	SpinZ(float dgr) : Param(dgr, SPIN_SPEED), matrix(new Mat4())
	{
		auto mat = matrix;
		(
			data->trigger = [=](float dgr)
			{
				std::clog << "SpinZ set to " << dgr << std::endl;
				*mat = {
					cosf(dgr),	-sinf(dgr),	0,			0,
					sinf(dgr),	cosf(dgr),	0,			0,
					0,			0,			1,			0,
					0,			0,			0,			1
				};
			}
		)(dgr);
	}
	SpinZ(SpinZ &&other) : Param(std::move(other)), matrix(other.matrix) { other.matrix=0; }
	~SpinZ() { if (matrix) delete matrix; }
};

class OffsetX : public Param
{
public:
	Mat4 *matrix;

	OffsetX(float x) : Param(x, OFFSET_SPEED), matrix(new Mat4())
	{
		auto mat = matrix;
		(
			data->trigger = [=](float x)
			{
				std::clog << "OffsetX set to " << x << std::endl;
				*mat = {
					1,	0,	0,	x,
					0,	1,	0,	0,
					0,	0,	1,	0,
					0,	0,	0,	1
				};
			}
		)(x);
	}
	OffsetX(OffsetX &&other) : Param(std::move(other)), matrix(other.matrix) { other.matrix=0; }
	~OffsetX() { if (matrix) delete matrix; }
};

class OffsetY : public Param
{
public:
	Mat4 *matrix;

	OffsetY(float y) : Param(y, OFFSET_SPEED), matrix(new Mat4())
	{
		auto mat = matrix;
		(
			data->trigger = [=](float y)
			{
				std::clog << "OffsetY set to " << y << std::endl;
				*mat = {
					1,	0,	0,	0,
					0,	1,	0,	y,
					0,	0,	1,	0,
					0,	0,	0,	1
				};
			}
		)(y);
	}
	OffsetY(OffsetY &&other) : Param(std::move(other)), matrix(other.matrix) { other.matrix=0; }
	~OffsetY() { if (matrix) delete matrix; }
};

class OffsetZ : public Param
{
public:
	Mat4 *matrix;

	OffsetZ(float z) : Param(z, OFFSET_SPEED), matrix(new Mat4())
	{
		auto mat = matrix;
		(
			data->trigger = [=](float z)
			{
				std::clog << "OffsetZ set to " << z << std::endl;
				*mat = {
					1,	0,	0,	0,
					0,	1,	0,	0,
					0,	0,	1,	z,
					0,	0,	0,	1
				};
			}
		)(z);
	}
	OffsetZ(OffsetZ &&other) : Param(std::move(other)), matrix(other.matrix) { other.matrix=0; }
	~OffsetZ() { if (matrix) delete matrix; }
};

class Item // DO NOT COPY THE OBJECTS.
{
	int n;
	char *name;
	std::vector<Triangle> tri;
	GLuint vao, vbo; // vao saves data settings and vbo saves data.
	
public:
	SpinX sx;
	SpinY sy;
	SpinZ sz;
	OffsetX ox;
	OffsetY oy;
	OffsetZ oz;

	Item() : tri(), sx(SX0), sy(SY0), sz(SZ0), ox(OX0), oy(OY0), oz(OZ0)
	{
		name = new char[81];
		memset(name, 0, sizeof(char)*81);
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}
	
	Item(Item &&other) :
		n(other.n), name(other.name), tri(std::move(other.tri)),
		vao(other.vao), vbo(other.vbo),
		sx(std::move(other.sx)), sy(std::move(other.sy)), sz(std::move(other.sz)),
		ox(std::move(other.ox)), oy(std::move(other.oy)), oz(std::move(other.oz))
	{
		other.name = 0;
		other.vao = 0;
		other.vbo = 0;
	}

	~Item()
	{
		delete[] name;
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
	
	void load(const char filename[])
	{
		FILE *f = fopen(filename, "rb");
		fread(name, sizeof(char), 80, f);
		fread(&n, sizeof(int), 1, f);
		for (int i=0; i<n; i++)
		{
			Triangle cur;
			fread(cur.norm.val, sizeof(float), 3, f);
			cur.norm.val[3] = 0; // norm=(a-b), (a-b).w=0
			for (int j=0; j<3; j++)
			{
				fread(cur.vert[j].val, sizeof(float), 3, f);
				cur.vert[j].val[3] = 1;
			}
			fread(&cur.msg, sizeof(short), 1, f);
			tri.push_back(cur);
		}
		fclose(f);

		float *buffer = new float[n*3*4*2];
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		float *vst = buffer, *cst = buffer + n*12;
		for (const auto &one : tri)
		{
			memcpy(vst, one.vert, sizeof(one.vert)), vst += 12;
			memcpy(cst, &one.norm, sizeof(one.norm)), cst += 4;
			memcpy(cst, &one.norm, sizeof(one.norm)), cst += 4;
			memcpy(cst, &one.norm, sizeof(one.norm)), cst += 4;
		}
		glBufferData(GL_ARRAY_BUFFER, n*3*4*2*sizeof(float), buffer, GL_STATIC_DRAW);
		delete[] buffer;
		buffer = vst = cst = 0;
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)(n*3*4*sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	
	void draw() const
	{
		glBindVertexArray(vao);
		(
		 Mat4({
			 scale,	0,		0,				0,						// zN -> 0,  zF -> zF+1
			 0,		scale,	0,				0,						// z = (z-zN)*(zF+1)/(zF-zN)
			 0,		0,		(zF+1)/(zF-zN),	-zN*(zF+1)/(zF-zN),
			 0,		0,		1,				1
			 })
		 * *ox.matrix * *oy.matrix * *oz.matrix
		 * *sx.matrix * *sy.matrix * *sz.matrix		// left-multiply
		).setTo("convert");
		glDrawArrays(GL_TRIANGLES, 0, n*3);
		glBindVertexArray(0);
	}
};
std::vector<Item> items;

void init()
{
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glDepthRange(0, 1);

	std::vector<Shader> shaders;
	shaders.push_back(std::move(Shader(GL_VERTEX_SHADER, "obj.vert")));
	shaders.push_back(std::move(Shader(GL_FRAGMENT_SHADER, "obj.frag")));
	prog = new Program(shaders);
	shaders.clear();

	Item item;
	item.load("obj.stl");
	item.sx->bindUp('a');
	item.sx->bindDown('x');
	item.sy->bindUp('c');
	item.sy->bindDown('r');
	item.sz->bindUp('e');
	item.sz->bindDown('q');
	item.ox->bindUp('z');
	item.ox->bindDown('s');
	item.oy->bindUp('d');
	item.oy->bindDown('s');
	item.oz->bindUp('w');
	item.oz->bindDown('s');
	items.push_back(std::move(item));
}

void display()
{
	glClearColor(0, 0, 0.2, 0);
	glClearDepth(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(prog->id);
	setUniformInt(lightCnt, "lightCnt");
	for (int i=0; i<lightCnt; i++)
	{
		lightDir[i].setTo(("lightDir["+std::to_string(i)+"]").c_str());
		lightCol[i].setTo(("lightCol["+std::to_string(i)+"]").c_str());
	}
	for (const auto &item : items)
		item.draw();
	glUseProgram(0);
	glutSwapBuffers();
}

void reshape(int w, int h)
{
	int l = std::min(w, h);
	glViewport((w-l)/2, (h-l)/2, l, l);
}

void keyboard(unsigned char key, int x, int y) // this is thread safe
{
	for (const auto &func : keyMap[key])
		func.first();
	glutPostRedisplay();
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE|GLUT_ALPHA|GLUT_DEPTH|GLUT_STENCIL);
	glutInitWindowSize(500, 500);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextFlags(GLUT_DEBUG);
	glutCreateWindow("Display");
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err)
		throw std::runtime_error((const char*)glewGetErrorString(err));
	std::clog <<  "Status: Using GLEW " << glewGetString(GLEW_VERSION) << std::endl;
	init();
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyboard);
	glutMainLoop();
	return 0;
}


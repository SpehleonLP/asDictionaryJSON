#include "../../../asDictionaryJSON/dictionary_json.h"
#include "add_on/scriptdictionary/scriptdictionary.h"
#include "add_on/scriptarray/scriptarray.h"
#include <sstream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>


static std::string DefaultNormalize(std::string s) { return s; }
static StringNormalizeFunc g_UnicodeFunc{&DefaultNormalize};
static StringNormalizeFunc g_PathFunc{&DefaultNormalize};

static CScriptDictionary  * asLoadFromFile(std::string path)
{
	std::string contents;

	try
	{
		{
			std::ifstream stream(g_PathFunc(std::move(path)));

			if(!stream.is_open())
			{
				throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), path);
			}

			stream.exceptions( std::iostream::failbit | std::iostream::badbit );

			stream.seekg(0, std::ios::end);
			size_t size = stream.tellg();
			stream.seekg(0, std::ios::beg);

			contents.resize(size);
			stream.read(&contents[0], size);
		}

		return asFromJSON_String(std::move(contents), asGetActiveContext()->GetEngine());
	}
	catch(std::exception & e)
	{
		asGetActiveContext()->SetException(e.what());
	}

	return nullptr;
}

static CScriptDictionary * asLoadFromString(std::string text)
{
	try
	{
		return asFromJSON_String(std::move(text), asGetActiveContext()->GetEngine());
	}
	catch(std::exception & e)
	{
		asGetActiveContext()->SetException(e.what());
	}

	return nullptr;
}

void asSaveToFile(std::string path, CScriptDictionary * in)
{
	try
	{
		std::ofstream stream(g_PathFunc(std::move(path)));

		if(!stream.is_open())
		{
			throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), path);
		}

		stream.exceptions( std::iostream::failbit | std::iostream::badbit );

		asToJSON_String(stream, in, false);
	}
	catch(std::exception & e)
	{
		asGetActiveContext()->SetException(e.what());
	}
}

static std::string asSaveToString(CScriptDictionary * in)
{
	try
	{
		std::ostringstream stream;

		stream.exceptions( std::iostream::failbit | std::iostream::badbit );

		asToJSON_String(stream, in, true);

		return stream.str();
	}
	catch(std::exception & e)
	{
		asGetActiveContext()->SetException(e.what());
	}

	return {};
}


void asRegisterDictionaryExtensions(asIScriptEngine * engine,  StringNormalizeFunc UnicodeNormalization, StringNormalizeFunc PathNormalization)
{
	if(UnicodeNormalization)
		g_UnicodeFunc = UnicodeNormalization;

	if(PathNormalization)
		g_PathFunc = PathNormalization;

	int r;
	r = engine->SetDefaultNamespace("dictionary"); assert(r >= 0);

	r = engine->RegisterGlobalFunction("dictionary@ FromJsonFile(string &in)", asFUNCTION(asLoadFromFile), asCALL_CDECL); assert(r >= 0);
	r = engine->RegisterGlobalFunction("dictionary@ FromJsonString(string &in)", asFUNCTION(asLoadFromString), asCALL_CDECL); assert(r >= 0);

	r = engine->SetDefaultNamespace(""); assert(r >= 0);

	r = engine->RegisterObjectMethod("dictionary", "void toJsonFile(string &in)", asFUNCTION(asSaveToFile), asCALL_CDECL_OBJLAST); assert(r >= 0);
	r = engine->RegisterObjectMethod("dictionary", "string toJsonString()", asFUNCTION(asSaveToString), asCALL_CDECL_OBJLAST); assert(r >= 0);
}


static bool asToJSON_String(std::vector<void const*> & object_stack,std::ostream & stream, CScriptDictionary const* dict, int depth, std::string indent, bool compressWhitespace);
static bool asToJSON_String(std::vector<void const*> & object_stack, std::ostream & stream, CScriptArray const* array, int depth, std::string indent, bool compressWhitespace);
static bool asToJSON_String(std::vector<void const*> & object_stack,std::ostream & stream, const char * field, int typeId, void const* object, int depth, std::string indent, bool compressWhitespace);
static bool asToJSON_String(std::vector<void const*> & object_stack,std::ostream & stream, int typeId, void const* object, int depth, std::string indent, bool compressWhitespace);
static bool CanSerialize(asIScriptEngine * engine, int asTypeId);
static std::string EscapeString(std::string s);

void asToJSON_String(std::ostream & stream, const CScriptDictionary * dict, bool compressWhitespace)
{
	if(dict == nullptr)
		return;

	std::vector<void const*> object_stack;
	asToJSON_String(object_stack, stream, dict, 1, compressWhitespace? " " : "\n", compressWhitespace);
	assert(object_stack.empty());
}

static bool asToJSON_String(std::vector<void const*> & object_stack, std::ostream & stream, CScriptDictionary const* dict, int depth, std::string indent, bool compressWhitespace)
{
	object_stack.push_back(dict);

	if(depth) stream << indent;

	if(!compressWhitespace)
		indent.resize(depth+1, '\t');

	stream << "{";

	bool first = true;
	for(auto i = dict->begin(); i != dict->end(); ++i)
	{
		if(!CanSerialize(asGetActiveContext()->GetEngine(), i.GetTypeId()))
		{
			continue;
		}

		if(first)
			stream << indent;
		else
		{
			if(!compressWhitespace)
				stream << ',' << indent;
			else
				stream << ", ";
		}


		asToJSON_String(object_stack, stream, i.GetKey().c_str(), i.GetTypeId(), i.GetAddressOfValue(), depth+1, indent, compressWhitespace);
		first = false;
	}

	if(!compressWhitespace)
		indent.resize(depth, '\t');

	stream << indent << "}";

	assert(object_stack.back() == dict);
	object_stack.pop_back();

	return true;
}

static bool CanSerialize(asIScriptEngine * engine, int asTypeId)
{
	if(asTypeId <= asTYPEID_DOUBLE)
		return true;

	auto typeInfo = engine->GetTypeInfoById(asTypeId);

	if(strcmp(typeInfo->GetName(), "string") == 0
	|| strcmp(typeInfo->GetName(), "dictionary"))
		return true;

	if(strcmp(typeInfo->GetName(), "array") == 0)
	{
		return CanSerialize(engine, typeInfo->GetSubTypeId());
	}

	return false;
}


static bool asToJSON_String(std::vector<void const*> & object_stack, std::ostream & stream, const CScriptArray * array, int depth, std::string indent, bool compressWhitespace)
{
	object_stack.push_back(array);

	bool r = false;

	if(!compressWhitespace)
		indent.resize(depth, '\t');

	stream << "[";

	for(asUINT i = 0; i != array->GetSize(); ++i)
	{
		if(i != 0)
		{
				stream << ", ";
		}

		r |= asToJSON_String(object_stack, stream, array->GetElementTypeId(), array->At(i), depth, indent, compressWhitespace);
	}

	stream << "]";


	assert(object_stack.back() == array);
	object_stack.pop_back();

	return r;
}

static bool asToJSON_String(std::vector<void const*> & object_stack, std::ostream & stream, const char * field, int typeId, void const* object, int depth, std::string indent, bool compressWhitespace)
{
	stream << "\"" << EscapeString(field) << "\": ";
	return asToJSON_String(object_stack, stream, typeId, object, depth, std::move(indent), compressWhitespace);
}


static bool asToJSON_String(std::vector<void const*> & object_stack, std::ostream & stream, int typeId, void const* object, int depth, std::string indent, bool compressWhitespace)
{
	if(object == nullptr)
	{
		stream << "null";
		return false;
	}

	if(typeId & asTYPEID_OBJHANDLE)
	{
		object = *(void**)object;
	}

	for(auto & c : object_stack)
	{
		if(c == object)
		{
			stream << "null";
			return false;
		}
	}

	stream.setf(std::ios::fixed,std::ios::floatfield);

	switch(typeId)
	{
	case asTYPEID_VOID:   stream << "null"; return false;
	case asTYPEID_BOOL:   stream <<((*(const bool*)object)? "true" : "false"); return false;
	case asTYPEID_INT8:   stream << *(const int8_t*)object; return false;
	case asTYPEID_INT16:  stream << *(const  int16_t*)object; return false;
	case asTYPEID_INT32:  stream << *(const  int32_t*)object; return false;
	case asTYPEID_INT64:  stream << *(const  int64_t*)object; return false;
	case asTYPEID_UINT8:  stream << *(const uint8_t*)object; return false;
	case asTYPEID_UINT16: stream << *(const uint16_t*)object; return false;
	case asTYPEID_UINT32: stream << *(const uint32_t*)object; return false;
	case asTYPEID_UINT64: stream << *(const uint64_t*)object; return false;
	case asTYPEID_FLOAT:
	{
		char buffer[16];
		snprintf(buffer, sizeof(buffer), ".6%f", *(const float*)object);

		stream << buffer;
		return false;
	}
	case asTYPEID_DOUBLE:
	{
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "%.10lf", *(const double*)object);

		stream << buffer;
		return false;
	}
	default:
		break;
	}

	auto typeInfo = asGetActiveContext()->GetEngine()->GetTypeInfoById(typeId);

	if(strcmp(typeInfo->GetName(), "string") == 0)
	{
		stream << '\"' << EscapeString(*(std::string*)object) << '\"';
		return false;
	}

	if(strcmp(typeInfo->GetName(), "dictionary") == 0)
	{
		return asToJSON_String(object_stack, stream, (CScriptDictionary*)object, depth, std::move(indent), compressWhitespace);
	}

	if(strcmp(typeInfo->GetName(), "array") == 0)
	{
		return asToJSON_String(object_stack, stream, (CScriptArray*)object, depth, std::move(indent), compressWhitespace);
	}

	return false;
}

std::string EscapeString(std::string s)
{
	s = g_UnicodeFunc(std::move(s));

	std::string out;
	out.reserve(s.size()*2);

	for(uint32_t i = 0; i < s.size(); ++i)
	{
		if(s[i] == '\\')
		{
			out.push_back('\\');
		}
		if(s[i] == '\"')
		{
			out.push_back('\\');
		}

		out.push_back(s[i]);

	}

	return out;
}

struct JSONTokenRange;

union JSON_ANY
{
	bool   boolean;
	asINT64 _int{};
	double  dbl;
	void  * obj;
};

static void asFromJSON_String(JSONTokenRange & stream, CScriptDictionary * dict);
static void asFromJSON_String(JSONTokenRange & stream, CScriptArray *& array);
static void asFromJSON_String(JSONTokenRange & stream, JSON_ANY & value, int & typeId);
static std::string CleanString(const char *, const char * end);
static std::string GetFullTypeName(asIScriptEngine * engine, int typeId);
static void FreePairVec(asIScriptEngine * engine, std::vector<std::pair<JSON_ANY, int> > vec);

struct JSONTokenRange
{
	static bool ischar(char c, const char * str)
	{
		for(;*str; ++str)
		{
			if(c == *str) return true;
		}

		return false;
	}

	JSONTokenRange(std::string & string, asIScriptEngine * engine) :
		engine(engine),
		asTypeIdDictionary(engine->GetTypeIdByDecl("dictionary")),
		asTypeIdString(engine->GetTypeIdByDecl("string")),
		begin(string.data()),
		end(string.data() + string.size()),
		tokBegin(&string[0]),
		tokEnd(&string[0]),
		swapChar(tokBegin? *tokBegin : 0)
	{
		popFront();
	}

	bool empty() const { return tokBegin >= end || *tokBegin == 0; }
	const char * front() const { return tokBegin; }
	const char * back() const { return tokEnd; }

	void popFront()
	{
		if(empty()) return;

		*tokEnd = swapChar;

//skip whitespace
		for(tokBegin = tokEnd; tokBegin < end && *tokBegin && *tokBegin <= ' '; ++tokBegin) { }

		if(tokBegin < end && ischar(*tokBegin, ",:[]{}"))
			tokEnd = tokBegin+1;
		else if(tokBegin < end && ischar(*tokBegin, "'\"`"))
		{
			bool is_escape = false;

			for(tokEnd = tokBegin+1; tokEnd < end && *tokEnd; ++tokEnd)
			{
				if(*tokEnd == *tokBegin && !is_escape)
				{
					++tokEnd;
					break;
				}

				is_escape = (*tokEnd == '\\');
			}
		}
		else
		{
	//move to end of token
			for(tokEnd = tokBegin+1; tokEnd < end && *tokEnd; ++tokEnd)
			{
				if(*tokEnd <= ' '
				|| ischar(*tokEnd, ",:[]{}") )
					break;
			}
		}

		if(tokEnd < end)
		{
			swapChar = *tokEnd;
			*tokEnd = 0;
		}
	}

	asIScriptEngine * const engine{};
	const int asTypeIdDictionary;
	const int asTypeIdString;

private:
	const char *const begin{};
	const char *const end{};
	char * tokBegin{};
	char * tokEnd{};

	char swapChar{};
};

CScriptDictionary * asFromJSON_String(std::string stream,  asIScriptEngine * engine)
{
	JSONTokenRange tokenizer(stream, engine);

	if(strcmp(tokenizer.front(), "{"))
		return nullptr;

	CScriptDictionary * dict = CScriptDictionary::Create(engine);

	try
	{
		asFromJSON_String(tokenizer, dict);
	}
	catch(std::exception & e)
	{
		dict->Release();
		throw;
	}

	return dict;
}

void asFromJSON_String(JSONTokenRange & stream, CScriptDictionary * dict)
{
	assert(strcmp(stream.front(), "{") == 0);

	while(!stream.empty())
	{
		stream.popFront();

		if(stream.empty())
			throw std::runtime_error("expected string found EOF");
//not strictly correct but i don't really care.
		if(*stream.front() == '}')
			break;

		if(JSONTokenRange::ischar(*stream.front(), "'`\"") == false)
			throw std::runtime_error("expected string found: \"" + std::string(stream.front()) + "\"");

		std::string key(CleanString(stream.front(), stream.back()));

		stream.popFront();

		if(stream.empty() || *stream.front() != ':')
			throw std::runtime_error("expected ':' found EOF");

		stream.popFront();

		if(stream.empty())
			throw std::runtime_error("expected value found EOF");

		JSON_ANY value{};
		int asTypeId{};
		asFromJSON_String(stream, value, asTypeId);

		if(asTypeId == asTYPEID_INT64)
		{
			dict->Set(key, value._int);
		}
		else if(asTypeId == asTYPEID_DOUBLE)
		{
			dict->Set(key, value.dbl);
		}
		else
		{
			dict->Set(key, value.obj, asTypeId);
			stream.engine->ReleaseScriptObject(value.obj, stream.engine->GetTypeInfoById(asTypeId));
		}

		stream.popFront();

		if(stream.empty() || !JSONTokenRange::ischar(*stream.front(), ",}"))
			throw std::runtime_error("expected ',' or '}' found EOF");

		if(*stream.front() == '}')
			break;
	}
}

static void asFromJSON_String(JSONTokenRange & stream, CScriptArray *& array)
{
	assert(strcmp(stream.front(), "[") == 0);

	std::vector<std::pair<JSON_ANY, int> > vec;

	try
	{
		while(!stream.empty())
		{
			stream.popFront();

			if(stream.empty())
				throw std::runtime_error("expected item found EOF");

	//not strictly correct but i don't really care.
			if(*stream.front() == ']')
				break;

			JSON_ANY value{};
			int asTypeId{};

			asFromJSON_String(stream, value, asTypeId);
			vec.push_back({value, asTypeId});
			assert(vec.back().first.obj == value.obj);

			stream.popFront();

			if(stream.empty() || !JSONTokenRange::ischar(*stream.front(), ",]"))
				throw std::runtime_error("expected ',' or ']' found EOF");

			if(*stream.front() == ']')
				break;
		}

		if(vec.empty())
		{
			array = nullptr;
			return;
		}

		int cur_type = vec[0].second;

		for(asUINT i = 1; i < vec.size(); ++i)
		{
			if(cur_type != vec[i].second)
			{
				if(cur_type == asTYPEID_DOUBLE && vec[i].second == asTYPEID_INT64)
				{
					vec[i].first.dbl = vec[i].first._int;
					vec[i].second = asTYPEID_DOUBLE;
					continue;
				}

				if(cur_type == asTYPEID_INT64 && vec[i].second == asTYPEID_DOUBLE)
				{
					cur_type = asTYPEID_DOUBLE;
					i = -1;
					continue;
				}


				FreePairVec(stream.engine, std::move(vec));
				throw std::runtime_error("type mismatch: all entries in array must have same asTYPEID.");
			}
		}

		std::string type_name = "array<" + GetFullTypeName(stream.engine, cur_type) + ">";

		auto typeInfo = stream.engine->GetTypeInfoByDecl(type_name.c_str());
		assert(typeInfo && (typeInfo->GetSubTypeId() == cur_type || typeInfo->GetSubTypeId() == (cur_type | asTYPEID_OBJHANDLE)));

		array = CScriptArray::Create(typeInfo, vec.size());

		auto subType = array->GetArrayObjectType()->GetSubType();
		bool is_value = subType? subType->GetFlags() & asOBJ_VALUE : false;

		for(asUINT i = 0; i < vec.size(); ++i)
		{
			array->SetValue(i, is_value? vec[i].first.obj : &vec[i].first);
			stream.engine->ReleaseScriptObject(vec[i].first.obj, subType);
			vec[i].first.obj = nullptr;
		}
	}
	catch(std::exception & e)
	{
		FreePairVec(stream.engine, std::move(vec));
		throw;
	}

}


std::string GetFullTypeName(asIScriptEngine * engine, int typeId)
{
	switch(typeId)
	{
	case asTYPEID_VOID: return "void";
	case asTYPEID_INT8: return "int8";
	case asTYPEID_INT16: return "int16";
	case asTYPEID_INT32: return "int";
	case asTYPEID_INT64: return "int64";
	case asTYPEID_UINT8: return "uint8";
	case asTYPEID_UINT16: return "uint16";
	case asTYPEID_UINT32: return "uint";
	case asTYPEID_UINT64: return "uint64";
	case asTYPEID_FLOAT: return "float";
	case asTYPEID_DOUBLE: return "double";
	default:
		break;
	}

	auto typeInfo = engine->GetTypeInfoById(typeId);

	if(!typeInfo)
	{
		return "void";
	}

	std::string name = typeInfo->GetName();

	if(typeInfo->GetFlags() & asOBJ_TEMPLATE)
	{
		name += "<";

		for(asUINT i = 0; i < typeInfo->GetSubTypeCount(); ++i)
		{
			name += GetFullTypeName(engine, typeInfo->GetSubTypeId(i));

			if(i+1 < typeInfo->GetSubTypeCount())
			{
				name += ", ";
			}
		}

		name += ">";
	}

	if(!(typeInfo->GetFlags() & asOBJ_VALUE))
		return name + "@";

	return name;
}

void FreePairVec(asIScriptEngine * engine, std::vector<std::pair<JSON_ANY, int> > vec)
{
	for(asUINT i = 0; i < vec.size(); ++i)
	{
		engine->ReleaseScriptObject(&vec[i].first, engine->GetTypeInfoById(vec[i].second));
	}
}

std::string CleanString(const char * input, const char * end)
{
	bool in_escape = false;

	std::string r;

	for(auto p = input+1; p < (end-1); ++p)
	{
		if(in_escape && *p == *input)
		{
			r.back() = *p;
			continue;
		}

		in_escape = (*p == '\\');
		r.push_back(*p);
	}

	return r;
}

#ifdef _WIN32
#define LONG_INT_SPEC "%lld"
#else
#define LONG_INT_SPEC "%ld"
#endif

static void asFromJSON_String(JSONTokenRange & stream, JSON_ANY & value, int & typeId)
{
	if(strcmp(stream.front(), "true") == 0)
	{
		value.boolean = true;
		typeId     = asTYPEID_BOOL;
		return;
	}
	if(strcmp(stream.front(), "false") == 0)
	{
		value.boolean = false;
		typeId     = asTYPEID_BOOL;
		return;
	}

	if(('0' <= *stream.front() && *stream.front() <= '9') || JSONTokenRange::ischar(*stream.front(), ".-+eE"))
	{
		auto p = stream.front();

		bool is_float = false;
		for(; *p != 0; ++p)
		{
			if(*p == '.' || tolower(*p) == 'e')
			{
				is_float = true;
			}
		}

		if(!is_float)
		{
			sscanf(stream.front(), LONG_INT_SPEC, &value._int);
			typeId = asTYPEID_INT64;
			return;
		}


		sscanf(stream.front(), "%lf", &value.dbl);
		typeId = asTYPEID_DOUBLE;
		return;
	}

	if(JSONTokenRange::ischar(*stream.front(), "'\"`"))
	{
		value.obj  = new std::string(CleanString(stream.front(), stream.back()));
		typeId     = stream.asTypeIdString;
		return;
	}

	if(*stream.front() == '[')
	{
		CScriptArray * array{};
		asFromJSON_String(stream, array);

		value.obj = array;
		typeId    = array->GetArrayTypeId();
		return;
	}

	if(*stream.front() == '{')
	{
		CScriptDictionary * dict{};

		try
		{
			dict = CScriptDictionary::Create(stream.engine);
			asFromJSON_String(stream, dict);
		}
		catch(std::exception & e)
		{
			dict->Release();
			throw;
		}

		value.obj = dict;
		typeId    = stream.asTypeIdDictionary;
		return;
	}

	throw std::runtime_error("unexpected token: " + std::string(stream.front()));
}


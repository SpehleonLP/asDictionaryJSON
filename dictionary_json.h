#ifndef DICTIONARY_EXTENSIONS_H
#define DICTIONARY_EXTENSIONS_H
#include <string>

class asIScriptEngine;
class asDocumenter;
class CScriptDictionary;

typedef std::string (*StringNormalizeFunc)(std::string);

void asRegisterDictionaryExtensions(asIScriptEngine * engine, StringNormalizeFunc UnicodeNormalization = nullptr, StringNormalizeFunc PathNormalization = nullptr);

void asToJSON_String(std::ostream & stream, CScriptDictionary * dict, bool compressWhitespace);
//ifstream is just the wrong base class to tokenize from
CScriptDictionary * asFromJSON_String(std::string stream, asIScriptEngine * engine);

#endif // DICTIONARY_EXTENSIONS_H

#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_LDTKLEVELREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_LDTKLEVELREADER_H

// #include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Includes/HashMap.h>
// #include <Engine/IO/Stream.h>
// #include <Engine/Scene/LayerGroup.h>
#include <Engine/Types/Property.h>
// #include <Engine/Types/Tileset.h>
#include <map>

#define JSMN_HEADER
#include "Engine/Scene/TileLayer.h"
#include "Engine/Types/Tileset.h"
#include "Libraries/jsmn.h"

struct LDtkLayer {
	char *Tileset = "";
	bool Visible = true;
	Uint32 CellWidth = 16;
	Uint32 CellHeight = 16;
	float OffsetX = 0.0;
	float OffsetY = 0.0;
	// HashMap<Property>* Properties = nullptr;
};

class LDtkLevelReader {
private:
	static float TokenToFloat(const char* ldtkl, jsmntok_t* token);
	static bool TokenToBool(const char* ldtkl, jsmntok_t* token);
	static char* TokenToString(const char* ldtkl, jsmntok_t* token);
	static bool IsTokenKey(jsmntok_t* token);
	static bool MatchToken(const char* ldtkl, jsmntok_t* token, const char* with);
	static bool LoadTileset(char* tileset, const char* parentFolder);
	static TileLayer* ReadLayer(LDtkLayer* layer, int width, int height);

public:
	static void Read(const char* sourceF, const char* parentFolder);
	// static void KeyGetValue(jsmntok_t* key);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_LDTKLEVELREADER_H */

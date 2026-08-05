#include <Engine/ResourceTypes/SceneFormats/LDtkLevelReader.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
// #include <Engine/IO/Compression/ZLibStream.h>
// #include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/Scene.h>
// #include <Engine/Scene/SceneLayer.h>
// #include <Engine/Scene/TileLayer.h>
// #include <Engine/TextFormats/XML/XMLParser.h>
// #include <Engine/Types/Entity.h>
// #include <Engine/Utilities/StringUtils.h>
#include <Engine/IO/ResourceStream.h>

#include "Engine/ResourceTypes/ResourceManager.h"

#define MAX_LAYER_COUNT 256

#define TILE_FLIPX_MASK 0x80000000U
#define TILE_FLIPY_MASK 0x40000000U
#define TILE_COLLA_MASK 0x30000000U
#define TILE_COLLB_MASK 0x0C000000U
#define TILE_COLLC_MASK 0x03000000U
#define TILE_IDENT_MASK 0x00FFFFFFU

float LDtkLevelReader::TokenToFloat(const char *ldtkl, jsmntok_t *token) {
	if (token->type == JSMN_PRIMITIVE) {
		switch (ldtkl[token->start]) {
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				case '-': return atof(ldtkl + token->start);
		}
	}
	return 0;
}

bool LDtkLevelReader::TokenToBool(const char *ldtkl, jsmntok_t *token) {
	if (token->type == JSMN_PRIMITIVE) {
		switch (ldtkl[token->start]) {
			case 'f': return false;
			case 't': return true;
		}
	}
	return false;
}

char *LDtkLevelReader::TokenToString(const char *ldtkl, jsmntok_t *token) {
	if (token->type == JSMN_STRING) {
		char* string = (char*)Memory::Malloc(token->end - token->start + 1);
		memcpy(string, ldtkl + token->start, token->end - token->start);
		string[token->end - token->start] = '\0';
		return string;
	}
	return "";
}

bool LDtkLevelReader::IsTokenKey(jsmntok_t *token) {
	return token->size != 0 && token->type == JSMN_STRING;
}

bool LDtkLevelReader::MatchToken(const char *ldtkl, jsmntok_t* token, const char *with) {
	if (token->type == JSMN_STRING) {
		return strncmp(ldtkl + token->start, with, token->end - token->start) == 0;
	}
	return false;
}

// TODO: Load multiple tiles.
bool LDtkLevelReader::LoadTileset(char *tileset, const char* parentFolder) {
	int curTileCount = (int)Scene::TileSpriteInfos.size();

	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
	char path[MAX_RESOURCE_PATH_LENGTH];
	snprintf(path, sizeof(path), "%s%s", parentFolder, tileset);

	StringUtils::NormalizePath(path, resourcePath, MAX_RESOURCE_PATH_LENGTH);

	// if (StringUtils::StartsWith(resourcePath, "../")) {
	// 	if (Application::DevMode) {
	// 		Log::Print(Log::LOG_WARN, "Path \"%s\" is outside of Resources.", resourcePath);
	// 	}
	// 	return false;
	// }
	//
	// // If it does not exist
	// if (!ResourceManager::ResourceExists(resourcePath)) {
	// 	Log::Print(Log::LOG_ERROR, "Resource \"%s\" does not exist!", resourcePath);
	// 	return false;
	// }

	ISprite* tileSprite = new ISprite();
	Texture* spriteSheet = tileSprite->AddSpriteSheet(resourcePath);
	if (!spriteSheet) {
		delete tileSprite;
		return false;
	}

	int cols = spriteSheet->Width / Scene::TileWidth;
	int rows = spriteSheet->Height / Scene::TileHeight;

	tileSprite->ReserveAnimationCount(1);
	tileSprite->AddAnimation("TileSprite", 0, 0, cols * rows);

	TileSpriteInfo info;
	for (int i = 0; i != cols * rows; i++) {
		info.Sprite = tileSprite;
		info.AnimationIndex = 0;
		info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
		info.TilesetID = Scene::Tilesets.size();
		Scene::TileSpriteInfos.push_back(info);

		tileSprite->AddFrame(
			0,
			(i % cols) * Scene::TileWidth,
			(i / cols) * Scene::TileHeight,
			Scene::TileWidth, Scene::TileHeight,
			-Scene::TileWidth / 2,
			-Scene::TileHeight / 2
			);

		Scene::EmptyTile = Scene::TileSpriteInfos.size();

		info.Sprite = tileSprite;
		info.AnimationIndex = 0;
		info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
		info.TilesetID = Scene::Tilesets.size();
		Scene::TileSpriteInfos.push_back(info);

		tileSprite->AddFrame(0, 0, 0, 1, 1, 0, 0);

		tileSprite->RefreshGraphicsID();

		Tileset sceneTileset(tileSprite,
			Scene::TileWidth,
			Scene::TileHeight,
			0,
			curTileCount,
			Scene::TileSpriteInfos.size(),
			resourcePath);
		Scene::Tilesets.push_back(sceneTileset);
	}

	return true;
}

TileLayer *LDtkLevelReader::ReadLayer(LDtkLayer *levelLayer, int width, int height) {
	TileLayer* layer = new TileLayer(width, height);
	layer->Name = levelLayer->Name;
	layer->Flags = SceneLayer::FLAGS_COLLIDEABLE;
	layer->Visible = levelLayer->Visible;
	layer->OffsetX = -levelLayer->OffsetX;
	layer->OffsetY = -levelLayer->OffsetY;

	for (size_t i = 0; i != width * height; i++) {
		layer->Tiles[i] = 2;
	}

	memcpy(layer->TilesBackup, layer->Tiles, layer->DataSize);

	return layer;
}


void LDtkLevelReader::Read(const char* sourceF, const char* parentFolder) {
	ResourceStream* res = ResourceStream::New(sourceF);
	if (!res) {
		return;
	}

	size_t size = res->Length();
	char* ldtkl = (char*)Memory::Malloc(size+1);

	if (!ldtkl) {
		return;
	}
	res->ReadBytes(ldtkl, size);
	ldtkl[size] = '\0';

	jsmn_parser parser;
	jsmntok_t* tokens;

	jsmn_init(&parser);
	size_t token_count = jsmn_parse(&parser, ldtkl, size, NULL, 0);
	tokens = (jsmntok_t*)malloc(sizeof(*tokens) * token_count);

	if (token_count != 0) {
		jsmn_init(&parser);
		do {
			int result = jsmn_parse(&parser, ldtkl, size, tokens, token_count);
			if (result < 0) {
				// TODO: Do error-handling.
				switch (result) {
					case JSMN_ERROR_INVAL: break;
					case JSMN_ERROR_NOMEM: break;
					case JSMN_ERROR_PART: break;
				}
				break;
			}

			int levelWidth = 0;
			int levelHeight = 0;

			// TODO: Use malloc?
			LDtkLayer* layers[MAX_LAYER_COUNT];
			int curLayer = -1;
			size_t layerCount = 0;

			// const jsmntok_t* layersToken = nullptr;
			int layersTokenIdx = -1;
			int layerDataTokenIdx = -1;

			// First token is useless.
			for (int i = 1; i != result; i++) {
				jsmntok_t& token = tokens[i];

				if (LDtkLevelReader::IsTokenKey(&token)) {
					jsmntok_t& value = tokens[i+1];

					if (LDtkLevelReader::MatchToken(ldtkl, &token, "pxWid")) {
						levelWidth = (int)LDtkLevelReader::TokenToFloat(ldtkl, &tokens[i+1]);
						continue;
					}

					if (LDtkLevelReader::MatchToken(ldtkl, &token, "pxHei")) {
						levelHeight = (int)LDtkLevelReader::TokenToFloat(ldtkl, &tokens[i+1]);
						continue;
					}

					if (layersTokenIdx == -1) {
						if (LDtkLevelReader::MatchToken(ldtkl, &token, "layerInstances")) {
							layersTokenIdx = i;
							// curLayer = 0;
							continue;
						}
					} else {
						if (layerDataTokenIdx == -1) {
							if (MatchToken(ldtkl, &token, "gridTiles")) {
								layerDataTokenIdx = i;
							}
						} else {
							if (layers[])
						}

						printf("%i\n", (i - layersTokenIdx) % (tokens[layersTokenIdx + 2].size));
						// if (i - layersTokenIdx % tokens[layersTokenIdx + 2].size == 0) {
						// 	curLayer++;
						// }
					}
				}
				//
				// if (layersToken != nullptr) {
				// 	if (token.type == JSMN_OBJECT) {
				// 		if (curLayer != layerCount) {
				// 			curLayer++;
				// 			layers[curLayer] = new LDtkLayer();
				// 		} else {
				// 			layersToken = nullptr;
				// 		}
				// 	} else {
				// 		// We assume it's the layers array as it's the next token after "layerInstances".
				// 		if (curLayer == -1) {
				// 			layerCount = token.size;
				// 		} else {
				// 			if (LDtkLevelReader::IsTokenKey(&token)) {
				// 				jsmntok_t& value = tokens[i+1];
				//
				// 				if (MatchToken(ldtkl, &token, "__identifier")) {
				// 					layers[curLayer]->Name = TokenToString(ldtkl, &value);
				// 					continue;
				// 				}
				//
				// 				if (MatchToken(ldtkl, &token, "__cWid")) {
				// 					layers[curLayer]->CellWidth = (Uint32)TokenToFloat(ldtkl, &value);
				// 					continue;
				// 				}
				//
				// 				if (MatchToken(ldtkl, &token, "__cHei")) {
				// 					layers[curLayer]->CellHeight = (Uint32)TokenToFloat(ldtkl, &value);
				// 					continue;
				// 				}
				//
				// 				if (MatchToken(ldtkl, &token, "__pxTotalOffsetX")) {
				// 					layers[curLayer]->OffsetX = TokenToFloat(ldtkl, &value);
				// 					continue;
				// 				}
				//
				// 				if (MatchToken(ldtkl, &token, "__pxTotalOffsetY")) {
				// 					layers[curLayer]->OffsetY = TokenToFloat(ldtkl, &value);
				// 					continue;
				// 				}
				//
				// 				if (MatchToken(ldtkl, &token, "__tilesetRelPath")) {
				// 					layers[curLayer]->Tileset = TokenToString(ldtkl, &value);
				// 					continue;
				// 				}
				//
				// 				if (MatchToken(ldtkl, &token, "visible")) {
				// 					layers[curLayer]->OffsetX = TokenToBool(ldtkl, &value);
				// 					continue;
				// 				}

								// Log::Print(Log::LOG_INFO, "%.*s", token.end - token.start, ldtkl + token.start);
							// }
						// }
					// }
				// }
			}
			//
			// Scene::SceneType = SCENETYPE_LDTK;
			//
			// Scene::FreePriorityLists();
			// Scene::PriorityPerLayer = BASE_PRIORITY_PER_LAYER;
			// Scene::InitPriorityLists();
			//
			// for (int i = 0; i != layerCount; i++) {
			// 	LDtkLayer *levelLayer = layers[i];
			// 	if (!LDtkLevelReader::LoadTileset(levelLayer->Tileset, parentFolder)) {
			// 		continue;
			// 	}
			//
			// 	TileLayer* layer = LDtkLevelReader::ReadLayer(levelLayer, levelWidth / Scene::TileWidth, levelHeight / Scene::TileHeight);
			//
			// 	Scene::AddLayer(layer);
			// }

			// Pretty sure there aren't layer properties in LDtk.
			// TODO: Check if there are layer properties in LDtk.
			// Scene::Properties = new HashMap<Property>(NULL, 4);
		} while (false);
	}

	free(tokens);
}

// FREE:
// 	XMLParser::Free(tileMapXML);
// }

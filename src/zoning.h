/** @file zoning.h */

#ifndef ZONING_H_
#define ZONING_H_

#include "openttd.h"
#include "tile_cmd.h"

enum EvaluationMode {
	CHECKNOTHING = 0,
	CHECKOPINION = 1,  ///< Check the local authority's opinion.
	CHECKBUILD = 2,    ///< Check wither or not the player can build.
	CHECKSTACATCH = 3, ///< Check catchment area for stations	
	CHECKBULUNSER = 4, ///< Check for unserved buildings
	CHECKINDUNSER = 5, ///< Check for unserved industries
	CHECKTOWNBORDERS = 6,
	CHECKCBBORDERS = 7,
};

struct Zoning {
	EvaluationMode inner;
	EvaluationMode outer;	
};

extern Zoning _zoning;

SpriteID TileZoningSpriteEvaluation(TileIndex tile, Owner owner, EvaluationMode ev_mode);
void DrawTileZoning(const TileInfo *ti);
void ShowZoningToolbar();

#endif /*ZONING_H_*/

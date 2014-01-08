/** @file zoning_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "station_type.h"
#include "station_base.h"
#include "industry.h"
#include "gfx_func.h"
#include "viewport_func.h"
#include "map_func.h"
#include "company_func.h"
#include "town_map.h"
#include "table/sprites.h"
#include "station_func.h"
#include "station_map.h"
#include "town.h"
#include "zoning.h"
#include <math.h>

Zoning _zoning;

/**
 * Draw the zoning sprites.
 *
 * @param SpriteID image
 *        the image
 * @param SpriteID colour
 *        the colour of the zoning
 * @param TileInfo ti
 *        the tile
 */
const byte _tileh_to_sprite[32] = {
	0, 1, 2, 3, 4, 5, 6,  7, 8, 9, 10, 11, 12, 13, 14, 0,
	0, 0, 0, 0, 0, 0, 0, 16, 0, 0,  0, 17,  0, 15, 18, 0,
};
void DrawZoningSprites(SpriteID image, SpriteID colour, const TileInfo *ti) {
	if ( colour!=INVALID_SPRITE_ID )
		AddSortableSpriteToDraw(image + _tileh_to_sprite[ti->tileh], colour, ti->x, ti->y, 0x10, 0x10, 1, ti->z + 7);
}

/**
 * Detect whether this area is within the acceptance of any station.
 *
 * @param TileArea area
 *        the area to search by
 * @param Owner owner
 *        the owner of the stations which we need to match again
 * @return true if a station is found
 */
bool IsAreaWithinAcceptanceZoneOfStation(TileArea area, Owner owner) {

	// TODO: Actually do owner check.
	
	int catchment = _settings_game.station.station_spread + (_settings_game.station.modified_catchment ? MAX_CATCHMENT : CA_UNMODIFIED);
	
	StationFinder morestations(TileArea(TileXY(TileX(area.tile)-catchment/2, TileY(area.tile)-catchment/2), 
		TileX(area.tile) + area.w + catchment, TileY(area.tile) + area.h + catchment));
	
	for ( Station * const *st_iter = morestations.GetStations()->Begin(); st_iter != morestations.GetStations()->End(); ++st_iter ) {
		Station *st = *st_iter;
		Rect rect = st->GetCatchmentRect();
		return TileArea(TileXY(rect.left, rect.top), TileXY(rect.right, rect.bottom)).Intersects(area);
	}
	
	return false;	
}

/**
 * Detect whether this tile is within the acceptance of any station.
 *
 * @param TileIndex tile
 *        the tile to search by
 * @param Owner owner
 *        the owner of the stations
 * @return true if a station is found
 */
bool IsTileWithinAcceptanceZoneOfStation(TileIndex tile, Owner owner) {

	// TODO: Actually do owner check.
	
	int catchment = _settings_game.station.station_spread + (_settings_game.station.modified_catchment ? MAX_CATCHMENT : CA_UNMODIFIED);
	
	StationFinder morestations(TileArea(TileXY(TileX(tile)-catchment/2, TileY(tile)-catchment/2), 
		catchment, catchment));
	
	for ( Station * const *st_iter = morestations.GetStations()->Begin(); st_iter != morestations.GetStations()->End(); ++st_iter ) {
		Station *st = *st_iter;
		Rect rect = st->GetCatchmentRect();
		if ( (uint)rect.left <= TileX(tile) && TileX(tile) <= (uint)rect.right
			&& (uint)rect.top <= TileY(tile) && TileY(tile) <= (uint)rect.bottom )
			return true;
	}
	
	return false;	
}

/**
 * Check whether the player can build in tile.
 *
 * @param TileIndex tile
 * @param Owner owner
 * @return red if they cannot
 */
SpriteID TileZoneCheckBuildEvaluation(TileIndex tile, Owner owner) {
	/* Let's first check for the obvious things you cannot build on */
	switch ( GetTileType(tile) ) {
		case MP_INDUSTRY:
		case MP_OBJECT:
		case MP_STATION:
		case MP_HOUSE:
		case MP_TUNNELBRIDGE: return SPR_PALETTE_ZONING_RED;
		/* There are only two things you can own (or some else
		 * can own) that you can still build on. i.e. roads and
		 * railways.
		 * @todo
		 * Add something more intelligent, check what tool the
		 * user is currently using (and if none, assume some
		 * standards), then check it against if owned by some-
		 * one else (e.g. railway on someone else's road).
		 * While that being said, it should also check if it
		 * is not possible to build railway/road on someone
		 * else's/your own road/railway (e.g. the railway track
		 * is curved or a cross).*/
		case MP_ROAD:
		case MP_RAILWAY:      {
			if ( GetTileOwner(tile) != owner ) return SPR_PALETTE_ZONING_RED;
			else return INVALID_SPRITE_ID;
		}
		default: return INVALID_SPRITE_ID;
	}
}

/**
 * Check the opinion of the local authority in the tile.
 *
 * @param TileIndex tile
 * @param Owner owner
 * @return black if no opinion, orange if bad, 
 *         light blue if good or invalid if no town
 */
SpriteID TileZoneCheckOpinionEvaluation(TileIndex tile, Owner owner) {	
	Town *town = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

	if ( town !=NULL ) {
		if ( HasBit(town->have_ratings, owner) ) {  // good : bad                 
			return ( town->ratings[owner] > 0 ) ? SPR_PALETTE_ZONING_LIGHT_BLUE : SPR_PALETTE_ZONING_ORANGE;
		} else {
			return SPR_PALETTE_ZONING_BLACK;      // no opinion
		}
	}	
	return INVALID_SPRITE_ID; // no town	
}
SpriteID TileZoneCheckTownBorders(TileIndex tile) {	
	HouseZonesBits next_zone = HZB_BEGIN;
	Town *town;

	FOR_ALL_TOWNS(town) {	//HZB_END
		while (next_zone < HZB_TOWN_OUTER_SUBURB && DistanceSquare(tile, town->xy) <= town->cache.squared_town_zone_radius[next_zone]){
			next_zone++;
		}
		if (next_zone >= HZB_TOWN_OUTER_SUBURB) break;
	}

	switch (next_zone) {
		case HZB_TOWN_EDGE: return INVALID_SPRITE_ID; // no town
		case HZB_TOWN_OUTSKIRT: return SPR_PALETTE_ZONING_LIGHT_BLUE; // Tz0
		default: return SPR_PALETTE_ZONING_BLACK; // Tz1 - Tz4
	}
	return INVALID_SPRITE_ID; //nothing
}

SpriteID TileZoneCheckCBBorders(TileIndex tile) {
	Town *town = CalcClosestTownFromTile(tile);

	if ( town != NULL ) {
		if (DistanceManhattan(town->xy, tile) <= _settings_client.gui.cb_distance_check) {
			return SPR_PALETTE_ZONING_LIGHT_BLUE; //cb catchment
		}
	}

	return INVALID_SPRITE_ID; // no town
}

/**
 * Detect whether the tile is within the catchment zone of a station.
 *
 * @param TileIndex tile
 * @param Owner owner
 * @return black if within, light blue if only in acceptance zone 
 *         and nothing if no nearby station.
 */
SpriteID TileZoneCheckStationCatchmentEvaluation(TileIndex tile, Owner owner) {
	//TODO: Actually check owner.

	// Never on a station.
	if ( IsTileType(tile, MP_STATION) )
		return INVALID_SPRITE_ID;
	
	// For provided goods	
	StationFinder stations(TileArea(tile, 1, 1));
	
	if ( stations.GetStations()->Length() > 0 ) {
		return SPR_PALETTE_ZONING_BLACK;
	}
	
	// For accepted goods	
	if ( IsTileWithinAcceptanceZoneOfStation(tile, owner) ){
		return SPR_PALETTE_ZONING_LIGHT_BLUE;
	}
	
	return INVALID_SPRITE_ID;
}

/**
 * Detect whether a building is unserved by a station of owner.
 *
 * @param TileIndex tile
 * @param Owner owner
 * @return red if unserved, orange if only accepting, nothing if served or not
 *         a building
 */
SpriteID TileZoneCheckUnservedBuildingsEvaluation(TileIndex tile, Owner owner) {
	//TODO: Actually use owner.	
	CargoArray dat;
	
	if ( IsTileType (tile, MP_HOUSE) 
		&& ( ( memset(&dat, 0, sizeof(dat)), AddAcceptedCargo(tile, dat, NULL), (dat[CT_MAIL] + dat[CT_PASSENGERS] > 0) ) 
			|| ( memset(&dat, 0, sizeof(dat)), AddProducedCargo(tile, dat), (dat[CT_MAIL] + dat[CT_PASSENGERS] > 0) ) ) ) {
		StationFinder stations(TileArea(tile, 1, 1));
		
		if ( stations.GetStations()->Length() > 0 ) {
			return INVALID_SPRITE_ID;
		}
	
		// For accepted goods		
		if ( IsTileWithinAcceptanceZoneOfStation(tile, owner) )
			return SPR_PALETTE_ZONING_ORANGE;
		
		//TODO: Check for stations that does not accept mail/passengers,
		//which is currently only truck stops.		
		return SPR_PALETTE_ZONING_RED;
	}
	
	return INVALID_SPRITE_ID;
}

/**
 * Detect whether an industry is unserved by a station of owner.
 *
 * @param TileIndex tile
 * @param Owner owner
 * @return red if unserved, orange if only accepting, nothing if served or not
 *         a building
 */
SpriteID TileZoneCheckUnservedIndustriesEvaluation(TileIndex tile, Owner owner) {
	//TODO: Actually use owner
	if ( IsTileType(tile, MP_INDUSTRY) ) {
		Industry *ind = Industry::GetByTile(tile);
		StationFinder stations(ind->location);
		
		if ( stations.GetStations()->Length() > 0 ) {
			return INVALID_SPRITE_ID;
		}
	
		// For accepted goods		
		if ( IsAreaWithinAcceptanceZoneOfStation(ind->location, owner) )
			return SPR_PALETTE_ZONING_ORANGE;
		
		//TODO: Check for stations that only accepts mail/passengers,
		//which is currently only bus stops.		
		return SPR_PALETTE_ZONING_RED;
	}
	
	return INVALID_SPRITE_ID;
}

/**
 * General evaluation function; calls all the other functions depending on
 * evaluation mode.
 *
 * @param TileIndex tile
 *        Tile to be evaluated.
 * @param Owner owner
 *        The current player
 * @param EvaluationMode ev_mode
 *        The current evaluation mode.
 * @return The colour returned by the evaluation functions (none if no ev_mode).
 */
SpriteID TileZoningSpriteEvaluation(TileIndex tile, Owner owner, EvaluationMode ev_mode) {
	switch ( ev_mode ) {
		case CHECKBUILD:       return TileZoneCheckBuildEvaluation(tile, owner);
		case CHECKOPINION:     return TileZoneCheckOpinionEvaluation(tile, owner);		
		case CHECKSTACATCH:    return TileZoneCheckStationCatchmentEvaluation(tile, owner);		
		case CHECKBULUNSER:    return TileZoneCheckUnservedBuildingsEvaluation(tile, owner);
		case CHECKINDUNSER:    return TileZoneCheckUnservedIndustriesEvaluation(tile, owner);
		case CHECKTOWNBORDERS: return TileZoneCheckTownBorders(tile);
		case CHECKCBBORDERS:   return TileZoneCheckCBBorders(tile);
		default:               return INVALID_SPRITE_ID;
	}
}

/**
 * Draw the the zoning on the tile.
 *
 * @param TileInfo ti
 *        the tile to draw on.
 */
void DrawTileZoning(const TileInfo *ti) {
	if ( IsTileType(ti->tile, MP_VOID) || _game_mode != GM_NORMAL ) return;
	
	if ( _zoning.outer != CHECKNOTHING )
		DrawZoningSprites(SPR_SELECT_TILE, TileZoningSpriteEvaluation(ti->tile, _local_company, _zoning.outer), ti);
		
	if ( _zoning.inner != CHECKNOTHING )
		DrawZoningSprites(SPR_INNER_HIGHLIGHT_BASE, TileZoningSpriteEvaluation(ti->tile, _local_company, _zoning.inner), ti);
}

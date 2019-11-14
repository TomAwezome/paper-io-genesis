# include <genesis.h>
# include <resources.h>

#define STATE_PLAYING 0
#define STATE_GAMEOVER 1
#define STATE_MENU 2
#define STATE_PAUSED 3
#define STATE_WIN 4

#define DIRECTION_NONE 0
//use DIRECTION_NONE sparingly, improper use leads to players desync.
#define DIRECTION_UP 1
#define DIRECTION_RIGHT 2
#define DIRECTION_DOWN 3
#define DIRECTION_LEFT 4

#define AISTATE_SAFE 0 // these are used for cpu players to determine their next actions
#define AISTATE_FIRST 1
#define AISTATE_SECOND 2
#define AISTATE_RETREAT 3

const u32 emptyTile[8]=
{
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
};
const u32 borderTile[8]=
{
    0x33333333,
    0x33333333,
    0x33333333,
    0x33333333,
    0x33333333,
    0x33333333,
    0x33333333,
    0x33333333,
};

struct Camera {
	s16 x; //s16 since scroll methods take s16 parameters.
	s16 y;
};
struct Player {
	u8 direction; // only need 4 directions of the 256 values possible. 1 up 2 right 3 down 4 left ... going through code to refactor these as defined vars, DIRECTION_UP etc.
	s16 x; // value in pixels
	s16 y;
	u8 nextDirection; // if inputs made while moving, stored here to use at next direction decision.
	Sprite *sprite; // holds the player's sprite, used when drawing.
	u8 spriteSelect; // 0 <-> total sprites in game. represents which index in spriteList player is.
	u8 gameover; // 0 not gameover 1 is gameover. goal, display screen asking retry or quit.
	u8 pathMapValue; // values used to represent player's claim/path numbers in map matrix. 
	u8 claimMapValue;//
	u8 isOnPath; // 1 if on path, 0 if not currently on path. used for claim/path calculation.
	s16 claimMinX; // claim min/max XY are to track bounds of players claim in map. used for path claim fill method.
	s16 claimMaxX; //
	s16 claimMinY; //
	s16 claimMaxY; //
	u32 score; // 100 on path, 25 on claim
	u8 state; // used for CPU players to determine actions, ignored for human player.
	u8 stepAmount; // used for CPU players to determine actions, ignored for human player.
	u8 lastSafeX; // used for CPU players to determine how to get back to their claim.
	u8 lastSafeY; //
};
struct ComputerPlayers {
	struct Player computers[20]; // allocate enough data for 20 computer players. if free memory later, increase?
	u8 computerCount; // how many computer players in game.
};
struct GameMap {
	u8 values[4096]; // allocate in advance max tileMap size (thus GameMap size) array vals which is 64x64 or 32x128, ultimately 4096.
	u8 columns;      // member values columns and rows represent how data is stored in the array.
	u8 rows;         // essentially creating a 2D array of size columns*rows out of 1D. if values array is bigger than needed, extra values are simply ignored.
	u8 gameState; // see compiler define vals at top of file. GameMap will hold the state of the game as well.
};
struct GameTile {
	const u32 *tile;
	u8 pal;
};
struct GameMap map;
struct Camera camera;
struct Player humanPlayer;
struct ComputerPlayers computerPlayers;
struct GameTile gameTileList[22]; // curently have 10 player sprites, each has 2 tiles so 20 tiles. plus 2 for 'empty' and border tile at indexes 0 & 1.

u16 search; // 16 bits since max search value could be 4096.
u8 getGameMapValue(u8 row, u8 column) {
	search = row * map.rows;
	search += column;
	return map.values[search]; // todo, bounds checking on input request
}
void setGameMapValue(u8 row, u8 column, u8 value) { 
	search = row * map.rows;
	search += column;
	map.values[search] = value; // todo bounds checking
}
void setTileMapGameValue(u8 row, u8 column, u8 value) { 
	if (value == 0) { // unclaimed
		VDP_clearTileMapRect(PLAN_B,column,row,1,1);
	}
	else {
		VDP_setTileMapXY(PLAN_B,TILE_ATTR_FULL(gameTileList[value].pal,0,FALSE,FALSE,value),column,row);
	}
}
u8 getValueAtPlayer(struct Player player) { // this is needed since player xy is stored as pixels (aka 8ths of tiles/map positions)
	if (player.x % 8 == 0 && player.y % 8 == 0) { // if player at XY 8 multiple (easy translate to map)
		return getGameMapValue(player.y / 8, player.x / 8);
	} else if (player.x % 8 != 0 && player.y % 8 == 0) { // if player at y multiple but not x multiple
		if (player.direction == DIRECTION_LEFT) { // if player going left
			return getGameMapValue(player.y/8,player.x/8);
		} else if (player.direction == DIRECTION_RIGHT) { // if player going right
			return getGameMapValue(player.y/8,(player.x/8)+1);
		}
	} else if (player.x % 8 == 0 && player.y % 8 != 0) { // if player at x multiple but not y multiple
		if (player.direction == DIRECTION_UP) { // if player going up
			return getGameMapValue(player.y/8,player.x/8);
		} else if (player.direction == DIRECTION_DOWN) { // if player going down
			return getGameMapValue((player.y/8)+1,player.x/8);
		}
	}
	return 1; // it shouldn't get here.... if it does, return 1 since 1 represents wall value.
}

u16 randomX;
u16 randomY;
void spawnHumanPlayer() {
    humanPlayer.direction = DIRECTION_NONE;
    humanPlayer.nextDirection = DIRECTION_NONE;
    humanPlayer.gameover = 0; // spawning so they shouldn't be gameovered
	randomX = random() % (map.columns-2); // -2 the modulo to ensure player and claim not trying to spawn inside map edge bounds (right/bottom)
	randomY = random() % (map.rows-2);    // XY based on GameMap. multiply by 8 to get respective pixel values
	if (randomX < 2) { // ensure player and claim not trying to spawn inside map edge bounds (left/top)
		randomX = 2;
	}
	if (randomY < 2) {
		randomY = 2;
	}
	humanPlayer.x = 8*randomX;
	humanPlayer.y = 8*randomY;
	for (u8 i = randomY-1; i <= randomY+1; i++) {
		for (u8 j = randomX-1; j <= randomX+1; j++) {
		    humanPlayer.score += 25;
			setGameMapValue(i,j,humanPlayer.claimMapValue);
			setTileMapGameValue(i,j,humanPlayer.claimMapValue);
		}
	}
	humanPlayer.claimMinX = (humanPlayer.x / 8) - 1; //setting claim bounds based on map and spawn position. one space left of humanPlayer.
	humanPlayer.claimMaxX = (humanPlayer.x / 8) + 1; // one space right of humanplayer
	humanPlayer.claimMinY = (humanPlayer.y / 8) - 1; // one space up(?) from humanplayer
	humanPlayer.claimMaxY = (humanPlayer.y / 8) + 1; // one space down(?) from humanplayer
}
void spawnComputerPlayers() {
	for (u8 c = 0; c < computerPlayers.computerCount; c++) { // iterate over each computer player
	    computerPlayers.computers[c].direction = DIRECTION_NONE;
	    computerPlayers.computers[c].nextDirection = DIRECTION_NONE;
	    computerPlayers.computers[c].gameover = 0; // spawning so they shouldn't be gameovered
		randomX = random() % (map.columns-2); // -2 the modulo to ensure player and claim not trying to spawn inside map edge bounds (right/bottom)
		randomY = random() % (map.rows-2);    // XY based on GameMap. multiply by 8 to get respective pixel values
		if (randomX < 2) { // ensure player and claim not trying to spawn inside map edge bounds (left/top)
			randomX = 2;
		}
		if (randomY < 2) {
			randomY = 2;
		}
		computerPlayers.computers[c].x = 8*randomX;
		computerPlayers.computers[c].y = 8*randomY;
		for (u8 i = randomY-1; i <= randomY+1; i++) {
			for (u8 j = randomX-1; j <= randomX+1; j++) {
			    computerPlayers.computers[c].score += 25;
				setGameMapValue(i,j,computerPlayers.computers[c].claimMapValue);
				setTileMapGameValue(i,j,computerPlayers.computers[c].claimMapValue);
			}
		}
		computerPlayers.computers[c].claimMinX = (computerPlayers.computers[c].x / 8) - 1; //setting claim bounds based on map and spawn position. one space left of Player.
		computerPlayers.computers[c].claimMaxX = (computerPlayers.computers[c].x / 8) + 1; // one space right of player
		computerPlayers.computers[c].claimMinY = (computerPlayers.computers[c].y / 8) - 1; // one space up(?) from player
		computerPlayers.computers[c].claimMaxY = (computerPlayers.computers[c].y / 8) + 1; // one space down(?) from player
	}
}

u8 humanPlayerWon; // 0 no 1 yes. on fill, assume 1, then if claim fails, set to 0.
u8 hasClaimLeft;
u8 hasClaimRight;
u8 hasClaimUp;
u8 hasClaimDown;
u32 fillScoreResult; // wrote myself into a hole, C pass-by-value means can't update player's score if passing player as param. so changing void to u32 so score can be manipulated in checkPlayers using fill method return value of score
u32 fillPlayerPath(struct Player player) {
    if (player.claimMinY == 1 && player.claimMinX == 1 && player.claimMaxY == 62 && player.claimMaxX == 62) { // i think these values are right?
        if (player.spriteSelect == humanPlayer.spriteSelect) { // this is a quick way to check to see if the player in this method is the human player.
            humanPlayerWon = 1;
        }
    }
    fillScoreResult = 0;
	for (u8 y = player.claimMinY; y <= player.claimMaxY; y++) {     // only look to fill in map values within player's claim area
		for (u8 x = player.claimMinX; x <= player.claimMaxX; x++) { //
			if (getGameMapValue(y,x) == player.pathMapValue) { // set any path tiles to claimed tiles
		        fillScoreResult += 25;
				setGameMapValue(y,x,player.claimMapValue);
				setTileMapGameValue(y,x,player.claimMapValue);
			}
		}
	}
	for (u8 y = player.claimMinY; y <= player.claimMaxY; y++) {     // only look to fill in map values within player's claim area
		for (u8 x = player.claimMinX; x <= player.claimMaxX; x++) { // path fill algorithm is generous
//			if (getGameMapValue(y,x) == 0) { // if unclaimed, search in 4 directions outward from tile at (y,x) to ensure there are claims on all sides
			if (getGameMapValue(y,x) != player.claimMapValue && (getGameMapValue(y,x) % 2 != 0 || getGameMapValue(y,x) == 0)) { // if not player's claim, search in 4 directions outward from tile at (y,x) to ensure there are claims on all sides
				hasClaimLeft = 0; // 0 false 1 true. if all are 1, tile should be filled
				hasClaimRight = 0;
				hasClaimUp = 0;
				hasClaimDown = 0;
				if (y != player.claimMinY) {
					for (u8 i = y-1; i >= player.claimMinY; i--) { // check values up from tile at (y,x)
						if (getGameMapValue(i,x) == player.claimMapValue) {
							hasClaimUp = 1;
							break;
						}
					}
					if (hasClaimUp == 0) { // if search failed, skip tile
					    humanPlayerWon = 0;
						continue;
					}
				}
				if (y != player.claimMaxY) {
					for (u8 i = y+1; i <= player.claimMaxY; i++) { // check values down from tile at (y,x)
						if (getGameMapValue(i,x) == player.claimMapValue) {
							hasClaimDown = 1;
							break;
						}
					}
					if (hasClaimDown == 0) { // if search failed, skip tile
					    humanPlayerWon = 0;
						continue;
					}
				}
				if (x != player.claimMinX) {
					for (u8 i = x-1; i >= player.claimMinX; i--) { // check values left from tile at (y,x)
						if (getGameMapValue(y,i) == player.claimMapValue) {
							hasClaimLeft = 1;
							break;
						}
					}
					if (hasClaimLeft == 0) { // if search failed, skip tile
					    humanPlayerWon = 0;
						continue;
					}
				}
				if (x != player.claimMaxX) {
					for (u8 i = x+1; i <= player.claimMaxX; i++) { // check values right from tile at (y,x)
						if (getGameMapValue(y,i) == player.claimMapValue) {
							hasClaimRight = 1;
							break;
						}
					}
				}
				if (hasClaimRight == 0) { // if search failed, skip tile
				    humanPlayerWon = 0;
					continue;
				}
				if (hasClaimLeft == 1 && hasClaimRight == 1 && hasClaimUp == 1 && hasClaimDown == 1) { // still checking all for sanity check
				    fillScoreResult += 25;//cant increase score directly because C is pass-by-value, so using a variable to instead return in checkPlayers()
				    if (getGameMapValue(y,x) == humanPlayer.claimMapValue) {
				        humanPlayer.score -= 25;
				    }
					setGameMapValue(y,x,player.claimMapValue);
					setTileMapGameValue(y,x,player.claimMapValue);
				}
			}
		}
	}
	if (humanPlayerWon) {
	    map.gameState = STATE_WIN;
	}
	return fillScoreResult;
}

void initMap(u8 rows, u8 columns) {
	VDP_clearPlan(PLAN_B,1); // to avoid running setTileMapGameValue for up to 4096 iterations when clearing map
	map.rows = rows;   
	map.columns = columns;
	//set all map to unclaimed
    for (u8 j = 0; j < map.rows; j++) {
    	for (u8 k = 0; k < map.columns; k++) {
    		setGameMapValue(j,k,0); // 0 represents an unclaimed tile so all tiles are being set to zero
    	}
    }
    //set borders of map as walls
    for (u8 j = 0; j < map.columns; j++) { // upper lower borders
    	setGameMapValue(0,j,1);
		setTileMapGameValue(0,j,1);
		setGameMapValue(map.rows-1,j,1); // map.rows-1 should be index of lower wall
		setTileMapGameValue(map.rows-1,j,1);
    }
    for (u8 j = 0; j < map.rows; j++) { // side borders
    	setGameMapValue(j,0,1);
    	setTileMapGameValue(j,0,1);
    	setGameMapValue(j,map.columns-1,1); // map.columns-1 should be index of right wall
    	setTileMapGameValue(j,map.columns-1,1);
    }
}

void adjustCamera() {
	// adjust based on player. trying to center it.
	// let's experiment by just working out a raw centering algorithm.
	// cameraXY is 0 and based on pixels and each spriteXY is pixel based but has 8x8 size.
	// so, cameraX = humanPlayerX - 160 , cameraY = humanPlayerY - 112 ? each is half of pixel distance of XY screen res.
	camera.x = 160 - humanPlayer.x;
	camera.y = humanPlayer.y - 112;
	//
	// if camera is off screen left/up, fix that.
	if (camera.x > 0) camera.x = 0;
	if (camera.y < 0) camera.y = 0;
	// if camera is far enough to start looping right/down, fix that.
	if (camera.x < -192) camera.x = -192;
	if (camera.y > 288) camera.y = 288; // so far we're guessing/forcing magic numbers against map WH >->
	//

}

void drawPlayers() {
	// players have xy vals relative to map, albeit pixel.
	// relative to camera we can only see cameraX+320(319?) and cameraY+112(111?)
	// and relative to sprite layer, draw at playerXY-cameraXY
	SPR_setPosition(humanPlayer.sprite,humanPlayer.x+camera.x,humanPlayer.y-camera.y);
	for (u8 i = 0; i < computerPlayers.computerCount; i++) { // for each computer player
		if (computerPlayers.computers[i].gameover == 0) { // if computer not gameover, display them
			SPR_setPosition(computerPlayers.computers[i].sprite,computerPlayers.computers[i].x+camera.x,computerPlayers.computers[i].y-camera.y);
		}
	}
}

void movePlayers() {
	// move players and orient them if at a tile multiple.
	if (humanPlayer.direction == DIRECTION_UP) { // up
		humanPlayer.y--;
	} else if (humanPlayer.direction == DIRECTION_DOWN) { // down
		humanPlayer.y++;
	} else if (humanPlayer.direction == DIRECTION_RIGHT) { // right
		humanPlayer.x++;
	} else if (humanPlayer.direction == DIRECTION_LEFT) { // left ... in retrospect i think C has switch(){} for this. and would be more optimized.
		humanPlayer.x--;
	}
	if (humanPlayer.x % 8 == 0 && humanPlayer.y % 8 == 0) {
		humanPlayer.direction = humanPlayer.nextDirection;
	}
	for (u8 i = 0; i < computerPlayers.computerCount; i++) { // for each computer player
		if (computerPlayers.computers[i].gameover == 0) {// if computer not gameover, move them 
			if (computerPlayers.computers[i].direction == DIRECTION_UP) { // up
				computerPlayers.computers[i].y--;
			} else if (computerPlayers.computers[i].direction == DIRECTION_DOWN) { // down
				computerPlayers.computers[i].y++;
			} else if (computerPlayers.computers[i].direction == DIRECTION_RIGHT) { // right
				computerPlayers.computers[i].x++;
			} else if (computerPlayers.computers[i].direction == DIRECTION_LEFT) { // left ... in retrospect i think C has switch(){} for this. and would be more optimized.
				computerPlayers.computers[i].x--;
			}
			if (computerPlayers.computers[i].x % 8 == 0 && computerPlayers.computers[i].y % 8 == 0) {
				computerPlayers.computers[i].direction = computerPlayers.computers[i].nextDirection;
			}
		}
	}
	
}

void winHumanPlayer() {
	VDP_fillTileMapRect(PLAN_B, TILE_ATTR_FULL(PAL0,0,FALSE,FALSE,1), (-camera.x/8) + 6, (camera.y/8) + 7, 27, 2);
	VDP_drawText("You win! ",15,7);
	VDP_drawText("Retry (A) or Exit (START).",7,8);
	while (map.gameState == STATE_WIN) {
		// just wait. joy event handler will check which input is pressed and move game state accordingly.
		VDP_waitVSync();
	}
    humanPlayerWon = 0;
}

void gameoverHumanPlayer() {
	for (u8 y = 0; y < map.rows; y++) {       // for all map values check if belongs to player and if so empty it
		for (u8 x = 0; x < map.columns; x++) {// 
			if (getGameMapValue(y,x) == humanPlayer.pathMapValue || getGameMapValue(y,x) == humanPlayer.claimMapValue) {
				// if value belongs to player
				setGameMapValue(y,x,0); // set empty
				setTileMapGameValue(y,x,0); // set tilemap empty
			}
		}
	}
	map.gameState = STATE_GAMEOVER;
	VDP_fillTileMapRect(PLAN_B, TILE_ATTR_FULL(PAL0,0,FALSE,FALSE,1), (-camera.x/8) + 6, (camera.y/8) + 7, 27, 2);
	VDP_drawText("Gameover.",15,7);
	VDP_drawText("Retry (A) or Exit (START).",7,8);
	while (map.gameState == STATE_GAMEOVER) {
		// just wait. joy event handler will check which input is pressed and move game state accordingly.
		VDP_waitVSync();
	}
}

void gameoverComputerPlayer(u8 c) { // c is desired index of computerPlayers.computers array of Players.
	for (u8 y = 0; y < map.rows; y++) {       // for all map values check if belongs to player and if so empty it
		for (u8 x = 0; x < map.columns; x++) {// 
			if (getGameMapValue(y,x) == computerPlayers.computers[c].pathMapValue || getGameMapValue(y,x) == computerPlayers.computers[c].claimMapValue) {
				// if value belongs to player
				setGameMapValue(y,x,0); // set empty
				setTileMapGameValue(y,x,0); // set tilemap empty
			}
		}
	}
	SPR_setVisibility(computerPlayers.computers[c].sprite,HIDDEN);
}

/*u8 isRespawnFailed; // when random location is chosen, if its not all free, don't respawn them.
void respawnComputerPlayer(u8 c) {
    isRespawnFailed = 0;
    randomX = random() % (map.columns-2); // -2 the modulo to ensure player and claim not trying to spawn inside map edge bounds (right/bottom)
	randomY = random() % (map.rows-2);    // XY based on GameMap. multiply by 8 to get respective pixel values
	if (randomX < 2) { // ensure player and claim not trying to spawn inside map edge bounds (left/top)
		randomX = 2;
	}
	if (randomY < 2) {
		randomY = 2;
	}
	for (u8 i = randomY-1; i <= randomY+1; i++) {
		for (u8 j = randomX-1; j <= randomX+1; j++) {
		    if (getGameMapValue(i,j) != 0) {
		        isRespawnFailed = 1;
		    }
		}
	}
	if (!(isRespawnFailed)) {
	    SYS_die("respawned a cpu successfully");
    	computerPlayers.computers[c].x = 8*randomX;
    	computerPlayers.computers[c].y = 8*randomY;
    	computerPlayers.computers[c].direction = DIRECTION_NONE;
        computerPlayers.computers[c].nextDirection = DIRECTION_NONE;
        computerPlayers.computers[c].gameover = 0; // spawning so they shouldn't be gameovered
    	for (u8 i = randomY-1; i <= randomY+1; i++) {
    		for (u8 j = randomX-1; j <= randomX+1; j++) {
    		    computerPlayers.computers[c].score += 25;
    			setGameMapValue(i,j,computerPlayers.computers[c].claimMapValue);
    			setTileMapGameValue(i,j,computerPlayers.computers[c].claimMapValue);
    		}
    	}
    	computerPlayers.computers[c].claimMinX = (computerPlayers.computers[c].x / 8) - 1; //setting claim bounds based on map and spawn position. one space left of Player.
    	computerPlayers.computers[c].claimMaxX = (computerPlayers.computers[c].x / 8) + 1; // one space right of player
    	computerPlayers.computers[c].claimMinY = (computerPlayers.computers[c].y / 8) - 1; // one space up(?) from player
    	computerPlayers.computers[c].claimMaxY = (computerPlayers.computers[c].y / 8) + 1; // one space down(?) from player    
    	SPR_setVisibility(computerPlayers.computers[c].sprite,VISIBLE);
    }
}

u8 isRespawnEnabled; // if set to 1 in menu, will attempt cpu respawn*/
void checkPlayers() {
	// (check if two players collide but we can do that later)
	// check if players collide with wall, .. check if players collide with a path/claim in map
	// for checking a player position against the map, round up. if position was just at a multiple, and increments, always round up to next map spot (ceiling).
	if (getValueAtPlayer(humanPlayer) == 1) { // humanplayer hit wall
		//gameover , reset screen(?)
		humanPlayer.gameover = 1; // gameover player
		gameoverHumanPlayer();
	}
	for (u8 i = 0; i < computerPlayers.computerCount; i++) { // check all computer wall collisions
		if (computerPlayers.computers[i].gameover == 0) { // if not gameover, check computer wall collision
			if (getValueAtPlayer(computerPlayers.computers[i]) == 1) { // computer hit wall
				computerPlayers.computers[i].gameover = 1; // gameover computer
				gameoverComputerPlayer(i);                 // 
				/*if (isRespawnEnabled) {
				    respawnComputerPlayer(i);
				}*/
			}
		}
	}
	
	if (humanPlayer.x % 8 == 0 && humanPlayer.y % 8 == 0) { // if humanplayer at multiple (thus xy/8 is map value)
		if (humanPlayer.isOnPath) {
			if (humanPlayer.x / 8 < humanPlayer.claimMinX) { // adjust humanplayer's claim bounds
				humanPlayer.claimMinX = humanPlayer.x / 8;
			} else if (humanPlayer.x / 8 > humanPlayer.claimMaxX) {
				humanPlayer.claimMaxX = humanPlayer.x / 8;
			} else if (humanPlayer.y / 8 < humanPlayer.claimMinY) {
				humanPlayer.claimMinY = humanPlayer.y / 8;
			} else if (humanPlayer.y / 8 > humanPlayer.claimMaxY) {
				humanPlayer.claimMaxY = humanPlayer.y / 8;
			}
		}
		if (getValueAtPlayer(humanPlayer) == 0) { // if hit unclaimed
		    humanPlayer.score += 100;
			setGameMapValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.pathMapValue);    // claim it (path)
			setTileMapGameValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.pathMapValue);//
			if (!humanPlayer.isOnPath) {
				humanPlayer.isOnPath = 1;
			} 
		} else if (getValueAtPlayer(humanPlayer) == humanPlayer.claimMapValue) { // if humanplayer hit humanplayer's claim
			if (humanPlayer.isOnPath) {
				humanPlayer.isOnPath = 0;
				// run fill method
				humanPlayer.score += fillPlayerPath(humanPlayer); // C pass-by-value workaround
			}
		}
		
		for (u8 i = 0; i < computerPlayers.computerCount; i++) { // check all computers against humanplayer
			if (getValueAtPlayer(humanPlayer) == computerPlayers.computers[i].pathMapValue) { // check if humanplayer hits that computers path
				gameoverComputerPlayer(i); // if so, gameover the respective computer player
				computerPlayers.computers[i].gameover = 1;
				if (humanPlayer.isOnPath == 1) { // if on path set map val and tile for path
				    humanPlayer.score += 100;
					setGameMapValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.pathMapValue);    // claim it (path)
					setTileMapGameValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.pathMapValue);//
				} else if (humanPlayer.isOnPath == 0) { // else if still in own claim, set map val and tile for claim
				    humanPlayer.score += 25;
					setGameMapValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.claimMapValue);    // claim it (claim)
					setTileMapGameValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.claimMapValue);//
				}
			} else if (getValueAtPlayer(humanPlayer) == computerPlayers.computers[i].claimMapValue) { // check if player hits that computers claim
				// todo , reduce computer players score by 1 tile
				humanPlayer.score += 100;
				setGameMapValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.pathMapValue);    // claim it (path)
				setTileMapGameValue(humanPlayer.y/8,humanPlayer.x/8,humanPlayer.pathMapValue);// (same as unclaimed logic)
				if (!humanPlayer.isOnPath) {
					humanPlayer.isOnPath = 1;
				}
			} else if (getValueAtPlayer(computerPlayers.computers[i]) == humanPlayer.pathMapValue) { // check if computer hits human path
			    if (computerPlayers.computers[i].gameover == 0) { // without this check , gameover computer players will ghost-kill player
    			    humanPlayer.gameover = 1;
			        gameoverHumanPlayer();
			    }
			}
		}
	}
	
	for (u8 i = 0; i < computerPlayers.computerCount; i++) {
		if (computerPlayers.computers[i].gameover == 0) { // if not gameover, check computer game states
			if (computerPlayers.computers[i].x % 8 == 0 && computerPlayers.computers[i].y % 8 == 0) { // if player at multiple (thus xy/8 is map value)
				if (computerPlayers.computers[i].isOnPath) {
					if (computerPlayers.computers[i].x / 8 < computerPlayers.computers[i].claimMinX) { // adjust player's claim bounds
						computerPlayers.computers[i].claimMinX = computerPlayers.computers[i].x / 8;
					} else if (computerPlayers.computers[i].x / 8 > computerPlayers.computers[i].claimMaxX) {
						computerPlayers.computers[i].claimMaxX = computerPlayers.computers[i].x / 8;
					} else if (computerPlayers.computers[i].y / 8 < computerPlayers.computers[i].claimMinY) {
						computerPlayers.computers[i].claimMinY = computerPlayers.computers[i].y / 8;
					} else if (computerPlayers.computers[i].y / 8 > computerPlayers.computers[i].claimMaxY) {
						computerPlayers.computers[i].claimMaxY = computerPlayers.computers[i].y / 8;
					}
				}
				if (getValueAtPlayer(computerPlayers.computers[i]) == 0) { // if hit unclaimed
					setGameMapValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);    // claim it (path)
					setTileMapGameValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);//
					if (!computerPlayers.computers[i].isOnPath) {
						computerPlayers.computers[i].isOnPath = 1;
					} 
				} else if (getValueAtPlayer(computerPlayers.computers[i]) == computerPlayers.computers[i].claimMapValue) { // if player hit their own claim
					if (computerPlayers.computers[i].isOnPath) {
						computerPlayers.computers[i].isOnPath = 0;
						// run fill method
						computerPlayers.computers[i].score += fillPlayerPath(computerPlayers.computers[i]); // C pass-by-value workaround
					}
				} else if (getValueAtPlayer(computerPlayers.computers[i]) == humanPlayer.claimMapValue) { // if cpu player hit human claim
    				// todo , reduce computer players score by 1 tile
    				//computerPlayers.computers[i].score += 100;  is cpu score important?...
    				humanPlayer.score -= 25; // since cpu hit player and player score onscreen, decrement their score by 1 claim tile (25)
			    	setGameMapValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);    // claim it (path)
				    setTileMapGameValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);// (same as unclaimed logic)
				    if (!computerPlayers.computers[i].isOnPath) {
					    computerPlayers.computers[i].isOnPath = 1;
				    }
				}
				
				for (u8 j = 0; j < computerPlayers.computerCount; j++) { // check all computers against all computers
					if (i == j) {
						continue;
					}
					if (getValueAtPlayer(computerPlayers.computers[i]) == computerPlayers.computers[j].pathMapValue) { // check if player hits that computers path
						gameoverComputerPlayer(j); // if so, gameover the respective computer player
						computerPlayers.computers[j].gameover = 1;
						if (computerPlayers.computers[i].isOnPath == 1) { // if on path set map val and tile for path
							setGameMapValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);    // claim it (path)
							setTileMapGameValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);//
						} else if (computerPlayers.computers[i].isOnPath == 0) { // else if still in own claim, set map val and tile for claim
							setGameMapValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].claimMapValue);    // claim it (claim)
							setTileMapGameValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].claimMapValue);//
						}
					} else if (getValueAtPlayer(computerPlayers.computers[i]) == computerPlayers.computers[j].claimMapValue) { // check if player hits that computers claim
						// todo , reduce computer players score by 1 tile
						setGameMapValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);    // claim it (path)
						setTileMapGameValue(computerPlayers.computers[i].y/8,computerPlayers.computers[i].x/8,computerPlayers.computers[i].pathMapValue);// (same as unclaimed logic)
						if (!computerPlayers.computers[i].isOnPath) {
							computerPlayers.computers[i].isOnPath = 1;
						}
					}
				}
			}
		}
	}
	
}

void loadTiles() { // tiles are all claims and paths. PAL0 and PAL1 should be the palettes that need to be set based on the images.

	gameTileList[0].tile = (const u32 *)emptyTile;
	gameTileList[0].pal = PAL0;
	gameTileList[1].tile = (const u32 *)borderTile;
	gameTileList[1].pal = PAL0;
	gameTileList[2].tile = batpath.tileset->tiles;
	gameTileList[2].pal = PAL0;
	gameTileList[3].tile = batclaim.tileset->tiles;
	gameTileList[3].pal = PAL0;
	gameTileList[4].tile = cactuspath.tileset->tiles;
	gameTileList[4].pal = PAL1;
	gameTileList[5].tile = cactusclaim.tileset->tiles;
	gameTileList[5].pal = PAL1;
	gameTileList[6].tile = cowpath.tileset->tiles;
	gameTileList[6].pal = PAL0;
	gameTileList[7].tile = cowclaim.tileset->tiles;
	gameTileList[7].pal = PAL0;
	gameTileList[8].tile = duckpath.tileset->tiles;
	gameTileList[8].pal = PAL1;
	gameTileList[9].tile = duckclaim.tileset->tiles;
	gameTileList[9].pal = PAL1;
	gameTileList[10].tile = heartpath.tileset->tiles;
	gameTileList[10].pal = PAL0;
	gameTileList[11].tile = heartclaim.tileset->tiles;
	gameTileList[11].pal = PAL0;
	gameTileList[12].tile = ladybugpath.tileset->tiles;
	gameTileList[12].pal = PAL1;
	gameTileList[13].tile = ladybugclaim.tileset->tiles;
	gameTileList[13].pal = PAL1;
	gameTileList[14].tile = strawberrypath.tileset->tiles;
	gameTileList[14].pal = PAL0;
	gameTileList[15].tile = strawberryclaim.tileset->tiles;
	gameTileList[15].pal = PAL0;
	gameTileList[16].tile = tvpath.tileset->tiles;
	gameTileList[16].pal = PAL0;
	gameTileList[17].tile = tvclaim.tileset->tiles;
	gameTileList[17].pal = PAL1;
	gameTileList[18].tile = watermelonpath.tileset->tiles;
	gameTileList[18].pal = PAL0;
	gameTileList[19].tile = watermelonclaim.tileset->tiles;
	gameTileList[19].pal = PAL1;
	gameTileList[20].tile = whalepath.tileset->tiles;
	gameTileList[20].pal = PAL1;
	gameTileList[21].tile = whaleclaim.tileset->tiles;
	gameTileList[21].pal = PAL1;
	for (u8 i = 1; i <= 21; i++) { // for condition int compare should match last gameTileList index. starting from i since bg tile doesn't have to be loaded.
		VDP_loadTileData(gameTileList[i].tile,i,1,0);
	}
}

Sprite* spriteList[10];
void initSpriteList() {
	spriteList[0] = SPR_addSprite(&bat,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
	spriteList[1] = SPR_addSprite(&cactus,0,0,TILE_ATTR(PAL3,0,FALSE,FALSE));
	spriteList[2] = SPR_addSprite(&cow,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
	spriteList[3] = SPR_addSprite(&duck,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
	spriteList[4] = SPR_addSprite(&heart,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
	spriteList[5] = SPR_addSprite(&ladybug,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
	spriteList[6] = SPR_addSprite(&strawberry,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
	spriteList[7] = SPR_addSprite(&tv,0,0,TILE_ATTR(PAL3,0,FALSE,FALSE));
	spriteList[8] = SPR_addSprite(&watermelon,0,0,TILE_ATTR(PAL3,0,FALSE,FALSE));
	spriteList[9] = SPR_addSprite(&whale,0,0,TILE_ATTR(PAL2,0,FALSE,FALSE));
}
void assignSprites() {
	humanPlayer.sprite = spriteList[humanPlayer.spriteSelect];
	u8 spriteIndex = 0; // index to increment along for loop, also incrementing to skip over whichever sprite player has.
	for (u8 i = 0; i < computerPlayers.computerCount; i++) { // logic in here 100% needs to be changed to something user configurable
		if (spriteIndex == humanPlayer.spriteSelect) {
			spriteIndex++;
		}
		computerPlayers.computers[i].sprite = spriteList[spriteIndex];
		spriteIndex++;
	}
}

void assignTiles() {
	u8 currentComputer = 0;
	humanPlayer.pathMapValue = 2 + (humanPlayer.spriteSelect*2);
	humanPlayer.claimMapValue = 3 + (humanPlayer.spriteSelect*2);
	for (u8 i = 2; i <= 3+(computerPlayers.computerCount*2); i++) {
		if (i == humanPlayer.pathMapValue) {
			continue;
		} else if (i == humanPlayer.claimMapValue) {
			continue;
		} else {
			if (i % 2 == 0) {
				computerPlayers.computers[currentComputer].pathMapValue = i;
			} else {
				computerPlayers.computers[currentComputer].claimMapValue = i;
				currentComputer++;
			}
		}
	}

}

void startGame() {
    humanPlayer.score = 0;
    assignSprites();
    assignTiles();
    SPR_setVisibility(spriteList[2],HIDDEN); // hide cute cow menu select graphic
    initMap(64,64);
    map.gameState = STATE_PLAYING;
    spawnComputerPlayers();
    spawnHumanPlayer();
    for (u8 i = 0; i < computerPlayers.computerCount; i++) { // for each computer player
        computerPlayers.computers[i].gameover = 0;
        SPR_setVisibility(computerPlayers.computers[i].sprite,VISIBLE);
    }
        SPR_setVisibility(humanPlayer.sprite,VISIBLE);
    }


u8 currentMenuSelection = 0; // this represents which menu selection, on screen, is currently selected.
void myJoyHandler(u16 joy, u16 changed, u16 state)
{
    if (joy == JOY_1) {
        if (state & BUTTON_START) {
         // player 1 pressed START button
			if (map.gameState == STATE_GAMEOVER || map.gameState == STATE_WIN) {
			    map.gameState = STATE_MENU;
			}
			else if (map.gameState == STATE_MENU) {
                if (currentMenuSelection == 0) { // play selected
                    startGame();
                }
			} else if (map.gameState == STATE_PLAYING) {
			    map.gameState = STATE_PAUSED;
			} else if (map.gameState == STATE_PAUSED) {
			    map.gameState = STATE_PLAYING;
			}
        } else if (changed & BUTTON_START) {
			
        }
        
        else if (state & BUTTON_A) {
            if (map.gameState == STATE_GAMEOVER || map.gameState == STATE_WIN) {
                // replay level. so:
                //initialize map again,
                //reset all characters,
                //set game state back to playing.
                VDP_clearTileMapRect(PLAN_A,6,7,27,2);
                initMap(64,64);
            	for (u8 i = 0; i < computerPlayers.computerCount; i++) { // for each computer player
		            computerPlayers.computers[i].gameover = 0;
		            SPR_setVisibility(computerPlayers.computers[i].sprite,VISIBLE);
		        }
		        humanPlayer.gameover = 0;
		        humanPlayer.score = 0;
		        spawnComputerPlayers();
            	spawnHumanPlayer();
            	map.gameState = STATE_PLAYING;
            } else if (map.gameState == STATE_MENU) {
                if (currentMenuSelection == 0) { // play selected
                    startGame();
                }
            }
        }
        
        else if (state & BUTTON_UP) {
            if (map.gameState == STATE_PLAYING) {
            	if (humanPlayer.direction != DIRECTION_DOWN) { // can't go up into path if going down
    	        	humanPlayer.nextDirection = 1;
    	        }
    	    } else if (map.gameState == STATE_MENU) {
    	        if (currentMenuSelection > 0) {
    	            currentMenuSelection -= 1;
    	        }
    	    }
        } else if (state & BUTTON_DOWN) {
            if (map.gameState == STATE_PLAYING) {
            	if (humanPlayer.direction != DIRECTION_UP) { // can't go down into path if going up
    	        	humanPlayer.nextDirection = 3;
    	        }
	        } else if (map.gameState == STATE_MENU) {
	            if (currentMenuSelection < 2) {
	                currentMenuSelection += 1;
	            }
	        }
	        
        } else if (state & BUTTON_LEFT) {
            if (map.gameState == STATE_PLAYING) {
            	if (humanPlayer.direction != DIRECTION_RIGHT) { // can't go left into path if going right
            		humanPlayer.nextDirection = 4;
                }
            } else if (map.gameState == STATE_MENU) {
                if (currentMenuSelection == 1) { // computer count decrease
                    if (computerPlayers.computerCount > 0) {
                        computerPlayers.computerCount -= 1;
                    }
                } else if (currentMenuSelection == 2) { // humanPlayer sprite select decrease
                    if (humanPlayer.spriteSelect > 0) {
                        humanPlayer.spriteSelect -= 1;
                        SPR_setVisibility(humanPlayer.sprite,HIDDEN);
                        assignSprites();
                        SPR_setVisibility(humanPlayer.sprite,VISIBLE);
                    }
                }
            }
        } else if (state & BUTTON_RIGHT) {
            if (map.gameState == STATE_PLAYING) {
            	if (humanPlayer.direction != DIRECTION_LEFT) { // can't go right into path if going left
            		humanPlayer.nextDirection = 2;
                }
            } else if (map.gameState == STATE_MENU) {
                if (currentMenuSelection == 1) { // computer count decrease
                    if (computerPlayers.computerCount < 9) {
                        computerPlayers.computerCount += 1;
                    }
                } else if (currentMenuSelection == 2) { // humanPlayer sprite select decrease
                    if (humanPlayer.spriteSelect < 9) {
                        humanPlayer.spriteSelect += 1;
                        SPR_setVisibility(humanPlayer.sprite,HIDDEN);
                        assignSprites();
                        SPR_setVisibility(humanPlayer.sprite,VISIBLE);
                    }
                }
            }
        }
    }
}




char computerPlayersString[1];
char humanSpriteSelectString[1];
void doMenus() {
    VDP_clearPlan(PLAN_B,1);
    VDP_clearPlan(PLAN_A,1);
    for (u8 i = 0; i < computerPlayers.computerCount; i++) { // for each computer player
		SPR_setVisibility(computerPlayers.computers[i].sprite,HIDDEN);
    }
    SPR_update();
    VDP_clearTextLine(0);
    VDP_clearTextLine(1);
    while (map.gameState == STATE_MENU) {
        VDP_drawText("Paper-io Genesis Demake",0,0);

        VDP_drawText("Play",1,3);
        VDP_drawText("Computer Players: ",1,5);
        uintToStr(computerPlayers.computerCount,computerPlayersString,0);
        VDP_drawText(computerPlayersString,19,5);
        VDP_drawText("Player Select: ",1,7);
        uintToStr(humanPlayer.spriteSelect,humanSpriteSelectString,0);
        VDP_drawText(humanSpriteSelectString,17,7);


        if (currentMenuSelection == 0) { // if Play is selected
            SPR_setPosition(humanPlayer.sprite,0,8*3);
        } else if (currentMenuSelection == 1) { // if computerPlayersCount is selected
            SPR_setPosition(humanPlayer.sprite,0,8*5);
        } else if (currentMenuSelection == 2) { // if human sprite select is selected
            SPR_setPosition(humanPlayer.sprite,0,8*7);
        }

        SPR_update();
        VDP_waitVSync();
    }
    VDP_clearPlan(PLAN_A,1);
}

u8 u8Max(u8 a, u8 b) {
    if (a > b) {
        return a;
    } else if (b > a) {
        return b;
    }
    return a; // if values are equal it will return a
}

u8 u8Min(u8 a, u8 b) {
    if (a < b) {
        return a;
    } else if (b < a) {
        return b;
    }
    return a; // if values are equal it will return a
}

u8 distX; // used as distances player is from their last safe claim, largest distance gets chosen 
u8 distY; //
u8 randomMovement;// is a direction randomly chosen for each cpu player. 
u8 isWallAbove; // used as booleans to decide which direction CPU should go based on where wall is nearby
u8 isWallLeft;  //
u8 isWallRight;  //
u8 isWallBelow; //
void computeComputersMove() { // do simple AI for all computers. maybe expand on this idea later and make distinct CPU difficulties?
    for (u8 i = 0; i < computerPlayers.computerCount; i++) { // for each computer player
        if (humanPlayer.direction != DIRECTION_NONE) { // is only DIRECTION_NONE at start of game, once human player makes a move all CPUs should start moving too to keep things synchronized.
            if (computerPlayers.computers[i].x % 8 == 0 && computerPlayers.computers[i].y % 8 == 0) { // if player at multiple (thus xy/8 is map value)
                isWallAbove = 0;
                isWallLeft = 0;
                isWallRight = 0;
                isWallBelow = 0;
                for (u8 row = (computerPlayers.computers[i].y / 8) - 1; row <= (computerPlayers.computers[i].y / 8) + 1; row++) {
                    for (u8 col = (computerPlayers.computers[i].x / 8) - 1; col <= (computerPlayers.computers[i].x / 8) + 1; col++) {
                        if (row == computerPlayers.computers[i].y / 8 || col == computerPlayers.computers[i].x / 8) { // skip entries not in a cross (diagonals nonnavigable)
                            if (getGameMapValue(row,col) == 1) {
                                // if above player, set isWallAbove to 1 (true), cant go up. if going up, pick random left/right.
                                if (row < computerPlayers.computers[i].y / 8) {
                                    isWallAbove = 1;
                                } else if (col < computerPlayers.computers[i].x / 8) {
                                // else if to left of player, isWallLeft 1. cant go left, if going left pick random up/down IF isWallAbove is 0.
                                    isWallLeft = 1;
                                } else if (col > computerPlayers.computers[i].x / 8) {
                                // else if to right of player, isWallRight 1. cant go right, if going right pick random up/down IF isWallAbove is 0.
                                    isWallRight = 1;
                                    
                                } else if (row > computerPlayers.computers[i].y / 8) {
                                // else if below player, set isWallBelow to 1. cant go down. unless map size is 3x3 (centertile non-wall, unplayable!),
                                //    this and isWallAbove won't be set together. so, if going down, pick random left/right depending on which isWallLeft/Right vals are set.
                                    isWallBelow = 1;
                                }
                                
                            }
                        }
                    }
                }

                if (isWallAbove) {
                    if (isWallLeft) {
                        if (computerPlayers.computers[i].direction == DIRECTION_UP) {
                            computerPlayers.computers[i].direction = DIRECTION_RIGHT;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_LEFT) {
                            computerPlayers.computers[i].direction = DIRECTION_DOWN;
                        }
                    } else if (isWallRight) {
                        if (computerPlayers.computers[i].direction == DIRECTION_UP) {
                            computerPlayers.computers[i].direction = DIRECTION_LEFT;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_RIGHT) {
                            computerPlayers.computers[i].direction = DIRECTION_DOWN;
                        }
                    } else {
                        if (computerPlayers.computers[i].direction == DIRECTION_UP) {
                            computerPlayers.computers[i].direction = 2 + (2*(random()%2)); // this will make either a 2 or a 4, which is right or left
                            computerPlayers.computers[i].nextDirection = DIRECTION_DOWN;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_LEFT || computerPlayers.computers[i].direction == DIRECTION_RIGHT) {
                            computerPlayers.computers[i].direction = DIRECTION_DOWN;
                        }
                    }
                    continue;
                } else if (isWallBelow) {
                    if (isWallLeft) {
                        if (computerPlayers.computers[i].direction == DIRECTION_DOWN) {
                            computerPlayers.computers[i].direction = DIRECTION_RIGHT;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_LEFT) {
                            computerPlayers.computers[i].direction = DIRECTION_UP;
                        }
                    } else if (isWallRight) {
                        if (computerPlayers.computers[i].direction == DIRECTION_DOWN) {
                            computerPlayers.computers[i].direction = DIRECTION_LEFT;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_RIGHT) {
                            computerPlayers.computers[i].direction = DIRECTION_UP;
                        }
                    } else {
                        if (computerPlayers.computers[i].direction == DIRECTION_DOWN) {
                            computerPlayers.computers[i].direction = 2 + (2*(random()%2)); // this will make either a 2 or a 4, which is right or left
                            computerPlayers.computers[i].nextDirection = DIRECTION_UP;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_LEFT || computerPlayers.computers[i].direction == DIRECTION_RIGHT) {
                            computerPlayers.computers[i].direction = DIRECTION_UP;
                        }
                    }
                    continue;
                } else {
                    if (isWallLeft) {
                        if (computerPlayers.computers[i].direction == DIRECTION_LEFT) {
                            computerPlayers.computers[i].direction = 1 + (2*(random()%2)); // this will make either a 1 or a 3, which is up or down
                            computerPlayers.computers[i].nextDirection = DIRECTION_RIGHT;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_DOWN || computerPlayers.computers[i].direction == DIRECTION_UP) {
                            computerPlayers.computers[i].direction = DIRECTION_RIGHT;
                        }
                        continue;
                    } else if (isWallRight) {
                        if (computerPlayers.computers[i].direction == DIRECTION_RIGHT) {
                            computerPlayers.computers[i].direction = 1 + (2*(random()%2)); // this will make either a 1 or a 3, which is up or down
                            computerPlayers.computers[i].nextDirection = DIRECTION_LEFT;
                        } else if (computerPlayers.computers[i].direction == DIRECTION_DOWN || computerPlayers.computers[i].direction == DIRECTION_UP) {
                            computerPlayers.computers[i].direction = DIRECTION_LEFT;
                        }
                        continue;
                    }
                    
                }
                
                if (getValueAtPlayer(computerPlayers.computers[i]) == computerPlayers.computers[i].claimMapValue) { // if in claim just move randomly
                    computerPlayers.computers[i].lastSafeX = computerPlayers.computers[i].x / 8; // this will be where RETREAT starts looking at to figure out where to go back to
                    computerPlayers.computers[i].lastSafeY = computerPlayers.computers[i].y / 8;
                    randomMovement = 1 + (random() % 4); // becomes random value between 1 and 4 inclusive.
                    computerPlayers.computers[i].stepAmount = 0; // in claim so not going anywhere, set to 0
                    computerPlayers.computers[i].state = AISTATE_SAFE; // in claim so player is safe
                    if (randomMovement == DIRECTION_UP && computerPlayers.computers[i].direction == DIRECTION_DOWN) { // cant go up if going down
                        continue;
                    } else if (randomMovement == DIRECTION_LEFT && computerPlayers.computers[i].direction == DIRECTION_RIGHT) { // cant go left if going right
                        continue;
                    } else if (randomMovement == DIRECTION_RIGHT && computerPlayers.computers[i].direction == DIRECTION_LEFT) { // cant go right if going left
                        continue;
                    } else if (randomMovement == DIRECTION_DOWN && computerPlayers.computers[i].direction == DIRECTION_UP) { // cant go down if going up
                        continue;
                    }
                    computerPlayers.computers[i].nextDirection = randomMovement;
                } else {
                    if (computerPlayers.computers[i].state == AISTATE_SAFE) {
                        computerPlayers.computers[i].state = AISTATE_FIRST;
                        computerPlayers.computers[i].stepAmount = 1 + (random() % 7);//step 1-7 spaces forward
                    } else if (computerPlayers.computers[i].state == AISTATE_FIRST) {
                        computerPlayers.computers[i].stepAmount -= 1;
                        if (computerPlayers.computers[i].stepAmount == 0) {
                            //pick direction perpendicular to current one
                            if (computerPlayers.computers[i].direction == DIRECTION_UP || computerPlayers.computers[i].direction == DIRECTION_DOWN) {
                                randomMovement = 2 + (2*(random()%2)); // this will make either a 2 or a 4, which is right or left
                                computerPlayers.computers[i].nextDirection = randomMovement;
                            } else if (computerPlayers.computers[i].direction == DIRECTION_RIGHT || computerPlayers.computers[i].direction == DIRECTION_LEFT) {
                                randomMovement = 1 + (2*(random()%2)); // this will make either a 1 or a 3, which is up or down
                                computerPlayers.computers[i].nextDirection = randomMovement;
                            }
                            computerPlayers.computers[i].state = AISTATE_SECOND;
                            computerPlayers.computers[i].stepAmount = 1 + (random() % 7);//step 1-7 spaces forward
                        }
                    } else if (computerPlayers.computers[i].state == AISTATE_SECOND) {
                        computerPlayers.computers[i].stepAmount -= 1;
                        if (computerPlayers.computers[i].stepAmount == 0) {
                            computerPlayers.computers[i].state = AISTATE_RETREAT;
                        }
                    } else if (computerPlayers.computers[i].state == AISTATE_RETREAT) {
                        distX = u8Max(computerPlayers.computers[i].x / 8 , computerPlayers.computers[i].lastSafeX) - u8Min(computerPlayers.computers[i].x / 8, computerPlayers.computers[i].lastSafeX);
                        distY = u8Max(computerPlayers.computers[i].y / 8 , computerPlayers.computers[i].lastSafeY) - u8Min(computerPlayers.computers[i].y / 8, computerPlayers.computers[i].lastSafeY);
                        if (computerPlayers.computers[i].lastSafeX < computerPlayers.computers[i].x / 8) { // if player is to the right of path start
                            //
                            if (computerPlayers.computers[i].direction == DIRECTION_RIGHT) { // if player is going right, they need to turn around...
                                if (computerPlayers.computers[i].lastSafeY < computerPlayers.computers[i].y / 8) { // if player is loewr than path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_UP;
                                } else if (computerPlayers.computers[i].lastSafeY > computerPlayers.computers[i].y / 8) { // if player is higher than path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_DOWN;
                                } 
                            } else if (computerPlayers.computers[i].direction == DIRECTION_LEFT) {
                                if (distY > distX) { // if further in Y direction, change to a Y direction
                                    if (computerPlayers.computers[i].lastSafeY < computerPlayers.computers[i].y / 8) { // if player below path start
                                        computerPlayers.computers[i].nextDirection = DIRECTION_UP;
                                    } else if (computerPlayers.computers[i].lastSafeY > computerPlayers.computers[i].y / 8) { // if player above path start
                                        computerPlayers.computers[i].nextDirection = DIRECTION_DOWN;
                                    }
                                }
                            } else if (computerPlayers.computers[i].direction == DIRECTION_UP) {
                                if (computerPlayers.computers[i].lastSafeY > computerPlayers.computers[i].y / 8) { // if player above path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_LEFT;
                                } else if (distX > distY) { // if further in X direction, change to a X direction
                                    computerPlayers.computers[i].nextDirection = DIRECTION_LEFT;
                                }
                            } else if (computerPlayers.computers[i].direction == DIRECTION_DOWN) {
                                if (computerPlayers.computers[i].lastSafeY < computerPlayers.computers[i].y / 8) { // if player below path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_LEFT;
                                } else if (distX > distY) { // if further in X direction, change to a X direction
                                    computerPlayers.computers[i].nextDirection = DIRECTION_LEFT;
                                }
                            }
                        } else if (computerPlayers.computers[i].lastSafeX > computerPlayers.computers[i].x / 8) { // if player is to the left of path start
                            //
                            if (computerPlayers.computers[i].direction == DIRECTION_LEFT) { // if player is going left, they need to turn around...
                                if (computerPlayers.computers[i].lastSafeY < computerPlayers.computers[i].y / 8) { // if player is loewr than path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_UP;
                                } else if (computerPlayers.computers[i].lastSafeY > computerPlayers.computers[i].y / 8) { // if player is higher than path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_DOWN;
                                } 
                            } else if (computerPlayers.computers[i].direction == DIRECTION_RIGHT) {
                                if (distY > distX) { // if further in Y direction, change to a Y direction
                                    if (computerPlayers.computers[i].lastSafeY < computerPlayers.computers[i].y / 8) { // if player below path start
                                        computerPlayers.computers[i].nextDirection = DIRECTION_UP;
                                    } else if (computerPlayers.computers[i].lastSafeY > computerPlayers.computers[i].y / 8) { // if player above path start
                                        computerPlayers.computers[i].nextDirection = DIRECTION_DOWN;
                                    }
                                }
                            } else if (computerPlayers.computers[i].direction == DIRECTION_UP) {
                                if (computerPlayers.computers[i].lastSafeY > computerPlayers.computers[i].y / 8) { // if player above path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_RIGHT;
                                } else if (distX > distY) { // if further in X direction, change to a X direction
                                    computerPlayers.computers[i].nextDirection = DIRECTION_RIGHT;
                                }
                            } else if (computerPlayers.computers[i].direction == DIRECTION_DOWN) {
                                if (computerPlayers.computers[i].lastSafeY < computerPlayers.computers[i].y / 8) { // if player below path start
                                    computerPlayers.computers[i].nextDirection = DIRECTION_RIGHT;
                                } else if (distX > distY) { // if further in X direction, change to a X direction
                                    computerPlayers.computers[i].nextDirection = DIRECTION_RIGHT;
                                }
                            }
                        } 
                    }
                }
            }
        }
    }
}

int main()
{
	VDP_setPlanSize(64,64); //hard set 64 since early testing, later make variable and user-settable/influencable

	VDP_setWindowAddress(0xB000); // borrowed from lines 114-123 from sgdk vdp.c
	VDP_setSpriteListAddress(0xBC00); // fixed bug where setting 64x64 and using PLAN_B had glitches: was related to these memory addresses having been set incorrectly
	VDP_setHScrollTableAddress(0xB800);
	VDP_setBPlanAddress(0xC000);
	VDP_setAPlanAddress(0xE000);

    
	loadTiles();

	computerPlayers.computerCount = 9; // only have 9 sprites right now.
	
	VDP_setPalette(PAL0,batpath.palette->data);
	VDP_setPalette(PAL1,cactuspath.palette->data); // these will be wrong until palettes are manually moved in images
	VDP_setPalette(PAL2,strawberry.palette->data);
	VDP_setPalette(PAL3,cactus.palette->data);
	
	VDP_setBackgroundColor(2);

	

	SPR_init(0,0,0);
	humanPlayer.spriteSelect = 9; // change later. forcing index 6 (strawberry) until user can change it.

	initSpriteList();
	assignSprites();

	assignTiles();

	////// INIT MAP
	initMap(64,64); // set active data range of map to 64x64., reuse  to let user alter level setting later.
        
    JOY_init();
    JOY_setEventHandler( &myJoyHandler);    

	spawnComputerPlayers();
	spawnHumanPlayer();
	char scoreString[10];
    
    map.gameState = STATE_MENU;
    doMenus();
    VDP_drawText("Score:",23,0);
    while (1)
    {
        //read input
        //move sprite
        //update score
        //draw current screen (logo, start screen, settings, game, gameover, credits..)


        VDP_showFPS(0);
        VDP_drawText("FPS",3,1);
        
		VDP_setHorizontalScroll(PLAN_B,camera.x);
		VDP_setVerticalScroll(PLAN_B,camera.y);
		uintToStr(humanPlayer.score,scoreString,0);
        if (map.gameState == STATE_PLAYING) {
            computeComputersMove();
            movePlayers();
		    adjustCamera();
		    checkPlayers();
		    drawPlayers();
    		SPR_update();
        } else if (map.gameState == STATE_MENU) {
            // drawMenu
            // ... what else?.. will joy handler update menu values, leaving this the only necessary method?
            doMenus();
            VDP_drawText("Score:",23,0);
        } else if (map.gameState == STATE_PAUSED) {
            // do nothing until not paused.
            while (map.gameState == STATE_PAUSED) {
                VDP_waitVSync();
            }
        } else if (map.gameState == STATE_WIN) {
            winHumanPlayer();
        }
        VDP_drawText("          ",29,0);
		VDP_drawText(scoreString,29,0);
        VDP_waitVSync();
    }
    return 0;
}

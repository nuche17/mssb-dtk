#define SQRT2_LINKAGE static
#include "game/minigame/wall_ball.h"
#include "game/UnknownHomes_Game.h"
#include "header_rep_data.h"
#include "Dolphin/stl.h"
#include "Dolphin/rand.h"
#include "stl/math.h"
#include "game/sound/m_sound.h"
#include "game/math/game_math.h"
#include "static/UnknownHomes_Static.h"

// Shared minigame task-list sort/comparator helper (DOL-side, already used
// this way by scene_effects.c: in-place sort of `count` `size`-byte elements
// at `base`/`scratch` using `cmp` as the comparator).
extern void fn_800246D4(void* cmp, void* base, void* scratch, int size, int count);
// Shared minigame "drop wall ball note block" effect trigger, already used
// with this signature by barrel_batter.c (same minigame-effects helper).
extern void fn_8004C108(VecXYZ* pos, BOOL flag);
// Elsewhere in the module (symbols.txt .text:0x001500C8); not yet in a header.
extern void wallBall_updateSomePointers(void);

extern f32 lbl_3_data_21500[4][2];
extern VecXYZ lbl_3_data_21520[2][7];
extern VecXYZ lbl_3_data_215C8[2];
extern VecXYZ lbl_3_data_215E0[7];
extern f32 lbl_3_data_21634[5];
extern int wallBall_nCoinsToGenerate[3];
extern s16 lbl_3_data_21654[12];
extern f32 lbl_3_data_21674[2];
extern f32 lbl_3_data_219B8[19];
extern s16 lbl_3_data_2167C[6];
extern f32 lbl_3_data_21688[3];
extern s8 lbl_3_data_21694[4][7];
extern s8 lbl_3_data_216B0[4][2];
extern s8 lbl_3_data_216B8[4];

// AI throw-power tuning tables, Ghidra-named by RAM address -- not yet
// cross-checked against raw asm byte offsets (first pass draft; see checkpoint).
typedef struct {
    s8 randomBounds[4][2];
    s8 perfectOverchargeProb[4];
} WallBallConstants2;
extern WallBallConstants2 wallBallConstants2_807d2c88;
extern s8 wallBallAIPitches_oddsTable[]; // unresolved relocation in target; address unknown

typedef struct {
    f32 wallBreakOffset;
    f32 pitchVeloMult;
    s16 rotatePitchersMaxFrames;
} WallBallConstants1;
extern WallBallConstants1 wallBallConstants1_807d2c74;

extern s16 SHORT_ARRAY_807d2c5c[8];   // wall "points remaining" value by coinGenerationCategory
extern s16 SHORT_ARRAY_807d2c62[8];   // points awarded by coinGenerationCategory
extern VecXYZ FLOAT_ARRAY_ARRAY_807d2be0[8]; // coin spawn position by coinRelated[0] category
extern u8 FLOAT_ARRAY_ARRAY_807d2b08[0x60];  // wall target position table, byte-indexed (unverified)
extern f32 FLOAT_807d2b74[8];
extern f32 FLOAT_807d2b78[8];
extern f32 FLOAT_807d2b7c[8];
extern f32 FLOAT_807d2bc8;
extern f32 FLOAT_807d2b00[8];
extern f32 FLOAT_807d2b04[8];
extern VecXYZ fieldingStartingCoords_regular[6]; // symbols.txt data:0x450C size:0x48

// .text:0x00114A88 size:0x538 mapped:0x80753B1C
void wallBallRotatePitchers(int rotateAll) {
    MiniGameStruct* mg = &g_Minigame;
    int i;
    f32 zPos;
    f32 xPos, zPos2;
    f32 waitX, waitZ;
    u8 fielderIdx;
    int offset;
    f32 walkingSpeed;
    f32 dx, dz, speedSq;

    zPos = mg->wallBallSomeZPos;
    mg->wallBallPitcherRotationCounter++;

    if (mg->wallBallPitcherRotationCounter == 1 || rotateAll != 0) {
        if (mg->soloMinigameDifficulty != MINIGAME_DIFFICULTY_SOLO_NON_CHALLENGE || mg->multiPlayerInd != 0) {
            fielderIdx = mg->minigameFielderIndex[(s8)mg->minigameControlStruct[0].aIStrength[(mg->turnNumberWithinRound + 3 & 3) + 4]];
            g_Fielders[fielderIdx].pos.x = mg->wallBallSomeXPos;
            g_Fielders[fielderIdx].pos.z = zPos;
        }

        xPos = fieldingStartingCoords_regular[0].x;
        zPos2 = fieldingStartingCoords_regular[0].z;
        for (i = 0; i < 4; i++) {
            if (mg->minigameControlStruct[0].characterIndex[i] >= CHAR_ID_NONE + 1) {
                offset = i - mg->turnNumberWithinRound;
                if (offset < 0) {
                    offset += mg->miniGameNumberOfParticipants;
                }
                if (offset == 0) {
                    mg->wallBallWaitingLocations[mg->minigameControlStruct[0].aIStrength[4]][0] = xPos;
                    mg->wallBallWaitingLocations[mg->minigameControlStruct[0].aIStrength[4]][1] = zPos2;
                } else {
                    mg->wallBallWaitingLocations[mg->minigameControlStruct[0].aIStrength[4]][0] = FLOAT_807d2b00[offset * 2];
                    mg->wallBallWaitingLocations[mg->minigameControlStruct[0].aIStrength[4]][1] = FLOAT_807d2b04[offset * 2];
                }
            }
        }

        if (rotateAll != 0) {
            int n = mg->miniGameNumberOfParticipants;
            if (n == 0) {
                return;
            }
            for (i = 0; i < n; i++) {
                fielderIdx = mg->minigameFielderIndex[i];
                g_Fielders[fielderIdx].pos.x = mg->wallBallWaitingLocations[i][0];
                g_Fielders[fielderIdx].pos.y = 0.0f;
                g_Fielders[fielderIdx].pos.z = mg->wallBallWaitingLocations[i][1];
            }
            return;
        }
    }

    if ((int)mg->wallBallPitcherRotationCounter < wallBallConstants1_807d2c74.rotatePitchersMaxFrames) {
        walkingSpeed = 1.0f / (f32)(wallBallConstants1_807d2c74.rotatePitchersMaxFrames - (int)mg->wallBallPitcherRotationCounter);

        for (i = 0; i < 4; i++) {
            if (mg->minigameControlStruct[0].characterIndex[i] >= CHAR_ID_NONE + 1) {
                fielderIdx = mg->minigameFielderIndex[i];
                dx = (mg->wallBallWaitingLocations[i][0] - g_Fielders[fielderIdx].pos.x) * walkingSpeed;
                dz = (mg->wallBallWaitingLocations[i][1] - g_Fielders[fielderIdx].pos.z) * walkingSpeed;
                g_Fielders[fielderIdx].velocityX = dx;
                g_Fielders[fielderIdx].velocityZ = dz;
                speedSq = dx * dx + dz * dz;
                g_Fielders[fielderIdx].pos.x += dx;
                g_Fielders[fielderIdx].pos.z += dz;
                g_Fielders[fielderIdx].currentVelocity = dolsqrtf2(speedSq);
                if (g_Fielders[fielderIdx].currentVelocity > 0.0f) {
                    g_Fielders[fielderIdx].xMovementDir = dx / g_Fielders[fielderIdx].currentVelocity;
                    g_Fielders[fielderIdx].zMovementDir = dx / g_Fielders[fielderIdx].currentVelocity;
                }
            }
        }
    } else {
        /* Timer reached the max; stop moving the pitchers. */
        mg->wallBallRotatePitchersInd = 0;
        for (i = 0; i < 4; i++) {
            if (mg->minigameControlStruct[0].characterIndex[i] >= CHAR_ID_NONE + 1) {
                g_Fielders[mg->minigameFielderIndex[i]].currentVelocity = 0.0f;
            }
        }
    }
}

// .text:0x00114A2C size:0x5C mapped:0x80753AC0
void fn_3_114A2C(void) {
    if (g_Minigame.wallBallGameState == WALL_BALL_GAME_STATE_CALCULATE_NEW_WALLS) {
        wallBallCalculateNewWalls();
        wallBall_updateSomePointers();
    } else if (g_Minigame.wallBallGameState == WALL_BALL_GAME_STATE_DROP_IN_NEW_WALLS) {
        wallBallDropInNewWalls();
    } else if (g_Minigame.wallBallGameState == WALL_BALL_GAME_STATE_PITCH_OR_WAITING_FOR_PITCH) {
        wallBallSomething2();
        fn_3_113D20();
    }
}

// .text:0x001149B8 size:0x74 mapped:0x80753A4C
// Not a normal call target -- passed by address to fn_800246D4 as the
// comparator used to sort g_Minigame.wallIndexTracker[7] in
// wallBallCalculateNewWalls (walls with wallState==3 "ready" sort first;
// ties keep their original setupOrderIndex).
int fn_3_1149B8(u8* a, u8* b) {
    WallBallWallStruct* wallA = &g_Minigame.wallBallWalls[*a];
    WallBallWallStruct* wallB = &g_Minigame.wallBallWalls[*b];

    if (wallA->wallState == 3 && wallB->wallState != 3) {
        return -1;
    }
    if (wallA->wallState != 3 && wallB->wallState == 3) {
        return 1;
    }
    return wallA->setupOrderIndex - wallB->setupOrderIndex;
}

// .text:0x00114384 size:0x634 mapped:0x80753418
void wallBallCalculateNewWalls(void) {
    MiniGameStruct* mg = &g_Minigame;
    int difficultyLevel;
    int i, k;
    u8 sortedWallIdx;
    int category;
    int nSpecial;
    int remaining;
    int countByCategory[3];
    u8 candidateSlots[7];
    int nCandidates;
    int slot;
    int j;

    if (mg->multiPlayerInd == 0 && mg->_1A3C == 0 && mg->soloMinigameDifficulty == MINIGAME_DIFFICULTY_SOLO_NON_CHALLENGE) {
        if (mg->miniGameTurnCounter < 6) {
            difficultyLevel = 1;
        } else if (mg->miniGameTurnCounter < 11) {
            difficultyLevel = 2;
        } else if (mg->miniGameTurnCounter < 16) {
            difficultyLevel = 3;
        } else {
            difficultyLevel = 4;
        }
    } else if (g_Scores.Inning == 2) {
        difficultyLevel = 2;
    } else if (g_Scores.Inning < 2 && g_Scores.Inning > 0) {
        difficultyLevel = 1;
    } else {
        difficultyLevel = 3;
    }

    fn_800246D4(fn_3_1149B8, mg->wallIndexTracker, mg->wallIndexTracker, 1, 7);

    for (i = 0; i < 7; i++) {
        mg->wallBallWalls[mg->wallIndexTracker[i]].setupOrderIndex = (u8)i;
    }

    countByCategory[0] = countByCategory[1] = countByCategory[2] = 0;

    nSpecial = 0;
    for (i = 0; i < 7; i++) {
        WallBallWallStruct* wall = &mg->wallBallWalls[mg->wallIndexTracker[i]];
        if (wall->wallState == 3) {
            nSpecial++;
            countByCategory[wall->coinGenerationCategory]++;
        }
    }

    for (i = 0; i < 7; i++) {
        WallBallWallStruct* wall = &mg->wallBallWalls[mg->wallIndexTracker[i]];
        category = wall->coinGenerationCategory;
        wall->targetX = FLOAT_ARRAY_ARRAY_807d2be0[category].x;
        wall->targetY = FLOAT_ARRAY_ARRAY_807d2be0[category].y;
        wall->targetZ = FLOAT_ARRAY_ARRAY_807d2be0[category].z;

        if (i < nSpecial) {
            wall->wallState = 2;
        } else {
            wall->xPos = wall->targetX + FLOAT_807d2b74[category * 3];
            wall->yPos = wall->targetY + FLOAT_807d2b78[category * 3];
            wall->zPositionOfSomeWall = wall->targetZ + FLOAT_807d2b7c[category * 3];
            wall->wallState = 1;
            wall->coinGenerationCategory = 1;
            category = wall->coinGenerationCategory;
            wall->wallPower = SHORT_ARRAY_807d2c5c[category];
            countByCategory[category]++;
        }

        wall->_26 = 0;
        wall->wobbleAngle = 0.0f;
        wall->wobbleAngleVelocity = 0.0f;
        wall->wobbleAmplitude = 0.0f;
    }

    if (nSpecial < 7) {
        remaining = 7 - nSpecial;
        if (countByCategory[0] == 0) {
            /* Pick one more wall to become the special "note block" wall,
               preferring a wall slot not already used as a special wall
               last round (mg->wallBallSpecialWallPos history of 2). */
            nCandidates = 0;
            for (k = nSpecial; k < 7; k++) {
                int matches = 0;
                for (j = 0; j < 2; j++) {
                    if ((u8)k == mg->wallBallSpecialWallPos) {
                        matches++;
                    }
                }
                if (matches <= 1) {
                    candidateSlots[nCandidates++] = (u8)k;
                }
            }
            if (nCandidates == 0) {
                slot = RandomInt_Game_Range(nSpecial, 6);
            } else {
                slot = candidateSlots[RandomInt_Game(nCandidates)];
            }
            {
                WallBallWallStruct* wall = &mg->wallBallWalls[mg->wallIndexTracker[slot]];
                remaining--;
                wall->coinGenerationCategory = 2;
                wall->wallPower = SHORT_ARRAY_807d2c5c[wall->coinGenerationCategory];
            }
        }

        if ((u32)difficultyLevel < (u32)remaining) {
            /* Thin out the remaining "normal" walls down to difficultyLevel
               by randomly flipping some back to category 0. */
            int candidates[7];
            int nCand = 0;
            for (k = nSpecial; k < 7; k++) {
                WallBallWallStruct* wPrev = (k > 0) ? &mg->wallBallWalls[mg->wallIndexTracker[k - 1]] : NULL;
                WallBallWallStruct* wNext = (k < 6) ? &mg->wallBallWalls[mg->wallIndexTracker[k + 1]] : NULL;
                WallBallWallStruct* wThis = &mg->wallBallWalls[mg->wallIndexTracker[k]];
                if (wThis->wallState != 2 &&
                    (k == nSpecial || wPrev == NULL || wPrev->wallState != 2) &&
                    (k == 6 || difficultyLevel < 2 || wNext == NULL || wNext->wallState != 2)) {
                    candidates[nCand++] = k;
                }
            }
            while (nCand != 0 && (u32)remaining > (u32)difficultyLevel) {
                int pick = RandomInt_Game(nCand);
                int slotIdx = candidates[pick];
                WallBallWallStruct* wall = &mg->wallBallWalls[mg->wallIndexTracker[slotIdx]];
                remaining--;
                wall->coinGenerationCategory = 0;
                wall->wallPower = SHORT_ARRAY_807d2c5c[wall->coinGenerationCategory];
                for (j = pick; j < nCand - 1; j++) {
                    candidates[j] = candidates[j + 1];
                }
                nCand--;
            }
        }
    }

    mg->wallBallWalls[mg->wallIndexTracker[0]]._26 = mg->wallBallSpecialWallPos;

    for (i = 0; i < 7; i++) {
        if (mg->wallBallWalls[mg->wallIndexTracker[i]].wallState == 2) {
            mg->wallBallSpecialWallPos = (u8)i;
            mg->wallBallGameState = WALL_BALL_GAME_STATE_DROP_IN_NEW_WALLS;
            mg->_1A80 = 7;
            return;
        }
    }

    mg->wallBallGameState = WALL_BALL_GAME_STATE_DROP_IN_NEW_WALLS;
    mg->_1A80 = 7;
}

// .text:0x00114204 size:0x180 mapped:0x80753298
void wallBallDropInNewWalls(void) {
    int i;
    int nMoving;
    WallBallWallStruct* wall;

    nMoving = 0;
    for (i = 0; i < 7; i++) {
        wall = &g_Minigame.wallBallWalls[i];
        if (wall->wallState == 2) {
            wall->xPos += lbl_3_data_215C8[1].x;
            wall->yPos += lbl_3_data_215C8[1].y;
            wall->zPositionOfSomeWall += lbl_3_data_215C8[1].z;
            if (wall->zPositionOfSomeWall >= wall->targetZ) {
                wall->zPositionOfSomeWall = wall->targetZ;
                wall->wallState = 3;
            }
            nMoving++;
        }
    }

    if (nMoving == 0) {
        for (i = 0; i < 7; i++) {
            wall = &g_Minigame.wallBallWalls[i];
            if (wall->wallState == 1) {
                wall->xPos += lbl_3_data_215C8[0].x;
                wall->yPos += lbl_3_data_215C8[0].y;
                wall->zPositionOfSomeWall += lbl_3_data_215C8[0].z;
                if (wall->yPos <= wall->targetY) {
                    wall->yPos = wall->targetY;
                    wall->wallState = 3;
                    fn_8004C108((VecXYZ*)&wall->targetX, TRUE);
                    callSfx(0x2f4);
                }
                nMoving++;
            }
        }
        if (nMoving == 0) {
            g_Minigame.wallBallGameState = WALL_BALL_GAME_STATE_PITCH_OR_WAITING_FOR_PITCH;
        }
    }
}

// .text:0x00113EC0 size:0x54 mapped:0x80752F54
void fn_3_113EC0(void) {
    g_Pitcher.ballVelocity.z = -g_Pitcher.ballVelocity.z;
    g_Pitcher.ballVelocity.x *= lbl_3_data_21674[0];
    g_Pitcher.ballVelocity.y *= lbl_3_data_21674[0];
    g_Pitcher.ballVelocity.z *= lbl_3_data_21674[0];
    g_Minigame.ballStoppedBreakingWallsInd = TRUE;
}

// .text:0x00113D20 size:0x1A0 mapped:0x80752DB4
// "Wobbling note block" animation: once a wall's wobble angle/velocity is
// disturbed, oscillate it with a cosine spring and let it settle back to
// zero once its amplitude decays below the wobble speed.
//
// The target keeps this as a real call from its one call site
// (fn_3_114A2C); -inline auto would otherwise fold this single-call-site
// function in, so pin it with dont_inline.
#pragma dont_inline on
void fn_3_113D20(void) {
    WallBallWallStruct* wall;
    int i;
    f32 oldAngle;
    f32 newAngle;
    s8 dir;
    s8 crossedZero = FALSE;

    for (i = 0; i < 7; i++) {
        wall = &g_Minigame.wallBallWalls[i];
        oldAngle = wall->wobbleAngle;
        if (oldAngle != 0.0f || wall->wobbleAngleVelocity != 0.0f) {
            wall->wobbleAngle = wall->wobbleAngleVelocity * cos(oldAngle) + wall->wobbleAngle;
            newAngle = wall->wobbleAngle;
            if (newAngle >= 0.0f) {
                dir = -1;
            } else {
                dir = 1;
            }

            if (oldAngle < newAngle) {
                if (oldAngle < 0.0f && newAngle >= 0.0f) {
                    crossedZero = TRUE;
                }
            } else {
                if (oldAngle > 0.0f && newAngle <= 0.0f) {
                    crossedZero = TRUE;
                }
            }

            if (crossedZero) {
                wall->wobbleAngleVelocity *= lbl_3_data_21688[2];
                if (fabsf(wall->wobbleAngleVelocity) < wall->wobbleAmplitude * lbl_3_data_21688[2]) {
                    wall->wobbleAngle = wall->wobbleAngleVelocity = wall->wobbleAmplitude = 0.0f;
                }
                crossedZero = FALSE;
            }

            wall->wobbleAngleVelocity = dir * wall->wobbleAmplitude + wall->wobbleAngleVelocity;
        }
    }
}
#pragma dont_inline reset

// .text:0x00113A48 size:0x2D8 mapped:0x80752ADC
void wallBallCalc_WallsBroken(void) {
    MiniGameStruct* mg = &g_Minigame;
    int i;
    int nWallsBroken;
    int remainingPower;
    int firstUnbrokenIdx;

    if (g_Ball.pitchHangtimeCounter != 1) {
        return;
    }

    mg->wallBallPitchPower = 10; /* const_10WallBallMinPower */
    if (g_Pitcher.ChargePitchType == 3) { /* Perfect */
        mg->wallBallPitchPower = 150;
    } else if (g_Pitcher.ChargePitchType > 1) { /* startingCharge/chargedCaptainStar */
        f32 lerp = LinearInterpolateToNewRange(g_Pitcher.pitchChargeUp, 0.0f, 1.0f, 30.0f, 123.0f);
        mg->wallBallPitchPower = (s16)(int)lerp;
    }

    mg->wallBallPitchPowerRemaining = mg->wallBallPitchPower;

    firstUnbrokenIdx = 7;
    remainingPower = 0;
    for (i = 0; i < 7; i++) {
        WallBallWallStruct* wall = &mg->wallBallWalls[mg->wallIndexTracker[i]];
        remainingPower += wall->wallPower;
        if (wall->wallState == 2) {
            firstUnbrokenIdx = i;
            break;
        }
    }

    if (remainingPower <= mg->wallBallPitchPower) {
        if (firstUnbrokenIdx < 6) {
            WallBallWallStruct* nextWall = &mg->wallBallWalls[mg->wallIndexTracker[firstUnbrokenIdx + 1]];
            if ((int)mg->wallBallPitchPower <= remainingPower + nextWall->wallPower - 1) {
                mg->wallBall_hitNoteBlock = 1;
            }
        } else {
            mg->wallBall_hitNoteBlock = 1;
        }
    }

    if (mg->wallBall_hitNoteBlock != 1) {
        s16 power;
        nWallsBroken = 0;
        power = mg->wallBallPitchPower - mg->wallBallWalls[mg->wallIndexTracker[0]].wallPower;
        for (i = 0; i < 6; i++) {
            if (power < 0) {
                break;
            }
            nWallsBroken = i + 1;
            power -= mg->wallBallWalls[mg->wallIndexTracker[i + 1]].wallPower;
        }
        if (nWallsBroken == 7 || (nWallsBroken == 6 && power >= 0)) {
            nWallsBroken = 7;
        }

        if (nWallsBroken - 1 >= 0 &&
            mg->wallBallWalls[mg->wallIndexTracker[nWallsBroken - 1]].coinGenerationCategory == 1) {
            mg->_1A8B = (u8)(nWallsBroken - 1);
            mg->wallBall_hitBowserWall = 1;
        }
    }
}

// .text:0x00113950 size:0xF8 mapped:0x807529E4
// Per-frame coin physics: fall under gravity, bounce off the floor, and
// despawn after lbl_3_data_2167C[4] frames visible.
void fn_3_113950(void) {
    int i;

    for (i = 0; i < 100; i++) {
        if (g_Minigame.wallBall_coinsVisibleInd[i]) {
            g_Minigame.wallBall_coinsVisibleFrameCounter[i]++;

            g_Minigame.wallBall_coinCoordinates[i].x += g_Minigame.wallBall_coinVelocity[i].x;
            g_Minigame.wallBall_coinCoordinates[i].y += g_Minigame.wallBall_coinVelocity[i].y;
            g_Minigame.wallBall_coinCoordinates[i].z += g_Minigame.wallBall_coinVelocity[i].z;

            g_Minigame.wallBall_coinVelocity[i].y -= lbl_3_data_21634[2];

            if (g_Minigame.wallBall_coinCoordinates[i].y < lbl_3_data_219B8[14]) {
                g_Minigame.wallBall_coinCoordinates[i].y = lbl_3_data_219B8[14];
                g_Minigame.wallBall_coinVelocity[i].y = -g_Minigame.wallBall_coinVelocity[i].y * lbl_3_data_21634[3];
                g_Minigame.wallBall_coinVelocity[i].x *= lbl_3_data_21634[4];
                g_Minigame.wallBall_coinVelocity[i].z *= lbl_3_data_21634[4];
            }

            if (g_Minigame.wallBall_coinsVisibleFrameCounter[i] > lbl_3_data_2167C[4]) {
                g_Minigame.wallBall_coinsVisibleInd[i] = FALSE;
            }
        }
    }
}

// .text:0x0011391C size:0x34 mapped:0x807529B0
void fn_3_11391C(void) {
    memset(&g_Minigame._1D7C, 0, 0x78);
}

// .text:0x001136FC size:0x220 mapped:0x80752790
void wallBallAIPitches(void) {
    MiniGameStruct* mg = &g_Minigame;
    int i;
    int aiStrength;
    s16 powerUpper, powerLower;
    int lastWallIdx;
    int category;
    int rng;

    aiStrength = (int)(s8)mg->minigameControlStruct[0].aIStrength[mg->minigamePlayerSelectedOrder];

    powerUpper = 0;
    powerLower = 0x7fff;
    lastWallIdx = 0;
    for (i = 0; i < 7; i++) {
        WallBallWallStruct* wall = &mg->wallBallWalls[mg->wallIndexTracker[i]];
        powerUpper += wall->wallPower;
        lastWallIdx = i;
        if (wall->coinGenerationCategory == 2) {
            break;
        }
    }

    if (lastWallIdx + 1 < 7) {
        WallBallWallStruct* nextWall = &mg->wallBallWalls[mg->wallIndexTracker[lastWallIdx + 1]];
        powerLower = powerUpper + nextWall->wallPower - 1;
    }

    rng = RandomInt_Game(100);
    if (rng < wallBallAIPitches_oddsTable[aiStrength]) {
        s16 bonus = RandomInt_Game_Range(wallBallConstants2_807d2c88.randomBounds[aiStrength][0],
                                          wallBallConstants2_807d2c88.randomBounds[aiStrength][1]);
        powerUpper += bonus;
        powerLower += bonus;
    }

    if (powerUpper <= 150 && powerLower >= 150) {
        mg->ai_wbThrowType_bbVertAngle = 0; /* perfect */
    } else if (powerUpper <= 10 && powerLower >= 10) {
        mg->ai_wbThrowType_bbVertAngle = 3; /* curveBall */
    } else if (powerLower < 30 || powerUpper > 123) {
        mg->ai_wbThrowType_bbVertAngle = 1; /* overcharge */
    } else {
        mg->ai_wbThrowType_bbVertAngle = 2; /* charge */
        if (powerUpper < 30) {
            powerUpper = 30;
        }
        if (powerLower > 123) {
            powerLower = 123;
        }
        mg->ai_wbChargePower_bbSwingFrame = (s16)RandomInt_Game_Range(powerUpper, powerLower);
    }

    /* If they got perfect, there's still a chance to overcharge depending on difficulty. */
    if (mg->ai_wbThrowType_bbVertAngle == 0 &&
        RandomInt_Game(100) < wallBallConstants2_807d2c88.perfectOverchargeProb[aiStrength]) {
        mg->ai_wbThrowType_bbVertAngle = 1; /* overcharge */
    }
}

// .text:0x00113F14 size:0x2F0 mapped:0x80752FA8
void wallBallSomething2(void) {
    WallBallWallStruct* wall;
    int i;
    int j;
    u8 nCoins;
    f32 angle;

    if (g_Ball.pitchHangtimeCounter < 1) {
        return;
    }

    for (i = 0; i < 7; i++) {
        wall = &g_Minigame.wallBallWalls[i];
        if (wall->wallState == 3 && g_Ball.AtBat_Contact_BallPos.z - g_Ball.groundYForBounces - lbl_3_data_21674[1] <= wall->zPositionOfSomeWall && !g_Minigame.ballStoppedBreakingWallsInd) {
            MiniGameStruct* mg = &g_Minigame;
            wall->wallPower -= mg->wallBallPitchPowerRemaining;
            if (wall->wallPower <= 0) {
                mg->wallBallPitchPowerRemaining = -wall->wallPower;
                wall->wallState = 4;
                mg->miniGameLatestPoints[(s8)mg->minigamePlayerSelectedOrder] += lbl_3_data_21654[wall->coinGenerationCategory + 7];
                nCoins = wallBall_nCoinsToGenerate[wall->coinGenerationCategory];
                for (j = 0; j < 100; j++) {
                    if (!g_Minigame.wallBall_coinsVisibleInd[j]) {
                        g_Minigame.wallBall_coinsVisibleFrameCounter[j] = 0;
                        g_Minigame.wallBall_coinsVisibleInd[j] = TRUE;
                        g_Minigame.wallBall_coinCoordinates[j].x = lbl_3_data_215E0[wall->setupOrderIndex].x;
                        g_Minigame.wallBall_coinCoordinates[j].y = lbl_3_data_215E0[wall->setupOrderIndex].y;
                        g_Minigame.wallBall_coinCoordinates[j].z = lbl_3_data_215E0[wall->setupOrderIndex].z;
                        angle = 0.017453292f * (rand() % 180);
                        g_Minigame.wallBall_coinVelocity[j].x = lbl_3_data_21634[1] * (f32)cos(angle);
                        g_Minigame.wallBall_coinVelocity[j].z = lbl_3_data_21634[1] * (f32)sin(angle);
                        g_Minigame.wallBall_coinVelocity[j].y = lbl_3_data_21634[0];
                        if (--nCoins == 0) {
                            break;
                        }
                    }
                }
                setCharacterAnimations(g_Minigame.minigameControlStruct[0].characterIndex[(s8)mg->minigamePlayerSelectedOrder], 0);
            } else {
                wall->wobbleAngle = 0.0f;
                wall->wobbleAngleVelocity = lbl_3_data_21688[0];
                wall->wobbleAmplitude = lbl_3_data_21688[1];
                callSfx(0x2e1);
                fn_3_113EC0();
            }
            return;
        }
    }
}

// .text:0x001133C4 size:0x338 mapped:0x80752458
// wallBallAISwitchVar is a 10-state (0-9) machine driving one AI-controlled
// batter's wall-ball pitch: 0=choose pitch, 1=wait out the windup countdown,
// then a pair of states per throw type (hold button / release) for each of
// perfect(2,3) overcharge(4,5) charge(6,7) curveBall(8), ending at 9 (idle).
void wallBallMultiplayer_AIControl(void) {
    MiniGameStruct* mg = &g_Minigame;
    int i;
    s8 charIdx;
    int port;

    for (i = 0; i < 4; i++) {
        mg->portOfAIBeingProcessed[i] = FALSE;
    }

    for (port = 0; port < 4; port++) {
        charIdx = mg->minigameControlStruct[0].characterIndex[port];
        if (charIdx >= CHAR_ID_NONE + 1 && charIdx < 4 &&
            port == mg->minigamePlayerSelectedOrder &&
            mg->minigameControlStruct[0].battingHandedness[port] != 0) {
            mg->portOfAIBeingProcessed[charIdx] = TRUE;
            memset(&mg->_1D7C[charIdx], 0, 0x10);

            switch (mg->wallBallAISwitchVar) {
            case 0:
                wallBallAIPitches();
                *(u8*)&mg->minigameAICountDownTillAction = 0x3C;
                mg->wallBallAISwitchVar = 1;
                break;
            case 1:
                *(u8*)&mg->minigameAICountDownTillAction = *(u8*)&mg->minigameAICountDownTillAction - 1;
                if ((s8)*(u8*)&mg->minigameAICountDownTillAction < 1) {
                    if (mg->ai_wbThrowType_bbVertAngle == 0) { /* perfect */
                        mg->wallBallAISwitchVar = 2;
                    } else if (mg->ai_wbThrowType_bbVertAngle < 2) { /* overcharge==1 */
                        mg->wallBallAISwitchVar = 4;
                    } else if (mg->ai_wbThrowType_bbVertAngle == 2) { /* charge */
                        mg->wallBallAISwitchVar = 6;
                    } else { /* curveBall==3 */
                        mg->wallBallAISwitchVar = 8;
                    }
                }
                break;
            case 2: /* perfect: press A */
                mg->_1D7C[charIdx].newButtonInput |= INPUT_BUTTON_A;
                mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                mg->wallBallAISwitchVar = 3;
                break;
            case 3: /* perfect: hold A until the release window */
                if (g_Pitcher.windupCountdownUntilBallReleased < 15) {
                    mg->wallBallAISwitchVar = 9;
                } else {
                    mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                }
                break;
            case 4: /* overcharge: press A */
                mg->_1D7C[charIdx].newButtonInput |= INPUT_BUTTON_A;
                mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                mg->wallBallAISwitchVar = 5;
                break;
            case 5: /* overcharge: hold A (released at the top-level countdown) */
                mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                break;
            case 6: /* charge: press A */
                mg->_1D7C[charIdx].newButtonInput |= INPUT_BUTTON_A;
                mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                mg->wallBallAISwitchVar = 7;
                break;
            case 7: /* charge: hold A until the AI's target charge power is reached */
                if (g_Pitcher.framesAHeldForChargePitches == 0) {
                    f64 windupFrac = 1.0 - (f64)g_Pitcher.windupCountdownUntilBallReleased / (f64)g_Pitcher.pitchWindUpCountDown;
                    f64 target = LinearInterpolateToNewRange(windupFrac, 0.0, 1.0, 30.0, 123.0);
                    if (mg->ai_wbChargePower_bbSwingFrame <= (s16)(int)target) {
                        mg->wallBallAISwitchVar = 9;
                        break;
                    }
                }
                mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                break;
            case 8: /* curveBall: press A */
                mg->_1D7C[charIdx].newButtonInput |= INPUT_BUTTON_A;
                mg->_1D7C[charIdx].buttonInput |= INPUT_BUTTON_A;
                mg->wallBallAISwitchVar = 9;
                break;
            }
        }
    }
}

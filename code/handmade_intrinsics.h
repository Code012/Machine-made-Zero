/*  date = June 28th 2026 11:55 AM  */

#if !defined(HANDMADE_INTRINSICS_H)

//
// TODO(casey): Convert all of these to platform-efficient versions
// and remove math.h
//
#include "math.h"


inline s32
RoundReal32ToInt32(f32 Number)
{
    s32 Result = (s32)(Number + 0.5f);
    return (Result);
}
inline u32
RoundReal32ToUInt32(f32 Number)
{
    u32 Result = (u32)(Number + 0.5f);
    return (Result);
}
inline s32
FloorReal32ToInt32(f32 Number)
{
    s32 Result = (s32)floorf(Number);
    return (Result);
}
inline s32
TruncateReal32ToInt32(f32 Number)
{
    s32 Result = (s32)Number;
    return (Result);
}
inline f32 
Sin(f32 Angle)
{
	f32 Result = sinf(Angle);
	return Result;
}
inline f32 
Cos(f32 Angle)
{
	f32 Result = cosf(Angle);
	return Result;
}
inline f32 
Atan2(f32 X, f32 Y)
{
	f32 Result = atan2(X, Y);
	return Result;
}
#define HANDMADE_INTRINSICS_H
#endif
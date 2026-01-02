#pragma once
#ifdef ENGINE_EXPORTS
#define DRAMA_API __declspec(dllexport)
#else
#define DRAMA_API __declspec(dllimport)
#endif

#pragma once
#if defined(ENGINE_STATIC)
#define DRAMA_API
#elif defined(ENGINE_EXPORTS)
#define DRAMA_API __declspec(dllexport)
#else
#define DRAMA_API __declspec(dllimport)
#endif

#pragma once
#if defined(GRAPHICSCORE_STATIC)
#define GRAPHICSCORE_API
#elif defined(GRAPHICSCORE_EXPORTS)
#define GRAPHICSCORE_API __declspec(dllexport)
#else
#define GRAPHICSCORE_API __declspec(dllimport)
#endif

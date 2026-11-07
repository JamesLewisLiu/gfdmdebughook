#pragma once
// The vendored host configuration disables default allocators. panel.cpp supplies
// DLL-owned malloc/free callbacks before creating the independent context.
